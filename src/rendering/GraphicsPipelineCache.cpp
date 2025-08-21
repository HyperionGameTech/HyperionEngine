/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <rendering/GraphicsPipelineCache.hpp>
#include <rendering/RenderableAttributes.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/RenderCommand.hpp>
#include <rendering/RenderGraphicsPipeline.hpp>
#include <rendering/RenderResult.hpp>
#include <rendering/RenderGlobalState.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <core/threading/Threads.hpp>
#include <core/threading/Task.hpp>

#include <core/containers/SparsePagedArray.hpp>

#include <core/profiling/PerformanceClock.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

// For CompiledShader
#include <rendering/shader_compiler/ShaderCompiler.hpp>

#include <engine/EngineGlobals.hpp>

namespace hyperion {

// discard a graphics pipeline that hasn't been used after this number of frames
static constexpr uint32 g_graphicsPipelineDiscardFrames = 32;

#pragma region CachedPipelinesMap

class CachedPipelinesMap : public SparsePagedArray<GraphicsPipelineRef, 1024>
{
public:
    using Base = SparsePagedArray<GraphicsPipelineRef, 1024>;
    using RefCountMap = SparsePagedArray<int, 1024>;

    using AttrMap = HashMap<RenderableAttributeSet, Array<GraphicsPipelineRef*, InlineAllocator<1>>>;
    using ReverseAttrMap = HashMap<uint32, RenderableAttributeSet>;

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

    void Add(const RenderableAttributeSet& renderableAttributes, uint32 slot)
    {
        Assert(Base::HasIndex(slot));

        GraphicsPipelineRef* pGraphicsPipeline = &Base::Get(slot);
        Assert(pGraphicsPipeline != nullptr);

        attrMap[renderableAttributes].PushBack(pGraphicsPipeline);
        reverseAttrMap[slot] = renderableAttributes;
    }

    void Remove(uint32 slot)
    {
        Assert(Base::HasIndex(slot));

        GraphicsPipelineRef* pGraphicsPipeline = &Base::Get(slot);

        auto reverseAttrMapIt = reverseAttrMap.Find(slot);
        Assert(reverseAttrMapIt != reverseAttrMap.End());

        auto attrMapIt = attrMap.Find(reverseAttrMapIt->second);
        Assert(attrMapIt != attrMap.End());

        // Remove the graphics pipeline from the attribute map
        Array<GraphicsPipelineRef*, InlineAllocator<1>>& pipelines = attrMapIt->second;

        auto it = pipelines.Find(pGraphicsPipeline);
        Assert(it != pipelines.end(), "Graphics pipeline not found in attribute map!");
        pipelines.Erase(it);

        if (pipelines.Empty())
        {
            // If there are no pipelines left for this attribute set, remove the entry
            attrMap.Erase(attrMapIt);
        }

        reverseAttrMap.Erase(reverseAttrMapIt);
        
        SafeDelete(std::move(*pGraphicsPipeline));
    }

    GraphicsPipelineCacheHandle Alloc(uint32& outSlot)
    {
        outSlot = idGenerator.Next();

        Assert(!Base::HasIndex(outSlot));

        refCountMap.Set(outSlot, 0);

        return GraphicsPipelineCacheHandle(&*Base::Set(outSlot, GraphicsPipelineRef::Null()));
    }

    void RemoveSlotIfUnused(uint32 slot)
    {
        Assert(slot != ~0u);

        if (Base::HasIndex(slot))
        {
            const int refCount = refCountMap.Get(slot);

            if (refCount <= 0)
            {
                GraphicsPipelineRef& graphicsPipeline = Base::Get(slot);

                if (graphicsPipeline != nullptr)
                {
                    Remove(slot);
                }

                // all pointers should now be invalidated.
                Base::EraseAt(slot, /* freeMemory */ true);

                // allow this slot to be reused.
                idGenerator.ReleaseId(slot);
            }
        }
    }

