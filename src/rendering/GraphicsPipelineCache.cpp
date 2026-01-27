/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

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

// For CompiledShader
#include <rendering/util/ShaderCompiler.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <core/threading/Threads.hpp>
#include <core/threading/Task.hpp>

#include <core/containers/SparsePagedArray.hpp>

#include <core/profiling/PerformanceClock.hpp>

#include <core/utilities/DeferredScope.hpp>

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

    using Map = HashMap<PSOCacheKey, Array<GraphicsPipelineRef*, InlineAllocator<1, RenderAllocator>>, NodeAllocator<RenderAllocator>>;
    using ReverseMap = HashMap<SizeType, PSOCacheKey, NodeAllocator<RenderAllocator>>;

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

    void Add(const PSOCacheKey& key, SizeType index)
    {
        Assert(Base::HasIndex(index));

        GraphicsPipelineRef* graphicsPipelinePtr = &Base::Get(index);
        Assert(graphicsPipelinePtr != nullptr);

        attrMap[key].PushBack(graphicsPipelinePtr);
        reverseAttrMap[index] = key;
    }

    void Remove(SizeType index)
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

        if (pipelines.Empty())
        {
            // If there are no pipelines left for this attribute set, remove the entry
            attrMap.Erase(attrMapIt);
        }

        reverseAttrMap.Erase(reverseAttrMapIt);

        SafeDelete(std::move(*graphicsPipelinePtr));
    }

    GraphicsPipelineCacheHandle Alloc(SizeType& outIndex)
    {
        outIndex = idGenerator.Next();

        Assert(!Base::HasIndex(outIndex));

        refCountMap.Set(outIndex, 0);

        return GraphicsPipelineCacheHandle(&*Base::Set(outIndex, GraphicsPipelineRef::Null()));
    }

    void RemoveSlotIfUnused(SizeType index)
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

    SizeType IndexOf(typename Base::ConstIterator iter) const
    {
        return Base::IndexOf(iter);
    }

    SizeType IndexOf(const GraphicsPipelineRef* graphicsPipelinePtr) const
    {
        if (!graphicsPipelinePtr)
        {
            return SizeType(-1);
        }

        // <page size> * sizeof(GraphicsPipelineRef)
        constexpr SizeType pageStorageSizeBytes = (1u << Base::PageSizeBits) * sizeof(GraphicsPipelineRef);

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

        return SizeType(-1);
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

    CachedPipelinesMap* cachedPipelines = g_renderInterface->graphicsPipelineCache->m_cachedPipelines;
    AssertDebug(cachedPipelines != nullptr);

    ValueStorage<TSharedLock<SharedMutex>> lockStorage {};

    if (lock)
    {
        lockStorage.Construct(g_renderInterface->graphicsPipelineCache->m_mutex);
    }

    const SizeType index = g_renderInterface->graphicsPipelineCache->m_cachedPipelines->IndexOf(cacheHandle.m_ptr);
    AssertDebug(index != SizeType(-1));

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
        SafeDelete(std::move(pipeline));
    }

    m_cachedPipelines->Clear();

    Assert(m_cachedPipelines->Empty(), "Graphics pipeline cache not empty!");
    delete m_cachedPipelines;
}

