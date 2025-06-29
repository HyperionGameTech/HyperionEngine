#include <rendering/Renderer.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/GraphicsPipelineCache.hpp>
#include <rendering/DrawCall.hpp>

#include <scene/View.hpp>

#include <core/threading/Task.hpp>
#include <core/threading/TaskSystem.hpp>
#include <core/threading/Threads.hpp>

#include <core/Defines.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <Engine.hpp>

namespace hyperion {

const RenderSetup& NullRenderSetup()
{
    static const RenderSetup null_render_setup;
    return null_render_setup;
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

    for (auto& entry : render_group_cache)
    {
        HYP_LOG(Rendering, Debug, "Destroying RenderGroupCacheEntry for RenderGroup '{}'", entry.render_group.Id());

        SafeRelease(std::move(entry.graphics_pipeline));
    }

    SafeRelease(std::move(descriptor_sets));
}

void PassData::CullUnusedGraphicsPipelines(uint32 max_iter)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    // Ensures the iterator is valid: the Iterator type for SparsePagedArray will find the next available slot in the constructor
    // elements may have been added in the middle or removed in the meantime.
    // elements that were added will be handled after the next time this loops around; elements that were removed will be skipped over to find the next valid entry.
    render_group_cache_iterator = typename SparsePagedArray<RenderGroupCacheEntry, 32>::Iterator(
        &render_group_cache,
        render_group_cache_iterator.page,
        render_group_cache_iterator.elem);

    // Loop around to the beginning of the container when the end is reached.
    if (render_group_cache_iterator == render_group_cache.End())
    {
        render_group_cache_iterator = render_group_cache.Begin();
    }

    for (uint32 i = 0; render_group_cache_iterator != render_group_cache.End() && i < max_iter; i++)
    {
        RenderGroupCacheEntry& entry = *render_group_cache_iterator;

        if (!entry.render_group.Lock())
        {
            HYP_LOG(Rendering, Debug, "Removing graphics pipeline for RenderGroup '{}' as it is no longer valid.", entry.render_group.Id());

            SafeRelease(std::move(entry.graphics_pipeline));

            render_group_cache_iterator = render_group_cache.Erase(render_group_cache_iterator);

            continue;
        }

        ++render_group_cache_iterator;
    }
}

GraphicsPipelineRef PassData::CreateGraphicsPipeline(
    PassData* pd,
    const ShaderRef& shader,
    const RenderableAttributeSet& renderable_attributes,
    const DescriptorTableRef& descriptor_table,
    IDrawCallCollectionImpl* impl)
{
    HYP_SCOPE;

    AssertThrow(pd != nullptr);

    Handle<View> view = pd->view.Lock();
    AssertThrow(view.IsValid());
    AssertThrow(view->GetOutputTarget().IsValid());

    AssertThrow(shader.IsValid());

    DescriptorTableRef table;

    if (!descriptor_table.IsValid())
    {
        const DescriptorTableDeclaration& descriptor_table_decl = shader->GetCompiledShader()->GetDescriptorTableDeclaration();

        table = g_render_backend->MakeDescriptorTable(&descriptor_table_decl);
        table->SetDebugName(NAME_FMT("DescriptorTable_{}", shader->GetCompiledShader()->GetName()));

        // Setup instancing buffers if "Instancing" descriptor set exists
        const uint32 instancing_descriptor_set_index = table->GetDescriptorSetIndex(NAME("Instancing"));

        if (instancing_descriptor_set_index != ~0u)
        {
            if (!impl)
            {
                impl = GetOrCreateDrawCallCollectionImpl<EntityInstanceBatch>();
            }

            AssertDebug(impl != nullptr);

            for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
            {
                const GpuBufferRef& gpu_buffer = impl->GetEntityInstanceBatchHolder()->GetBuffer(frame_index);
                AssertThrow(gpu_buffer.IsValid());

                const DescriptorSetRef& instancing_descriptor_set = table->GetDescriptorSet(NAME("Instancing"), frame_index);
                AssertThrow(instancing_descriptor_set.IsValid());

                instancing_descriptor_set->SetElement(NAME("EntityInstanceBatchesBuffer"), gpu_buffer);
            }
        }

        DeferCreate(table);
    }
    else
    {
        table = descriptor_table;
    }

    AssertThrow(table.IsValid());

    return g_engine->GetGraphicsPipelineCache()->GetOrCreate(
        shader,
        table,
        view->GetOutputTarget().GetFramebuffers(),
        renderable_attributes);
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

RendererBase::~RendererBase()
{
    for (PassData* pd : m_view_pass_data)
    {
        delete pd;
    }
}

PassData* RendererBase::TryGetViewPassData(View* view)
{
    if (!view)
    {
        return nullptr;
    }

    if (PassData** pd_ptr = m_view_pass_data.TryGet(view->Id().ToIndex()))
    {
        return *pd_ptr;
    }

    return nullptr;
}

PassData* RendererBase::FetchViewPassData(View* view, PassDataExt* ext)
{
    if (!view)
    {
        return nullptr;
    }

    PassData* pd = nullptr;

    if (PassData** pd_ptr = m_view_pass_data.TryGet(view->Id().ToIndex()))
    {
        pd = *pd_ptr;
    }

    if (!pd)
    {
        NullPassDataExt null_pass_data_ext {};

        // call virtual function to alloc / create

        pd = CreateViewPassData(view, ext ? *ext : null_pass_data_ext);
        AssertDebug(pd != nullptr);

        pd->next = ext ? ext->Clone() : nullptr;

        m_view_pass_data.Set(view->Id().ToIndex(), pd);
    }
    else if (pd->view.GetUnsafe() != view)
    {
        delete pd;

        NullPassDataExt null_pass_data_ext {};

        pd = CreateViewPassData(view, ext ? *ext : null_pass_data_ext);
        AssertDebug(pd != nullptr);

        pd->next = ext ? ext->Clone() : nullptr;

        m_view_pass_data.Set(view->Id().ToIndex(), pd);
    }

    AssertDebug(pd->view.GetUnsafe() == view);

    return pd;
}

#pragma region RendererBase

} // namespace hyperion