    Span<GraphicsPipelineRef* const> Find(const RenderableAttributeSet& renderableAttributes) const
    {
        auto attrMapIt = attrMap.Find(renderableAttributes);

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

    SizeType IndexOf(const GraphicsPipelineRef* pGraphicsPipeline) const
    {
        if (!pGraphicsPipeline)
        {
            return SizeType(-1);
        }

        // <page size> * sizeof(GraphicsPipelineRef)
        constexpr SizeType pageStorageSizeBytes = (1u << Base::pageSizeBits) * sizeof(GraphicsPipelineRef);

        //  - the underlying reference may be null if it has been destroyed,
        //    but the pointer itself is still valid as long as the cache handle exists.
        //  - therefore, we need to check if the pointer is within the bounds of any of the pages and calculate
        //    the index based on the page's storage address.
        for (Bitset::BitIndex pageIdx : Base::m_validPages)
        {
            typename Base::Page* page = Base::m_pages[pageIdx];
            AssertDebug(page != nullptr);

            if (uintptr_t(pGraphicsPipeline) < uintptr_t(&page->storage) ||
                uintptr_t(pGraphicsPipeline) >= uintptr_t(&page->storage) + pageStorageSizeBytes)
            {
                continue; // pointer not in this page
            }

            // calculate the index of the graphics pipeline, using the offset relative to the page's storage address
            // to get the index within the page.
            // then, we add the page index multiplied by the page size to get the absolute index in the SparsePagedArray.
            return (pageIdx << Base::pageSizeBits) +
                ((uintptr_t(pGraphicsPipeline) - uintptr_t(&page->storage)) / sizeof(GraphicsPipelineRef));
        }

        return SizeType(-1);
    }

    IdGenerator idGenerator;
    
    RefCountMap refCountMap;

    AttrMap attrMap;
    ReverseAttrMap reverseAttrMap;
    
    Iterator cleanupIterator;
};

#pragma endregion CachedPipelinesMap

#pragma region GraphicsPipelineCacheHandle

void GraphicsPipelineCacheHandle::UpdateRefCount(GraphicsPipelineCacheHandle& cacheHandle, int delta, bool lock)
{
    AssertDebug(cacheHandle.m_pRef != nullptr);

    CachedPipelinesMap* cachedPipelines = g_renderGlobalState->graphicsPipelineCache->m_cachedPipelines;
    AssertDebug(cachedPipelines != nullptr);

    if (lock)
        g_renderGlobalState->graphicsPipelineCache->m_mutex.Lock();

    const SizeType index = g_renderGlobalState->graphicsPipelineCache->m_cachedPipelines->IndexOf(cacheHandle.m_pRef);
    AssertDebug(index != SizeType(-1));

    int& refCount = cachedPipelines->refCountMap.Get(index);
    refCount += delta;

    if (lock)
        g_renderGlobalState->graphicsPipelineCache->m_mutex.Unlock();
}

GraphicsPipelineCacheHandle::GraphicsPipelineCacheHandle(GraphicsPipelineRef* pRef)
{
    m_pRef = pRef;

    if (m_pRef)
    {
        // created within lock so no need to lock it.
        UpdateRefCount(*this, 1, /* lock */ false);
    }
}

GraphicsPipelineCacheHandle::GraphicsPipelineCacheHandle(const GraphicsPipelineCacheHandle& other)
    : m_pRef(other.m_pRef)
{
    if (m_pRef)
    {
        UpdateRefCount(*this, 1, /* lock */ true);
    }
}

GraphicsPipelineCacheHandle& GraphicsPipelineCacheHandle::operator=(const GraphicsPipelineCacheHandle& other)
{
    if (m_pRef == other.m_pRef)
    {
        // no change, nothing to do
        return *this;
    }

    if (m_pRef)
    {
        UpdateRefCount(*this, -1, /* lock */ true);
    }

    m_pRef = other.m_pRef;

    if (m_pRef)
    {
        UpdateRefCount(*this, 1, /* lock */ true);
    }

    return *this;
}

GraphicsPipelineCacheHandle::~GraphicsPipelineCacheHandle()
{
    if (m_pRef)
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

GraphicsPipelineCacheHandle GraphicsPipelineCache::GetOrCreate(
    const ShaderRef& shader,
    const DescriptorTableRef& descriptorTable,
    Span<const FramebufferRef> framebuffers,
    const RenderableAttributeSet& attributes)
{
    HYP_SCOPE;

    if (!shader.IsValid())
    {
        HYP_LOG(Rendering, Error, "Shader is null or invalid!");

        return {};
    }

    const DescriptorTableDeclaration* descriptorTableDecl = nullptr;

    DescriptorTableRef table;

    if (descriptorTable)
    {
        table = descriptorTable;
        descriptorTableDecl = table->GetDeclaration();

        Assert(descriptorTableDecl != nullptr);
    }
    else
    {
        descriptorTableDecl = &shader->GetCompiledShader()->GetDescriptorTableDeclaration();
    }

    GraphicsPipelineCacheHandle cacheHandle = FindGraphicsPipeline(
        shader,
        *descriptorTableDecl,
        framebuffers,
        attributes);

    if (cacheHandle.IsAlive())
    {
        return cacheHandle;
    }

    if (!table)
    {
        table = g_renderBackend->MakeDescriptorTable(descriptorTableDecl);
        if (!table.IsValid())
        {
#ifdef HYP_DEBUG_MODE
            HYP_LOG(Rendering, Error, "Failed to create descriptor table for shader: {}", shader->GetDebugName());
#else
            HYP_LOG(Rendering, Error, "Failed to create descriptor table for shader");
#endif

            return {};
        }

        DeferCreate(table);
    }

    Proc<void(GraphicsPipelineBase* graphicsPipeline, uint32 slot)> newCallback([this, attributes](GraphicsPipelineBase* graphicsPipeline, uint32 slot)
        {
            Mutex::Guard guard(m_mutex);

#ifdef HYP_DEBUG_MODE
            HYP_LOG(Rendering, Debug, "Adding graphics pipeline {} (debug name: {}) to cache with hash: {}", graphicsPipeline->Id(), graphicsPipeline->GetDebugName(), attributes.GetHashCode().Value());
#endif
            // cache it now that it's been created so it can be reused
            m_cachedPipelines->Add(attributes, slot);
        });

    GraphicsPipelineRef graphicsPipeline = g_renderBackend->MakeGraphicsPipeline(
        shader,
        table,
        framebuffers,
        attributes);

    uint32 slot = ~0u;

    cacheHandle = m_cachedPipelines->Alloc(slot);
    Assert(cacheHandle.m_pRef != nullptr && slot != ~0u);

    // set new allocated slot to the graphics pipeline we just created
    *cacheHandle.m_pRef = std::move(graphicsPipeline);

    struct RENDER_COMMAND(CreateGraphicsPipelineAndAddToCache)
        : RenderCommand
    {
        GraphicsPipelineBase* graphicsPipeline;
        uint32 slot;
        Proc<void(GraphicsPipelineBase*, uint32)> callback;

        RENDER_COMMAND(CreateGraphicsPipelineAndAddToCache)(
            GraphicsPipelineBase* graphicsPipeline,
            uint32 slot,
            Proc<void(GraphicsPipelineBase*, uint32)>&& callback)
            : graphicsPipeline(graphicsPipeline),
              slot(slot),
              callback(std::move(callback))
        {
            Assert(graphicsPipeline != nullptr && slot != ~0u);
        }

        virtual ~RENDER_COMMAND(CreateGraphicsPipelineAndAddToCache)() override = default;

        virtual RendererResult operator()() override
        {
            HYP_GFX_CHECK(graphicsPipeline->Create());

            if (callback.IsValid())
            {
                // set initial lastFrame index so we don't delete it right away when cleaning up after the frame.
                graphicsPipeline->lastFrame = RenderApi_GetFrameCounter();

                callback(graphicsPipeline, slot);
            }

            return RendererResult {};
        }
    };

    PUSH_RENDER_COMMAND(CreateGraphicsPipelineAndAddToCache, *cacheHandle.m_pRef, slot, std::move(newCallback));

    return cacheHandle;
}

GraphicsPipelineCacheHandle GraphicsPipelineCache::FindGraphicsPipeline(
    const ShaderRef& shader,
    const DescriptorTableDeclaration& descriptorTableDecl,
    Span<const FramebufferRef> framebuffers,
    const RenderableAttributeSet& attributes)
{
    HYP_SCOPE;

    PerformanceClock clock;
    clock.Start();

    Mutex::Guard guard(m_mutex);

    const RenderableAttributeSet key = attributes;

    Span<GraphicsPipelineRef* const> pipelines = m_cachedPipelines->Find(key);

    if (!pipelines)
    {
        HYP_LOG(Rendering, Warning, "GraphicsPipelineCache cache miss ({}) ({} ms)", attributes.GetHashCode().Value(), clock.ElapsedMs());

        return {};
    }

    const HashCode shaderHashCode = shader->GetCompiledShader()->GetHashCode();

    for (GraphicsPipelineRef* const pPipeline : pipelines)
    {
        Assert(pPipeline != nullptr);

        if ((*pPipeline)->MatchesSignature(
                shader,
                descriptorTableDecl,
                Map(framebuffers, [](const FramebufferRef& framebuffer)
                    {
                        return static_cast<const FramebufferBase*>(framebuffer.Get());
                    }),
                attributes))
        {
            HYP_LOG(Rendering, Info, "GraphicsPipelineCache cache hit ({}) ({} ms)", attributes.GetHashCode().Value(), clock.ElapsedMs());

            return GraphicsPipelineCacheHandle(pPipeline);
        }
    }

    HYP_LOG(Rendering, Warning, "GraphicsPipelineCache cache miss ({}) ({} ms)", attributes.GetHashCode().Value(), clock.ElapsedMs());

    return {};
}

int GraphicsPipelineCache::RunCleanupCycle(int maxIter)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    const uint32 currFrame = RenderApi_GetFrameCounter();

    Mutex::Guard guard(m_mutex);

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

        if (frameDiff >= g_graphicsPipelineDiscardFrames)
        {
#ifdef HYP_DEBUG_MODE
            HYP_LOG(Rendering, Debug, "Removing graphics pipeline {} (debug name: {}) from cache as it has not been used in {} frames",
                graphicsPipeline->Id(),
                graphicsPipeline->GetDebugName(),
                frameDiff);
#endif

            m_cachedPipelines->Remove(m_cachedPipelines->IndexOf(m_cachedPipelines->cleanupIterator));
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

} // namespace hyperion
