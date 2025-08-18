#include <rendering/Renderer.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/GraphicsPipelineCache.hpp>
#include <rendering/DrawCall.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/RenderGroup.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <scene/View.hpp>

#include <core/threading/Task.hpp>
#include <core/threading/TaskSystem.hpp>
#include <core/threading/Threads.hpp>

#include <core/Defines.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/profiling/PerformanceClock.hpp>

#include <engine/EngineGlobals.hpp>

namespace hyperion {

const RenderSetup& NullRenderSetup()
{
    static const RenderSetup nullRenderSetup;
    return nullRenderSetup;
}

#pragma region PassData

PassData::~PassData()
{
    HYP_LOG(Rendering, Debug, "Destroying PassData");

    if (next != nullptr)
    {
        delete next;
        next = nullptr;
    }

    for (auto& entry : renderGroupCache)
    {
        HYP_LOG(Rendering, Debug, "Destroying RenderGroupCacheEntry for RenderGroup '{}'", entry.renderGroup.Id());

        SafeDelete(std::move(entry.graphicsPipeline));
    }

    SafeDelete(std::move(descriptorSets));
}

int PassData::CullUnusedGraphicsPipelines(int maxIter)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    // Ensures the iterator is valid: the Iterator type for SparsePagedArray will find the next available slot in the constructor
    // elements may have been added in the middle or removed in the meantime.
    // elements that were added will be handled after the next time this loops around; elements that were removed will be skipped over to find the next valid entry.
    renderGroupCacheIterator = typename SparsePagedArray<RenderGroupCacheEntry, 32>::Iterator(
        &renderGroupCache,
        renderGroupCacheIterator.page,
        renderGroupCacheIterator.elem);

    int numCycles = 0;

    for (; numCycles < maxIter; numCycles++)
    {
        // Loop around to the beginning of the container when the end is reached.
        if (renderGroupCacheIterator == renderGroupCache.End())
        {
            renderGroupCacheIterator = renderGroupCache.Begin();
            
            // no elements if still at end
            if (renderGroupCacheIterator == renderGroupCache.End())
            {
                break;
            }
        }
        
        RenderGroupCacheEntry& entry = *renderGroupCacheIterator;

        // check refcount is zero without lock
        if (entry.renderGroup.GetUnsafe()->GetObjectHeader_Internal()->GetRefCountStrong() == 0)
        {
            HYP_LOG(Rendering, Debug, "Removing graphics pipeline for RenderGroup '{}' as it is no longer valid.", entry.renderGroup.Id());

            SafeDelete(std::move(entry.graphicsPipeline));

            renderGroupCacheIterator = renderGroupCache.Erase(renderGroupCacheIterator);

            continue;
        }

        ++renderGroupCacheIterator;
    }

    return numCycles;
}

GraphicsPipelineRef PassData::CreateGraphicsPipeline(
    PassData* pd,
    const ShaderRef& shader,
    const RenderableAttributeSet& renderableAttributes,
    const DescriptorTableRef& descriptorTable,
    IDrawCallCollectionImpl* impl)
{
    HYP_SCOPE;

    Assert(pd != nullptr);

    PerformanceClock clock;
    clock.Start();

    Handle<View> view = pd->view.Lock();
    Assert(view.IsValid());
    Assert(view->GetOutputTarget().IsValid());

    Assert(shader.IsValid());

    DescriptorTableRef table;

    if (!descriptorTable.IsValid())
    {
        const DescriptorTableDeclaration& descriptorTableDecl = shader->GetCompiledShader()->GetDescriptorTableDeclaration();

        table = g_renderBackend->MakeDescriptorTable(&descriptorTableDecl);
        table->SetDebugName(NAME_FMT("DescriptorTable_{}", shader->GetCompiledShader()->GetName()));

        // Setup instancing buffers if "Instancing" descriptor set exists
        const uint32 instancingDescriptorSetIndex = table->GetDescriptorSetIndex("Instancing");

        if (instancingDescriptorSetIndex != ~0u)
        {
            if (!impl)
            {
                impl = GetOrCreateDrawCallCollectionImpl<EntityInstanceBatch>();
            }

            AssertDebug(impl != nullptr);

            for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
            {
                const GpuBufferRef& gpuBuffer = impl->GetGpuBufferHolder()->GetBuffer(frameIndex);
                Assert(gpuBuffer.IsValid());

                const DescriptorSetRef& instancingDescriptorSet = table->GetDescriptorSet("Instancing", frameIndex);
                Assert(instancingDescriptorSet.IsValid());

                instancingDescriptorSet->SetElement("EntityInstanceBatchesBuffer", gpuBuffer);
            }
        }

        DeferCreate(table);
    }
    else
    {
        table = descriptorTable;
    }

    Assert(table.IsValid());

    GraphicsPipelineRef graphicsPipeline = g_renderGlobalState->graphicsPipelineCache->GetOrCreate(
        shader,
        table,
        view->GetOutputTarget().GetFramebuffers(),
        renderableAttributes);

    clock.Stop();

    HYP_LOG(Rendering, Debug, "Created graphics pipeline ({} ms)", clock.ElapsedMs());

    return graphicsPipeline;
}

