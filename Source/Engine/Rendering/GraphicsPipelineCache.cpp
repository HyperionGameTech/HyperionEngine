/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/GraphicsPipelineCache.hpp>
#include <Rendering/RenderableAttributes.hpp>
#include <Rendering/RenderInterface.hpp>
#include <Rendering/RenderCommand.hpp>
#include <Rendering/GraphicsPipeline.hpp>
#include <Rendering/DescriptorSet.hpp>
#include <Rendering/RenderResult.hpp>
#include <Rendering/RenderMemory.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/Shader.hpp>
#include <Rendering/ShaderInstance.hpp>

// For Shader
#include <Rendering/Util/ShaderCompiler.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Core/Threading/Threads.hpp>
#include <Core/Threading/Task.hpp>

#include <Core/Containers/SparsePagedArray.hpp>

#include <Core/Profiling/PerformanceClock.hpp>

#include <Core/Utilities/DeferredScope.hpp>

namespace Hyperion {

// #define HYP_GRAPHICS_PIPELINE_TIMING_DEBUG 1

// discard a graphics pipeline that hasn't been used after this number of frames
static constexpr uint32 GraphicsPipelineDiscardFrames = 100;

#pragma region CachedPipelinesMap

class CachedPipelinesMap : public SparsePagedArray<GraphicsPipelineRef, 1024, RenderAllocator>
{
public:
    using Base = SparsePagedArray<GraphicsPipelineRef, 1024, RenderAllocator>;

    using RefCountMap = Map<size_t, uint32, RenderAllocator>;

    using CacheKeyToPipelines = Map<PSOCacheKey, FatArray<GraphicsPipelineRef*, InlineAllocator<1, RenderAllocator>>, RenderAllocator>;
    using IndexToCacheKey = Map<size_t, PSOCacheKey, RenderAllocator>;

    CachedPipelinesMap()
        : Base()
    {
        cleanupIterator = End();
    }

    void Clear()
    {
        Base::Clear();

        indexAllocator.Reset();

        refCountMap.Clear();

        attrMap.Clear();
        reverseAttrMap.Clear();
    }

    void Add(const PSOCacheKey& key, size_t index)
    {
        Assert(Base::HasIndex(index));

        GraphicsPipelineRef* pPipelineRef = &Base::Get(index);
        Assert(pPipelineRef != nullptr);

        attrMap[key].PushBack(pPipelineRef);
        reverseAttrMap[index] = key;
    }

    void Remove(size_t index, bool removeFromAttrMap = true)
    {
        Assert(Base::HasIndex(index));

        GraphicsPipelineRef* pPipelineRef = &Base::Get(index);

        auto reverseAttrMapIt = reverseAttrMap.Find(index);
        Assert(reverseAttrMapIt != reverseAttrMap.End());

        auto attrMapIt = attrMap.Find(reverseAttrMapIt->second);
        Assert(attrMapIt != attrMap.End());

        // Remove the graphics pipeline from the attribute map
        auto& pipelines = attrMapIt->second;

        auto it = pipelines.Find(pPipelineRef);
        Assert(it != pipelines.end(), "Graphics pipeline not found in attribute map!");
        pipelines.Erase(it);

        if (pipelines.Empty() && removeFromAttrMap)
        {
            // If there are no pipelines left for this attribute set, remove the entry
            attrMap.Erase(attrMapIt);
        }

        reverseAttrMap.Erase(reverseAttrMapIt);

        EnqueueDeletion(std::move(*pPipelineRef));
    }

    bool Remove(GraphicsPipelineRef* pPipelineRef, bool removeFromAttrMap = true)
    {
        if (!pPipelineRef)
        {
            return false;
        }

        for (auto attrMapIt = attrMap.Begin(); attrMapIt != attrMap.End(); ++attrMapIt)
        {
            auto& pipelines = attrMapIt->second;

            auto it = pipelines.Find(pPipelineRef);

            if (it == pipelines.End())
            {
                continue;
            }

            const PSOCacheKey key = attrMapIt->first;

            pipelines.Erase(it);

            if (pipelines.Empty() && removeFromAttrMap)
            {
                attrMap.Erase(attrMapIt);
            }

            for (auto reverseIt = reverseAttrMap.Begin(); reverseIt != reverseAttrMap.End(); ++reverseIt)
            {
                if (reverseIt->second == key && Base::HasIndex(reverseIt->first))
                {
                    GraphicsPipelineRef* pCandidate = &Base::Get(reverseIt->first);

                    if (pCandidate == pPipelineRef)
                    {
                        reverseAttrMap.Erase(reverseIt);

                        break;
                    }
                }
            }

            EnqueueDeletion(std::move(*pPipelineRef));

            return true;
        }

        return false;
    }

