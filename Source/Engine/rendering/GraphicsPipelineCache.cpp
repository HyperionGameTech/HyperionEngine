/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <rendering/GraphicsPipelineCache.hpp>
#include <rendering/RenderableAttributes.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/RenderCommand.hpp>
#include <rendering/GraphicsPipeline.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/RenderResult.hpp>
#include <rendering/RenderMemory.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/Shader.hpp>
#include <rendering/ShaderInstance.hpp>

// For Shader
#include <rendering/util/ShaderCompiler.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <Core/threading/Threads.hpp>
#include <Core/threading/Task.hpp>

#include <Core/containers/SparsePagedArray.hpp>

#include <Core/profiling/PerformanceClock.hpp>

#include <Core/utilities/DeferredScope.hpp>

namespace Hyperion {

// #define HYP_GRAPHICS_PIPELINE_TIMING_DEBUG 1

// discard a graphics pipeline that hasn't been used after this number of frames
static constexpr uint32 GraphicsPipelineDiscardFrames = 100;

#pragma region CachedPipelinesMap

class CachedPipelinesMap : public SparsePagedArray<GraphicsPipelineRef, 1024, RenderAllocator>
{
public:
    using Base = SparsePagedArray<GraphicsPipelineRef, 1024, RenderAllocator>;
    using RefCountMap = SparsePagedArray<int, 1024, RenderAllocator>;

    using Map = TMap<PSOCacheKey, Array<GraphicsPipelineRef*, InlineAllocator<1, RenderAllocator>>, RenderAllocator>;
    using ReverseMap = TMap<size_t, PSOCacheKey, RenderAllocator>;

    CachedPipelinesMap()
        : Base()
    {
        cleanupIterator = End();
    }

    void Clear()
    {
        Base::Clear();

        idGenerator.Reset();

        refCountMap.Clear();

        attrMap.Clear();
        reverseAttrMap.Clear();
    }

    void Add(const PSOCacheKey& key, size_t index)
    {
        Assert(Base::HasIndex(index));

        GraphicsPipelineRef* graphicsPipelinePtr = &Base::Get(index);
        Assert(graphicsPipelinePtr != nullptr);

        attrMap[key].PushBack(graphicsPipelinePtr);
        reverseAttrMap[index] = key;
    }

    void Remove(size_t index, bool removeFromAttrMap = true)
    {
        Assert(Base::HasIndex(index));

        GraphicsPipelineRef* graphicsPipelinePtr = &Base::Get(index);

        auto reverseAttrMapIt = reverseAttrMap.Find(index);
        Assert(reverseAttrMapIt != reverseAttrMap.End());

        auto attrMapIt = attrMap.Find(reverseAttrMapIt->second);
        Assert(attrMapIt != attrMap.End());

        // Remove the graphics pipeline from the attribute map
        auto& pipelines = attrMapIt->second;

        auto it = pipelines.Find(graphicsPipelinePtr);
        Assert(it != pipelines.end(), "Graphics pipeline not found in attribute map!");
        pipelines.Erase(it);

        if (pipelines.Empty() && removeFromAttrMap)
        {
            // If there are no pipelines left for this attribute set, remove the entry
            attrMap.Erase(attrMapIt);
        }

        reverseAttrMap.Erase(reverseAttrMapIt);

        EnqueueDeletion(std::move(*graphicsPipelinePtr));
    }

    GraphicsPipelineCacheHandle Alloc(size_t& outIndex)
    {
        outIndex = idGenerator.Next();

        Assert(!Base::HasIndex(outIndex));

        refCountMap.Set(outIndex, 0);

        return GraphicsPipelineCacheHandle(&*Base::Set(outIndex, GraphicsPipelineRef::Null()));
    }

    void RemoveSlotIfUnused(size_t index)
    {
        if (Base::HasIndex(index))
        {
            const int refCount = refCountMap.Get(index);

            if (refCount <= 0)
            {
                GraphicsPipelineRef& graphicsPipeline = Base::Get(index);

                if (graphicsPipeline != nullptr)
                {
                    Remove(index);
                }

                // all pointers should now be invalidated.
                Base::EraseAt(index, /* freeMemory */ true);

                // allow this slot to be reused.
                idGenerator.ReleaseId(index);
            }
        }
    }

    Span<GraphicsPipelineRef* const> Find(const PSOCacheKey& key) const
    {
        auto attrMapIt = attrMap.Find(key);

        if (attrMapIt == attrMap.End())
        {
            return {};
        }

        return attrMapIt->second;
    }

    size_t IndexOf(typename Base::ConstIterator iter) const
    {
        return Base::IndexOf(iter);
    }