#pragma endregion PassData

#pragma region RendererBase

struct NullPassDataExt final : PassDataExt
{
    PassDataExt* Clone() override
    {
        HYP_FAIL("Should not Clone() NullPassDataExt!");
    }
};

RendererBase::RendererBase()
    : m_viewPassDataCleanupIterator(m_viewPassData.End())
{
}

RendererBase::~RendererBase()
{
}

int RendererBase::RunCleanupCycle(int maxIter)
{
    // Ensures the iterator is valid: the Iterator type for SparsePagedArray will find the next available slot in the constructor
    // elements may have been added in the middle or removed in the meantime.
    // elements that were added will be handled after the next time this loops around; elements that were removed will be skipped over to find the next valid entry.
    m_viewPassDataCleanupIterator = typename SparsePagedArray<Handle<PassData>, 16>::Iterator(
        &m_viewPassData,
        m_viewPassDataCleanupIterator.page,
        m_viewPassDataCleanupIterator.elem);

    const typename SparsePagedArray<Handle<PassData>, 16>::Iterator startIterator = m_viewPassDataCleanupIterator; // the iterator we started at - use it to check that we don't do duplicate checks

    int numCycles = 0;
    for (; numCycles < maxIter; ++numCycles)
    {
        // Loop around to the beginning of the container when the end is reached.
        if (m_viewPassDataCleanupIterator == m_viewPassData.End())
        {
            m_viewPassDataCleanupIterator = m_viewPassData.Begin();

            if (m_viewPassDataCleanupIterator == m_viewPassData.End())
            {
                break;
            }
        }

        Handle<PassData>& pd = *m_viewPassDataCleanupIterator;

        if (!pd->view.Lock())
        {
            HYP_LOG(Rendering, Debug, "Removing PassData for View {} as it is no longer valid.", pd->view.Id());

            pd.Reset();

            m_viewPassDataCleanupIterator = m_viewPassData.Erase(m_viewPassDataCleanupIterator);
        }
        else
        {
            pd->CullUnusedGraphicsPipelines(1000);

            ++m_viewPassDataCleanupIterator;
        }

        if (m_viewPassDataCleanupIterator == startIterator)
        {
            // we checked everything
            break;
        }
    }

    return numCycles;
}

const Handle<PassData>& RendererBase::TryGetViewPassData(View* view)
{
    if (!view)
    {
        return Handle<PassData>::empty;
    }

    if (Handle<PassData>* pPassData = m_viewPassData.TryGet(view->Id().ToIndex()))
    {
        return *pPassData;
    }

    return Handle<PassData>::empty;
}

const Handle<PassData>& RendererBase::FetchViewPassData(View* view, PassDataExt* ext)
{
    if (!view)
    {
        return Handle<PassData>::empty;
    }

    Handle<PassData>* pPassData = m_viewPassData.TryGet(view->Id().ToIndex());

    if (!pPassData)
    {
        NullPassDataExt nullPassDataExt {};

        // call virtual function to alloc / create

        Handle<PassData> pd = CreateViewPassData(view, ext ? *ext : nullPassDataExt);
        AssertDebug(pd != nullptr);

        pd->next = ext ? ext->Clone() : nullptr;

        InitObject(pd);

        pPassData = &*m_viewPassData.Set(view->Id().ToIndex(), pd);
    }
    else if ((*pPassData)->view.GetUnsafe() != view)
    {
        Handle<PassData>& pd = *pPassData;
        pd.Reset();

        NullPassDataExt nullPassDataExt {};

        pd = CreateViewPassData(view, ext ? *ext : nullPassDataExt);
        AssertDebug(pd != nullptr);

        pd->next = ext ? ext->Clone() : nullptr;

        InitObject(pd);

        m_viewPassData.Set(view->Id().ToIndex(), pd);
    }

    AssertDebug(pPassData != nullptr && *pPassData != nullptr);
    AssertDebug((*pPassData)->view.GetUnsafe() == view);

    return *pPassData;
}

#pragma region RendererBase

} // namespace hyperion
