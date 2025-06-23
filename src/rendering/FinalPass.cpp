/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/FinalPass.hpp>
#include <rendering/Shader.hpp>
#include <rendering/FullScreenPass.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/RenderView.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/RenderTexture.hpp>
#include <rendering/RenderGlobalState.hpp>

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererSwapchain.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/logging/Logger.hpp>

#include <scene/Mesh.hpp>
#include <scene/Texture.hpp>

#include <util/MeshBuilder.hpp>

#include <system/AppContext.hpp>

#include <Engine.hpp>

#define HYP_RENDER_UI_IN_FINAL_PASS

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Rendering);

#pragma region Render commands

struct RENDER_COMMAND(SetUILayerImageView)
    : RenderCommand
{
    FinalPass& final_pass;
    ImageViewRef image_view;

    RENDER_COMMAND(SetUILayerImageView)(
        FinalPass& final_pass,
        const ImageViewRef& image_view)
        : final_pass(final_pass),
          image_view(image_view)
    {
    }

    virtual ~RENDER_COMMAND(SetUILayerImageView)() override = default;

    virtual RendererResult operator()() override
    {
        SafeRelease(std::move(final_pass.m_ui_layer_image_view));

        if (g_engine->IsShuttingDown())
        {
            // Don't set if the engine is in a shutdown state,
            // pipeline may already have been deleted.
            HYPERION_RETURN_OK;
        }

        if (final_pass.m_render_texture_to_screen_pass != nullptr)
        {
            const DescriptorTableRef& descriptor_table = final_pass.m_render_texture_to_screen_pass->GetRenderGroup()->GetPipeline()->GetDescriptorTable();
            AssertThrow(descriptor_table.IsValid());

            for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
            {
                const DescriptorSetRef& descriptor_set = descriptor_table->GetDescriptorSet(NAME("RenderTextureToScreenDescriptorSet"), frame_index);
                AssertThrow(descriptor_set != nullptr);

                if (image_view != nullptr)
                {
                    descriptor_set->SetElement(NAME("InTexture"), image_view);
                }
                else
                {
                    descriptor_set->SetElement(NAME("InTexture"), g_render_global_state->PlaceholderData->GetImageView2D1x1R8());
                }
            }
        }

        // Set frames to be dirty so the descriptor sets get updated before we render the UI
        final_pass.m_dirty_frame_indices = (1u << max_frames_in_flight) - 1;
        final_pass.m_ui_layer_image_view = image_view;

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

#pragma region FinalPass

FinalPass::FinalPass(const SwapchainRef& swapchain)
    : m_swapchain(swapchain),
      m_dirty_frame_indices(0)
{
}

FinalPass::~FinalPass()
{
    m_quad_mesh.Reset();

    if (m_render_texture_to_screen_pass != nullptr)
    {
        m_render_texture_to_screen_pass.Reset();
    }

    SafeRelease(std::move(m_ui_layer_image_view));
    SafeRelease(std::move(m_swapchain));
}

void FinalPass::SetUILayerImageView(const ImageViewRef& image_view)
{
    PUSH_RENDER_COMMAND(SetUILayerImageView, *this, image_view);
}

void FinalPass::Create()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    AssertThrow(m_swapchain != nullptr);

    m_extent = m_swapchain->GetExtent();
    m_image_format = m_swapchain->GetImageFormat();

    AssertThrow(m_extent.Volume() != 0);

    m_quad_mesh = MeshBuilder::Quad();
    InitObject(m_quad_mesh);
    m_quad_mesh->SetPersistentRenderResourceEnabled(true);

    ShaderRef render_texture_to_screen_shader = g_shader_manager->GetOrCreate(NAME("RenderTextureToScreen_UI"));
    AssertThrow(render_texture_to_screen_shader.IsValid());

    const DescriptorTableDeclaration& descriptor_table_decl = render_texture_to_screen_shader->GetCompiledShader()->GetDescriptorTableDeclaration();
    DescriptorTableRef descriptor_table = g_rendering_api->MakeDescriptorTable(&descriptor_table_decl);

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    {
        const DescriptorSetRef& descriptor_set = descriptor_table->GetDescriptorSet(NAME("RenderTextureToScreenDescriptorSet"), frame_index);
        AssertThrow(descriptor_set != nullptr);

        if (m_ui_layer_image_view != nullptr)
        {
            descriptor_set->SetElement(NAME("InTexture"), m_ui_layer_image_view);
        }
        else
        {
            descriptor_set->SetElement(NAME("InTexture"), g_render_global_state->PlaceholderData->GetImageView2D1x1R8());
        }
    }

    DeferCreate(descriptor_table);

    m_render_texture_to_screen_pass = MakeUnique<FullScreenPass>(
        render_texture_to_screen_shader,
        std::move(descriptor_table),
        m_image_format,
        m_extent,
        nullptr);

    m_render_texture_to_screen_pass->SetBlendFunction(BlendFunction(
        BMF_SRC_ALPHA, BMF_ONE_MINUS_SRC_ALPHA,
        BMF_ONE, BMF_ONE_MINUS_SRC_ALPHA));

    m_render_texture_to_screen_pass->Create();
}

void FinalPass::Render(FrameBase* frame, RenderWorld* render_world)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    const uint32 frame_index = frame->GetFrameIndex();
    const uint32 acquired_image_index = m_swapchain->GetAcquiredImageIndex();

    const FramebufferRef& framebuffer = m_swapchain->GetFramebuffers()[acquired_image_index];
    AssertDebug(framebuffer != nullptr);

    frame->GetCommandList().Add<BeginFramebuffer>(framebuffer, 0);
    frame->GetCommandList().Add<BindGraphicsPipeline>(m_render_texture_to_screen_pass->GetRenderGroup()->GetPipeline());

    frame->GetCommandList().Add<BindDescriptorTable>(
        m_render_texture_to_screen_pass->GetRenderGroup()->GetPipeline()->GetDescriptorTable(),
        m_render_texture_to_screen_pass->GetRenderGroup()->GetPipeline(),
        ArrayMap<Name, ArrayMap<Name, uint32>> {},
        frame_index);

    const uint32 descriptor_set_index = m_render_texture_to_screen_pass->GetRenderGroup()->GetPipeline()->GetDescriptorTable()->GetDescriptorSetIndex(NAME("RenderTextureToScreenDescriptorSet"));
    AssertDebug(descriptor_set_index != ~0u);

    // Render each sub-view
    DeferredRenderer* dr = static_cast<DeferredRenderer*>(g_render_global_state->Renderer);
    AssertDebug(dr != nullptr);

    // ordered by priority of the view
    for (const Pair<View*, DeferredPassData*>& it : dr->GetLastFrameData().pass_data)
    {
        View* view = it.first;
        AssertDebug(view != nullptr);

        DeferredPassData* pd = it.second;
        AssertDebug(pd != nullptr);

        AssertDebug(pd->final_pass_descriptor_set);

        frame->GetCommandList().Add<BindDescriptorSet>(
            pd->final_pass_descriptor_set,
            m_render_texture_to_screen_pass->GetRenderGroup()->GetPipeline(),
            ArrayMap<Name, uint32> {},
            descriptor_set_index);

        m_quad_mesh->GetRenderResource().Render(frame->GetCommandList());
    }

#ifdef HYP_RENDER_UI_IN_FINAL_PASS
    // Render UI onto screen, blending with the scene render pass
    if (m_ui_layer_image_view != nullptr)
    {
        // If the UI pass has needs to be updated for the current frame index, do it
        if (m_dirty_frame_indices & (1u << frame_index))
        {
            m_render_texture_to_screen_pass->GetRenderGroup()->GetPipeline()->GetDescriptorTable()->Update(frame_index);

            m_dirty_frame_indices &= ~(1u << frame_index);
        }

        frame->GetCommandList().Add<BindDescriptorSet>(
            m_render_texture_to_screen_pass->GetRenderGroup()->GetPipeline()->GetDescriptorTable()->GetDescriptorSet(NAME("RenderTextureToScreenDescriptorSet"), frame_index),
            m_render_texture_to_screen_pass->GetRenderGroup()->GetPipeline(),
            ArrayMap<Name, uint32> {},
            descriptor_set_index);

        m_quad_mesh->GetRenderResource().Render(frame->GetCommandList());
    }
#endif

    frame->GetCommandList().Add<EndFramebuffer>(framebuffer, 0);
}

#pragma endregion FinalPass

} // namespace hyperion