    size_t IndexOf(const GraphicsPipelineRef* graphicsPipelinePtr) const
    {
        if (!graphicsPipelinePtr)
        {
            return SIZE_MAX;
        }

        // <page size> * sizeof(GraphicsPipelineRef)
        constexpr size_t pageStorageSizeBytes = (1u << Base::PageSizeBits) * sizeof(GraphicsPipelineRef);

        //  - the underlying reference may be null if it has been destroyed,
        //    but the pointer itself is still valid as long as the cache handle exists.
        //  - therefore, we need to check if the pointer is within the bounds of any of the pages and calculate
        //    the index based on the page's storage address.
        for (Bitset::BitIndex pageIdx : Base::m_validPages)
        {
            typename Base::Page* page = Base::m_pages[pageIdx];
            AssertDebug(page != nullptr);

            if (UIntPtr(graphicsPipelinePtr) < UIntPtr(&page->storage) || UIntPtr(graphicsPipelinePtr) >= UIntPtr(&page->storage) + pageStorageSizeBytes)
            {
                continue; // pointer not in this page
            }

            // calculate the index of the graphics pipeline, using the offset relative to the page's storage address
            // to get the index within the page.
            // then, we add the page index multiplied by the page size to get the absolute index in the SparsePagedArray.
            return (pageIdx << Base::PageSizeBits) + ((UIntPtr(graphicsPipelinePtr) - UIntPtr(&page->storage)) / sizeof(GraphicsPipelineRef));
        }

        return SIZE_MAX;
    }

    IdGenerator idGenerator;

    RefCountMap refCountMap;

    Map attrMap;
    ReverseMap reverseAttrMap;