    GraphicsPipelineCacheHandle Alloc(size_t& outIndex)
    {
        outIndex = static_cast<size_t>(indexAllocator.Allocate());

        Assert(!Base::HasIndex(outIndex));

        refCountMap[outIndex] = 0;

        return GraphicsPipelineCacheHandle(&*Base::Set(outIndex, GraphicsPipelineRef::Null()));
    }

    void RemoveSlotIfUnused(size_t index)
    {
        if (Base::HasIndex(index))
        {
            const uint32 refCount = refCountMap[index];

            if (refCount == 0)
            {
                GraphicsPipelineRef& graphicsPipeline = Base::Get(index);

                if (graphicsPipeline != nullptr)
                {
                    Remove(index);
                }

                // all pointers should now be invalidated.
                Base::EraseAt(index, /* freeMemory */ true);

                // allow this slot to be reused.
                indexAllocator.Free(index);
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

    AtomicIndexAllocator indexAllocator;

    RefCountMap refCountMap;

    CacheKeyToPipelines attrMap;
    IndexToCacheKey reverseAttrMap;

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

    uint32& refCount = cachedPipelines->refCountMap[index];

    const int64 newRefCount = static_cast<int64>(refCount) + delta;
    AssertDebug(newRefCount >= 0);

    refCount = static_cast<uint32>(newRefCount);

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
    uint8 stencilWriteMask,
    uint8 stencilCompareMask,
    GraphicsPipelineCacheHandle& outCacheHandle)
{
    HYP_SCOPE;

    GraphicsPipelineCacheHandle cacheHandle = FindGraphicsPipeline(
        inOutAttributes,
        framebufferDesc,
        stencilWriteMask,
        stencilCompareMask);

    if (cacheHandle.IsAlive())
    {
        (*cacheHandle)->lastFrame = GetFrameCounter();

        Assert(!(*cacheHandle)->GetShader()->GetShader()->expired);

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
        HYP_LOG(Rendering, Warning, "Invalid shader returned from ShaderManager for {}",
                inOutAttributes.GetMaterialAttributes().shaderName);

        outCacheHandle = {};
        return;
    }

    Assert(!shader->GetShader()->expired);

    // // Shader may have additional static properties.
    // // See: ShaderBundle, staticProperties and its usage in ShaderCompiler.cpp.
    // inOutAttributes.GetMaterialAttributes().shaderProperties = shader->GetShader()->properties;

    size_t slot = SIZE_MAX;

    // Create pipeline
    GraphicsPipelineRef graphicsPipeline = RI.MakeGraphicsPipeline(
        shader,
        framebufferDesc,
        inOutAttributes,
        stencilWriteMask,
        stencilCompareMask);

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
    const FramebufferDesc& framebufferDesc,
    uint8 stencilWriteMask,
    uint8 stencilCompareMask)
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

        if ((*pPipeline)->MatchesSignature(attributes, framebufferDesc, stencilWriteMask, stencilCompareMask))
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
    AssertOnThread(g_renderThread);

    if (!shader)
    {
        return;
    }

    TUniqueLock guard(m_mutex);

    // find all pipelines that use this shader and remove them,
    // so they will be recreated with the new shader instance when requested again.
    for (auto it = m_cachedPipelines->attrMap.Begin(); it != m_cachedPipelines->attrMap.End();)
    {
        const PSOCacheKey& key = it->first;

        if (key.shaderName == shader->baseName && key.shaderProperties == shader->properties)
        {
            auto& pipelines = it->second;

            for (GraphicsPipelineRef* const pPipeline : pipelines)
            {
                Assert(pPipeline != nullptr);

                // Unset so we don't trip over a destroyed pipeline!
                if (RI.state.boundGraphicsPipeline == *pPipeline)
                {
                    RI.state.boundGraphicsPipeline = nullptr;
                }

                m_cachedPipelines->Remove(pPipeline, /* removeFromAttrMap */ false);
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