void GraphicsPipelineCache::GetOrCreate(
    const RenderableAttributeSet& attributes,
    const RenderTargetDesc& renderTargetDesc,
    GraphicsPipelineCacheHandle& outCacheHandle)
{
    HYP_SCOPE;

    GraphicsPipelineCacheHandle cacheHandle = FindGraphicsPipeline(attributes, renderTargetDesc);

    if (cacheHandle.IsAlive())
    {
        (*cacheHandle)->lastFrame = GetFrameCounter();

        outCacheHandle = std::move(cacheHandle);
        return;
    }

    Proc<void(GraphicsPipeline*, SizeType)> newCallback([this, key = PSOCacheKey(attributes, renderTargetDesc)](GraphicsPipeline* graphicsPipeline, SizeType slot)
        {
            TUniqueLock guard(m_mutex);

#if HYP_GRAPHICS_PIPELINE_TIMING_DEBUG
            HYP_LOG(Rendering, Debug, "Adding graphics pipeline {} (debug name: {}) to cache with hash: {}", graphicsPipeline->Id(), graphicsPipeline->GetDebugName(), attributes.GetHashCode().Value());
#endif
            // cache it now that it's been created so it can be reused
            m_cachedPipelines->Add(key, slot);
        });

    Assert(renderTargetDesc.numAttachments > 0,
        "Cannot create a graphics pipeline with no render target descriptor or 0 attachments!");

    ShaderRef shader = g_shaderManager->GetOrCreate(
        attributes.GetMaterialAttributes().shaderName,
        attributes.GetMaterialAttributes().shaderProperties,
        attributes.GetMeshAttributes().vertexAttributes);

    Assert(shader.IsValid());

    GraphicsPipelineRef graphicsPipeline = g_renderInterface->MakeGraphicsPipeline(
        shader,
        renderTargetDesc,
        attributes);

    // sanity check: newly created pipeline must match or caching will fail.
    AssertDebug(graphicsPipeline->MatchesSignature(attributes, renderTargetDesc));

    SizeType slot = SizeType(-1);

    cacheHandle = m_cachedPipelines->Alloc(slot);
    Assert(cacheHandle.m_ptr != nullptr && slot != SizeType(-1));

    // set new allocated slot to the graphics pipeline we just created
    *cacheHandle.m_ptr = std::move(graphicsPipeline);

    struct CreateGraphicsPipelineAndAddToCache : RenderCommand
    {
        GraphicsPipeline* graphicsPipeline;
        SizeType slot;
        Proc<void(GraphicsPipeline*, SizeType)> callback;

        CreateGraphicsPipelineAndAddToCache(
            GraphicsPipeline* graphicsPipeline,
            SizeType slot,
            Proc<void(GraphicsPipeline*, SizeType)>&& callback)
            : graphicsPipeline(graphicsPipeline),
              slot(slot),
              callback(std::move(callback))
        {
            Assert(graphicsPipeline != nullptr && slot != SizeType(-1));
        }

        virtual ~CreateGraphicsPipelineAndAddToCache() override = default;

        virtual RendererResult operator()() override
        {
            CheckResultOrReturn(graphicsPipeline->Create());

            if (callback.IsValid())
            {
                // set initial lastFrame index so we don't delete it right away when cleaning up after the frame.
                graphicsPipeline->lastFrame = GetFrameCounter();

                callback(graphicsPipeline, slot);
            }

            return RendererResult {};
        }
    };

    PUSH_RENDER_COMMAND(CreateGraphicsPipelineAndAddToCache, *cacheHandle.m_ptr, slot, std::move(newCallback));

    outCacheHandle = std::move(cacheHandle);

    return;
}

GraphicsPipelineCacheHandle GraphicsPipelineCache::FindGraphicsPipeline(
    const RenderableAttributeSet& attributes,
    const RenderTargetDesc& renderTargetDesc)
{
    HYP_SCOPE;

#if HYP_GRAPHICS_PIPELINE_TIMING_DEBUG
    PerformanceClock clock;
    clock.Start();
#endif

    TSharedLock guard(m_mutex);

    const PSOCacheKey key { attributes, renderTargetDesc };
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

        if ((*pPipeline)->MatchesSignature(attributes, renderTargetDesc))
        {
#if HYP_GRAPHICS_PIPELINE_TIMING_DEBUG
            HYP_LOG(Rendering, Info, "GraphicsPipelineCache cache hit ({}) ({} ms)", attributes.GetHashCode().Value(), clock.ElapsedMs());
#endif

            return GraphicsPipelineCacheHandle(pPipeline);
        }
    }

#if HYP_GRAPHICS_PIPELINE_TIMING_DEBUG
    HYP_LOG(Rendering, Warning, "GraphicsPipelineCache cache miss ({}) ({} ms)", attributes.GetHashCode().Value(), clock.ElapsedMs());
#endif

    return {};
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
            const SizeType index = m_cachedPipelines->IndexOf(m_cachedPipelines->cleanupIterator);
            Assert(index != SizeType(-1));

            // skip to next iterator before potentially removing the current slot
            ++m_cachedPipelines->cleanupIterator;

            m_cachedPipelines->RemoveSlotIfUnused(index);

            continue;
        }

        // signed as graphics pipelines that haven't been used yet have -1 as their lastFrame value
        const int64 frameDiff = int64(currFrame) - int64(graphicsPipeline->lastFrame);

        if (frameDiff >= GraphicsPipelineDiscardFrames)
        {
#ifdef HYP_DEBUG_MODE
            HYP_LOG(Rendering, Debug, "Removing graphics pipeline {} (debug name: {}) from cache as it has not been used in {} frames",
                graphicsPipeline->Id(),
                graphicsPipeline->GetDebugName(),
                frameDiff);
#endif

            const SizeType index = m_cachedPipelines->IndexOf(m_cachedPipelines->cleanupIterator);
            Assert(index != SizeType(-1));

            m_cachedPipelines->Remove(index);
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