    Iterator cleanupIterator;
};

#pragma endregion CachedPipelinesMap

#pragma region GraphicsPipelineCacheHandle

void GraphicsPipelineCacheHandle::UpdateRefCount(GraphicsPipelineCacheHandle& cacheHandle, int delta, bool lock)
{
    AssertDebug(cacheHandle.m_ptr != nullptr);

    CachedPipelinesMap* cachedPipelines = RI.graphicsPipelineCache->m_cachedPipelines;
    AssertDebug(cachedPipelines != nullptr);

    ValueStorage<TSharedLock<SharedMutex>> lockStorage {};

    if (lock)
    {
        lockStorage.Construct(RI.graphicsPipelineCache->m_mutex);
    }

    const size_t index = RI.graphicsPipelineCache->m_cachedPipelines->IndexOf(cacheHandle.m_ptr);
    AssertDebug(index != size_t(-1));

    int& refCount = cachedPipelines->refCountMap.Get(index);
    refCount += delta;

    if (lock)
    {
        lockStorage.Destruct();
    }
}

GraphicsPipelineCacheHandle::GraphicsPipelineCacheHandle(GraphicsPipelineRef* graphicsPipelinePtr)
{
    m_ptr = graphicsPipelinePtr;

    if (m_ptr)
    {
        // created within lock so no need to lock it.
        UpdateRefCount(*this, 1, /* lock */ false);
    }
}

GraphicsPipelineCacheHandle::GraphicsPipelineCacheHandle(const GraphicsPipelineCacheHandle& other)
    : m_ptr(other.m_ptr)
{
    if (m_ptr)
    {
        UpdateRefCount(*this, 1, /* lock */ true);
    }
}

GraphicsPipelineCacheHandle& GraphicsPipelineCacheHandle::operator=(const GraphicsPipelineCacheHandle& other)
{
    if (m_ptr == other.m_ptr)
    {
        // no change, nothing to do
        return *this;
    }

    if (m_ptr)
    {
        UpdateRefCount(*this, -1, /* lock */ true);
    }

    m_ptr = other.m_ptr;

    if (m_ptr)
    {
        UpdateRefCount(*this, 1, /* lock */ true);
    }

    return *this;
}

GraphicsPipelineCacheHandle::~GraphicsPipelineCacheHandle()
{
    if (m_ptr)
    {
        UpdateRefCount(*this, -1, /* lock */ true);
    }
}

#pragma endregion GraphicsPipelineCacheHandle

#pragma region GraphicsPipelineCache

GraphicsPipelineCache::GraphicsPipelineCache()
    : m_cachedPipelines(new CachedPipelinesMap())
{
}

GraphicsPipelineCache::~GraphicsPipelineCache()
{
    for (GraphicsPipelineRef& pipeline : *m_cachedPipelines)
    {
        EnqueueDeletion(std::move(pipeline));
    }

    m_cachedPipelines->Clear();

    Assert(m_cachedPipelines->Empty(), "Graphics pipeline cache not empty!");
    delete m_cachedPipelines;
}

void GraphicsPipelineCache::GetOrCreate(
    RenderableAttributeSet& inOutAttributes,
    const FramebufferDesc& framebufferDesc,
    GraphicsPipelineCacheHandle& outCacheHandle)
{
    HYP_SCOPE;

    GraphicsPipelineCacheHandle cacheHandle = FindGraphicsPipeline(inOutAttributes, framebufferDesc);

    if (cacheHandle.IsAlive())
    {
        (*cacheHandle)->lastFrame = GetFrameCounter();

        outCacheHandle = std::move(cacheHandle);
        return;
    }

    Assert(framebufferDesc.numAttachments > 0,
        "Cannot create a graphics pipeline with no render target descriptor or 0 attachments!");

    ShaderInstanceRef shader = RI.shaderManager->GetOrCreate(
        inOutAttributes.GetMaterialAttributes().shaderName,
        inOutAttributes.GetMaterialAttributes().shaderProperties,
        inOutAttributes.GetMeshAttributes().inputLayout);

    if (!shader.IsValid())
    {
        outCacheHandle = {};
        return;
    }

    // // Shader may have additional static properties.
    // // See: ShaderBundle, staticProperties and its usage in ShaderCompiler.cpp.
    // inOutAttributes.GetMaterialAttributes().shaderProperties = shader->GetShader()->properties;

    size_t slot = SIZE_MAX;

    // Create pipeline
    GraphicsPipelineRef graphicsPipeline = RI.MakeGraphicsPipeline(
        shader,
        framebufferDesc,
        inOutAttributes);

    TUniqueLock guard(m_mutex);

    ShaderInstance* shaderInstance = graphicsPipeline->GetShader();

#if HYP_DEBUG_MODE
    String shaderString = "\tProperties: " + shaderInstance->GetShader()->properties.GetDebugString();
    shaderString += "\n\tVertex attributes: " + String::ToString(shaderInstance->GetShader()->inputLayout.mask);

    HYP_LOG(Rendering, Verbose, "Creating graphics pipeline {} (debug name: {}) on render thread, Shader details:\n{}",
        graphicsPipeline->Id(), graphicsPipeline->GetDebugName(),
        shaderString);
#endif

    if (!CheckResult(graphicsPipeline->Create()))
    {
        return;
    }

    // set initial lastFrame index so we don't delete it right away when cleaning up after the frame.
    graphicsPipeline->lastFrame = GetFrameCounter();

    cacheHandle = m_cachedPipelines->Alloc(slot);

    Assert(cacheHandle.m_ptr != nullptr && slot != SIZE_MAX);

    // set new allocated slot to the graphics pipeline we just created
    *cacheHandle.m_ptr = std::move(graphicsPipeline);

    outCacheHandle = std::move(cacheHandle);

    // Add to cache
    Assert(slot != SIZE_MAX);

#if HYP_GRAPHICS_PIPELINE_TIMING_DEBUG
    HYP_LOG(Rendering, Verbose, "Adding graphics pipeline {} (debug name: {}) to cache with hash: {}", graphicsPipeline->Id(), graphicsPipeline->GetDebugName(), inOutAttributes.GetHashCode().Value());
#endif
    // cache it now that it's been created so it can be reused
    PSOCacheKey key { inOutAttributes, framebufferDesc };
    m_cachedPipelines->Add(key, slot);

    return;
}

GraphicsPipelineCacheHandle GraphicsPipelineCache::FindGraphicsPipeline(
    const RenderableAttributeSet& attributes,
    const FramebufferDesc& framebufferDesc)
{
    HYP_SCOPE;

#if HYP_GRAPHICS_PIPELINE_TIMING_DEBUG
    PerformanceClock clock;
    clock.Start();
#endif

    TSharedLock guard(m_mutex);

    const PSOCacheKey key { attributes, framebufferDesc };
    Span<GraphicsPipelineRef* const> pipelines = m_cachedPipelines->Find(key);

    if (!pipelines)
    {
#if HYP_GRAPHICS_PIPELINE_TIMING_DEBUG
        HYP_LOG(Rendering, Warning, "GraphicsPipelineCache cache miss ({}) ({} ms)", attributes.GetHashCode().Value(), clock.ElapsedMs());
#endif

        return {};
    }

    for (GraphicsPipelineRef* const pPipeline : pipelines)
    {
        Assert(pPipeline != nullptr);

        if ((*pPipeline)->MatchesSignature(attributes, framebufferDesc))
        {
#if HYP_GRAPHICS_PIPELINE_TIMING_DEBUG
            HYP_LOG(Rendering, Verbose, "GraphicsPipelineCache cache hit ({}) ({} ms)", attributes.GetHashCode().Value(), clock.ElapsedMs());
#endif

            return GraphicsPipelineCacheHandle(pPipeline);
        }
    }

#if HYP_GRAPHICS_PIPELINE_TIMING_DEBUG
    HYP_LOG(Rendering, Warning, "GraphicsPipelineCache cache miss ({}) ({} ms)", attributes.GetHashCode().Value(), clock.ElapsedMs());
#endif

    return {};
}

void GraphicsPipelineCache::ExpirePipelinesForShader(const Shader* shader)
{
    if (!shader)
    {
        return;
    }

    TUniqueLock guard(m_mutex);

    // find all pipelines that use this shader and remove them, so they will be recreated with the new shader instance when requested again.
    for (auto it = m_cachedPipelines->attrMap.Begin(); it != m_cachedPipelines->attrMap.End();)
    {
        const PSOCacheKey& key = it->first;

        if (key.shaderName == shader->baseName && key.shaderProperties == shader->properties)
        {
            // @NOTE intentionally making a copy of the array.
            // CachedPipelinesMap::Remove will modify the original array by removing pipelines one by one, so we need to avoid modifying the array while iterating over it.
            auto pipelines = it->second;

            for (GraphicsPipelineRef* const pPipeline : pipelines)
            {
                Assert(pPipeline != nullptr);

                const size_t index = m_cachedPipelines->IndexOf(pPipeline);
                Assert(index != SIZE_MAX);

                m_cachedPipelines->Remove(index, /* removeFromAttrMap */ false);
            }

            // if we removed all cached pipelines for these attrs then remove from attrMap
            if (it->second.Empty())
            {
                it = m_cachedPipelines->attrMap.Erase(it);
                continue;
            }
        }

        ++it;
    }
}

int GraphicsPipelineCache::RunCleanupCycle(int maxIter)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    const uint32 currFrame = GetFrameCounter();

