/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/HBAO.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/RenderEnvironment.hpp>

#include <rendering/backend/RendererGraphicsPipeline.hpp>

#include <core/system/AppContext.hpp>

#include <util/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region Render commands

struct RENDER_COMMAND(AddHBAOFinalImagesToGlobalDescriptorSet) : renderer::RenderCommand
{
    FixedArray<ImageViewRef, max_frames_in_flight> pass_image_views;

    RENDER_COMMAND(AddHBAOFinalImagesToGlobalDescriptorSet)(FixedArray<ImageViewRef, max_frames_in_flight> &&pass_image_views)
        : pass_image_views(std::move(pass_image_views))
    {
    }

    virtual ~RENDER_COMMAND(AddHBAOFinalImagesToGlobalDescriptorSet)() override = default;

    virtual Result operator()() override
    {
        for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)
                ->SetElement(NAME("SSAOResultTexture"), pass_image_views[frame_index]);
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(RemoveHBAODescriptors) : renderer::RenderCommand
{
    RENDER_COMMAND(RemoveHBAODescriptors)()
    {
    }

    virtual ~RENDER_COMMAND(RemoveHBAODescriptors)() override = default;

    virtual Result operator()() override
    {
        Result result;

        for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Global"), frame_index)
                ->SetElement(NAME("SSAOResultTexture"), g_engine->GetPlaceholderData()->GetImageView2D1x1R8());
        }

        return result;
    }
};

#pragma endregion Render commands

HBAO::HBAO()
    : FullScreenPass(InternalFormat::RGBA8)
{
}

HBAO::~HBAO()
{
    m_temporal_blending.Reset();

    PUSH_RENDER_COMMAND(RemoveHBAODescriptors);
}

void HBAO::Create()
{
    ShaderProperties shader_properties;
    shader_properties.Set("HBIL_ENABLED", g_engine->GetAppContext()->GetConfiguration().Get("rendering.hbil.enabled").ToBool());

    m_shader = g_shader_manager->GetOrCreate(
        NAME("HBAO"),
        shader_properties
    );

    FullScreenPass::Create();

    CreateTemporalBlending();
}

void HBAO::CreatePipeline(const RenderableAttributeSet &renderable_attributes)
{
    renderer::DescriptorTableDeclaration descriptor_table_decl = m_shader->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable();

    DescriptorTableRef descriptor_table = MakeRenderObject<DescriptorTable>(descriptor_table_decl);
    DeferCreate(descriptor_table, g_engine->GetGPUDevice());

    m_descriptor_table = descriptor_table;

    m_render_group = CreateObject<RenderGroup>(
        m_shader,
        renderable_attributes,
        *m_descriptor_table,
        RenderGroupFlags::NONE
    );

    m_render_group->AddFramebuffer(m_framebuffer);

    InitObject(m_render_group);
}

void HBAO::CreateTemporalBlending()
{
    m_temporal_blending.Reset(new TemporalBlending(
        GetFramebuffer()->GetExtent(),
        InternalFormat::RGBA8,
        TemporalBlendTechnique::TECHNIQUE_3,
        TemporalBlendFeedback::LOW,
        GetFramebuffer()
    ));

    m_temporal_blending->Create();
}

void HBAO::Resize_Internal(Extent2D new_size)
{
    FullScreenPass::Resize_Internal(new_size);

    m_temporal_blending.Reset();

    CreateTemporalBlending();

    PUSH_RENDER_COMMAND(
        AddHBAOFinalImagesToGlobalDescriptorSet,
        FixedArray<ImageViewRef, max_frames_in_flight> {
            m_temporal_blending ? m_temporal_blending->GetImageOutput(0).image_view : GetAttachment(0)->GetImageView(),
            m_temporal_blending ? m_temporal_blending->GetImageOutput(1).image_view : GetAttachment(0)->GetImageView()
        }
    );
}

void HBAO::Record(uint frame_index)
{
}

void HBAO::Render(Frame *frame)
{
    HYP_SCOPE;

    const uint frame_index = frame->GetFrameIndex();
    const CommandBufferRef &command_buffer = frame->GetCommandBuffer();

    {
        struct alignas(128)
        {
            Vec2u   dimension;
        } push_constants;

        push_constants.dimension = m_extent;

        GetRenderGroup()->GetPipeline()->SetPushConstants(&push_constants, sizeof(push_constants));
        Begin(frame);

        Frame temporary_frame = Frame::TemporaryFrame(GetCommandBuffer(frame_index), frame_index);
        
        GetRenderGroup()->GetPipeline()->GetDescriptorTable()->Bind(
            &temporary_frame,
            GetRenderGroup()->GetPipeline(),
            {
                {
                    NAME("Scene"),
                    {
                        { NAME("ScenesBuffer"), HYP_RENDER_OBJECT_OFFSET(Scene, g_engine->GetRenderState().GetScene().id.ToIndex()) },
                        { NAME("CamerasBuffer"), HYP_RENDER_OBJECT_OFFSET(Camera, g_engine->GetRenderState().GetCamera().id.ToIndex()) },
                        { NAME("LightsBuffer"), HYP_RENDER_OBJECT_OFFSET(Light, 0) },
                        { NAME("EnvGridsBuffer"), HYP_RENDER_OBJECT_OFFSET(EnvGrid, 0) },
                        { NAME("CurrentEnvProbe"), HYP_RENDER_OBJECT_OFFSET(EnvProbe, 0) }
                    }
                }
            }
        );
        
        GetQuadMesh()->Render(GetCommandBuffer(frame_index));
        End(frame);
    }
    
    m_temporal_blending->Render(frame);
}

} // namespace hyperion