    TUniqueLock guard(m_mutex);

    m_cachedPipelines->cleanupIterator = typename CachedPipelinesMap::Iterator(
        m_cachedPipelines,
        m_cachedPipelines->cleanupIterator.page,
        m_cachedPipelines->cleanupIterator.elem);

    const typename CachedPipelinesMap::Iterator startIterator = m_cachedPipelines->cleanupIterator; // the iterator we started at - use it to check that we don't do duplicate checks

    int numCycles = 0;

    for (; numCycles < maxIter; ++numCycles)
    {
        // Loop around to the beginning of the container when the end is reached.
        if (m_cachedPipelines->cleanupIterator == m_cachedPipelines->End())
        {
            m_cachedPipelines->cleanupIterator = m_cachedPipelines->Begin();

            if (m_cachedPipelines->cleanupIterator == m_cachedPipelines->End())
            {
                break;
            }
        }

        GraphicsPipelineRef& graphicsPipeline = *m_cachedPipelines->cleanupIterator;

        if (!graphicsPipeline)
        {
            // empty slot, remove if unused
            const size_t index = m_cachedPipelines->IndexOf(m_cachedPipelines->cleanupIterator);
            Assert(index != SIZE_MAX);

            // skip to next iterator before potentially removing the current slot
            ++m_cachedPipelines->cleanupIterator;

            m_cachedPipelines->RemoveSlotIfUnused(index);

            continue;
        }

        // signed as graphics pipelines that haven't been used yet have -1 as their lastFrame value
        const int64 frameDiff = int64(currFrame) - int64(graphicsPipeline->lastFrame);

        if (frameDiff >= GraphicsPipelineDiscardFrames)
        {
#if HYP_DEBUG_MODE
            HYP_LOG(Rendering, Verbose, "Removing graphics pipeline {} (debug name: {}) from cache as it has not been used in {} frames",
                graphicsPipeline->Id(),
                graphicsPipeline->GetDebugName(),
                frameDiff);
#endif

            const size_t index = m_cachedPipelines->IndexOf(m_cachedPipelines->cleanupIterator);
            Assert(index != SIZE_MAX);

            m_cachedPipelines->Remove(index, /* removeFromAttrMap */ true);
        }

        ++m_cachedPipelines->cleanupIterator;

        if (m_cachedPipelines->cleanupIterator == startIterator)
        {
            // we checked everything
            break;
        }
    }

    return numCycles;
}

#pragma endregion GraphicsPipelineCache

} // namespace Hyperion
