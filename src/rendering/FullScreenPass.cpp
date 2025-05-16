/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/FullScreenPass.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderMesh.hpp>
#include <rendering/RenderTexture.hpp>
#include <rendering/RenderEnvProbe.hpp>
#include <rendering/RenderView.hpp>
#include <rendering/RenderEnvGrid.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/RenderState.hpp>
#include <rendering/TemporalBlending.hpp>

#include <rendering/backend/RendererSwapchain.hpp> // temp
#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererFramebuffer.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>

#include <scene/Mesh.hpp>
#include <scene/Texture.hpp>

#include <core/math/MathUtil.hpp>

#include <Engine.hpp>
#include <Types.hpp>

#include <util/MeshBuilder.hpp>

#include <core/profiling/ProfileScope.hpp>

namespace hyperion {

using renderer::CommandBufferType;
using renderer::FillMode;

struct MergeHalfResTexturesUniforms
{
    Vec2u   dimensions;
};

#pragma region Render commands

struct RENDER_COMMAND(RecreateFullScreenPassFramebuffer) : renderer::RenderCommand
{
    FullScreenPass  &full_screen_pass;
    Vec2u           new_size;

    RENDER_COMMAND(RecreateFullScreenPassFramebuffer)(FullScreenPass &full_screen_pass, Vec2u new_size)
        : full_screen_pass(full_screen_pass),
          new_size(new_size)
    {
    }

    virtual ~RENDER_COMMAND(RecreateFullScreenPassFramebuffer)() override = default;

    virtual RendererResult operator()() override
    {
        if (full_screen_pass.m_is_initialized) {
            full_screen_pass.Resize_Internal(new_size);
        } else {
            full_screen_pass.m_extent = new_size;
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

FullScreenPass::FullScreenPass(InternalFormat image_format, GBuffer *gbuffer)
    : FullScreenPass(nullptr, image_format, Vec2u::Zero(), gbuffer)
{
}

FullScreenPass::FullScreenPass(InternalFormat image_format, Vec2u extent, GBuffer *gbuffer)
    : FullScreenPass(nullptr, image_format, extent, gbuffer)
{
}

FullScreenPass::FullScreenPass(
    const ShaderRef &shader,
    const DescriptorTableRef &descriptor_table,
    InternalFormat image_format,
    Vec2u extent,
    GBuffer *gbuffer
) : FullScreenPass(
        shader,
        image_format,
        extent,
        gbuffer
    )
{
    m_descriptor_table.Set(descriptor_table);
}

FullScreenPass::FullScreenPass(
    const ShaderRef &shader,
    InternalFormat image_format,
    Vec2u extent,
    GBuffer *gbuffer
) : m_shader(shader),
    m_image_format(image_format),
    m_extent(extent),
    m_gbuffer(gbuffer),
    m_blend_function(BlendFunction::None()),
    m_is_initialized(false),
    m_is_first_frame(true)
{
}

FullScreenPass::~FullScreenPass()
{
    if (m_framebuffer.IsValid() && m_render_group.IsValid()) {
        m_render_group->RemoveFramebuffer(m_framebuffer);
    }

    m_render_group.Reset();
    m_full_screen_quad.Reset();

    SafeRelease(std::move(m_framebuffer));

    if (m_is_initialized) {
        // Prevent dangling reference from render commands
        HYP_SYNC_RENDER();
    }
}

const ImageViewRef &FullScreenPass::GetFinalImageView() const
{
    if (UsesTemporalBlending()) {
        AssertThrow(m_temporal_blending != nullptr);

        return m_temporal_blending->GetResultTexture()->GetRenderResource().GetImageView();
    }

    if (ShouldRenderHalfRes()) {
        return m_merge_half_res_textures_pass->GetFinalImageView();
    }

    AttachmentBase *color_attachment = GetAttachment(0);

    if (!color_attachment) {
        return ImageViewRef::Null();
    }

    return color_attachment->GetImageView();
}

const ImageViewRef &FullScreenPass::GetPreviousFrameColorImageView() const
{
    // If we're rendering at half res, we use the same image we render to but at an offset.
    if (ShouldRenderHalfRes()) {
        AttachmentBase *color_attachment = GetAttachment(0);

        if (!color_attachment) {
            return ImageViewRef::Null();
        }

        return color_attachment->GetImageView();
    }

    if (m_previous_texture.IsValid()) {
        return m_previous_texture->GetRenderResource().GetImageView();
    }

    return ImageViewRef::Null();
}

void FullScreenPass::Create()
{
    HYP_SCOPE;

    AssertThrow(!m_is_initialized);

    AssertThrowMsg(
        m_image_format != InternalFormat::NONE,
        "Image format must be set before creating the full screen pass"
    );

    CreateQuad();
    CreateFramebuffer();
    CreateMergeHalfResTexturesPass();
    CreateRenderTextureToScreenPass();
    CreateTemporalBlending();
    CreateDescriptors();
    CreatePipeline();

    m_is_initialized = true;
}

void FullScreenPass::SetShader(const ShaderRef &shader)
{
    if (m_shader == shader) {
        return;
    }

    m_shader = shader;
}

AttachmentBase *FullScreenPass::GetAttachment(uint32 attachment_index) const
{
    AssertThrow(m_framebuffer.IsValid());

    return m_framebuffer->GetAttachment(attachment_index);
}

void FullScreenPass::SetBlendFunction(const BlendFunction &blend_function)
{
    m_blend_function = blend_function;
}

void FullScreenPass::Resize(Vec2u new_size)
{
    PUSH_RENDER_COMMAND(RecreateFullScreenPassFramebuffer, *this, new_size);
}

void FullScreenPass::Resize_Internal(Vec2u new_size)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    if (m_extent == new_size) {
        return;
    }

    if (new_size.x * new_size.y == 0) {
        // AssertThrow(m_gbuffer != nullptr);
        // new_size = m_gbuffer->GetExtent();
        // TEMP HACK
        m_extent = g_rendering_api->GetSwapchain()->GetExtent();
    }

    m_extent = new_size;

    if (!m_framebuffer.IsValid()) {
        // Not created yet; skip
        return;
    }

    SafeRelease(std::move(m_framebuffer));
    m_temporal_blending.Reset();

    m_render_group.Reset();

    CreateFramebuffer();
    CreateMergeHalfResTexturesPass();
    CreateRenderTextureToScreenPass();
    CreateTemporalBlending();
    CreateDescriptors();
    CreatePipeline();
}

void FullScreenPass::CreateQuad()
{
    HYP_SCOPE;

    m_full_screen_quad = MeshBuilder::Quad();
    InitObject(m_full_screen_quad);

    // Allow Render() to be called directly without a RenderGroup
    m_full_screen_quad->SetPersistentRenderResourceEnabled(true);
}

void FullScreenPass::CreateFramebuffer()
{
    HYP_SCOPE;

    if (m_extent.x * m_extent.y == 0) {
        // AssertThrow(m_gbuffer != nullptr);
        // m_extent = m_gbuffer->GetExtent();
        // TEMP HACK
        m_extent = g_rendering_api->GetSwapchain()->GetExtent();
    }

    Vec2u framebuffer_extent = m_extent;

    if (ShouldRenderHalfRes()) {
        constexpr double resolution_scale = 0.5;

        const uint32 num_pixels = framebuffer_extent.x * framebuffer_extent.y;
        const int num_pixels_scaled = MathUtil::Ceil(num_pixels * resolution_scale);

        const Vec2i reshaped_extent = MathUtil::ReshapeExtent(Vec2i { num_pixels_scaled, 1 });

        // double the width as we swap between the two halves when rendering (checkerboarded)
        framebuffer_extent = Vec2u { uint32(reshaped_extent.x * 2), uint32(reshaped_extent.y) };

        DebugLog(LogType::Debug, "Reshaped extent: %d, %d to %d, %d", m_extent.x, m_extent.y, framebuffer_extent.x, framebuffer_extent.y);
    }

    m_framebuffer = g_rendering_api->MakeFramebuffer(framebuffer_extent);

    TextureDesc texture_desc;
    texture_desc.type = ImageType::TEXTURE_TYPE_2D;
    texture_desc.format = m_image_format;
    texture_desc.extent = Vec3u { framebuffer_extent, 1 };
    texture_desc.filter_mode_min = renderer::FilterMode::TEXTURE_FILTER_NEAREST;
    texture_desc.filter_mode_mag = renderer::FilterMode::TEXTURE_FILTER_NEAREST;
    texture_desc.wrap_mode = renderer::WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE;
    texture_desc.image_format_capabilities = ImageFormatCapabilities::ATTACHMENT | ImageFormatCapabilities::SAMPLED;

    ImageRef attachment_image = g_rendering_api->MakeImage(texture_desc);
    DeferCreate(attachment_image);

    AttachmentRef attachment = m_framebuffer->AddAttachment(
        0,
        attachment_image,
        ShouldRenderHalfRes() ? renderer::LoadOperation::LOAD : renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE
    );

    if (m_blend_function != BlendFunction::None()) {
        attachment->SetAllowBlending(true);
    }

    DeferCreate(attachment);

    DeferCreate(m_framebuffer);
}

void FullScreenPass::CreatePipeline()
{
    HYP_SCOPE;

    CreatePipeline(RenderableAttributeSet(
        MeshAttributes {
            .vertex_attributes = static_mesh_vertex_attributes
        },
        MaterialAttributes {
            .fill_mode      = FillMode::FILL,
            .blend_function = m_blend_function,
            .flags          = MaterialAttributeFlags::NONE
        }
    ));
}

void FullScreenPass::CreatePipeline(const RenderableAttributeSet &renderable_attributes)
{
    HYP_SCOPE;

    if (m_descriptor_table.HasValue()) {
        m_render_group = CreateObject<RenderGroup>(
            m_shader,
            renderable_attributes,
            *m_descriptor_table,
            RenderGroupFlags::NONE
        );
    } else {
        m_render_group = CreateObject<RenderGroup>(
            m_shader,
            renderable_attributes,
            RenderGroupFlags::NONE
        );
    }

    m_render_group->AddFramebuffer(m_framebuffer);

    InitObject(m_render_group);
}

void FullScreenPass::CreateTemporalBlending()
{
    HYP_SCOPE;

    if (!UsesTemporalBlending()) {
        return;
    }

    m_temporal_blending = MakeUnique<TemporalBlending>(
        m_extent,
        InternalFormat::RGBA8,
        TemporalBlendTechnique::TECHNIQUE_3,
        TemporalBlendFeedback::LOW,
        ShouldRenderHalfRes()
            ? m_merge_half_res_textures_pass->GetFinalImageView()
            : GetAttachment(0)->GetImageView(),
        m_gbuffer
    );

    m_temporal_blending->Create();
}

void FullScreenPass::CreatePreviousTexture()
{
    // Create previous image
    m_previous_texture = CreateObject<Texture>(TextureDesc {
        ImageType::TEXTURE_TYPE_2D,
        m_image_format,
        Vec3u { m_extent.x, m_extent.y, 1 },
        FilterMode::TEXTURE_FILTER_LINEAR,
        FilterMode::TEXTURE_FILTER_LINEAR,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE
    });

    InitObject(m_previous_texture);

    m_previous_texture->SetPersistentRenderResourceEnabled(true);
}

void FullScreenPass::CreateRenderTextureToScreenPass()
{
    HYP_SCOPE;

    if (!UsesTemporalBlending()) {
        return;
    }

    if (!ShouldRenderHalfRes()) {
        CreatePreviousTexture();
    }

    // Create render texture to screen pass.
    // this is used to render the previous frame's result to the screen,
    // so we can blend it with the current frame's result (checkerboarded)

    ShaderProperties shader_properties;

    if (ShouldRenderHalfRes()) {
        shader_properties.Set("HALFRES");
    }

    ShaderRef render_texture_to_screen_shader = g_shader_manager->GetOrCreate(NAME("RenderTextureToScreen"), shader_properties);
    AssertThrow(render_texture_to_screen_shader.IsValid());

    const DescriptorTableDeclaration descriptor_table_decl = render_texture_to_screen_shader->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable();
    DescriptorTableRef descriptor_table = g_rendering_api->MakeDescriptorTable(descriptor_table_decl);

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        const DescriptorSetRef &descriptor_set = descriptor_table->GetDescriptorSet(NAME("RenderTextureToScreenDescriptorSet"), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(NAME("InTexture"), GetPreviousFrameColorImageView());
    }

    DeferCreate(descriptor_table);

    m_render_texture_to_screen_pass = MakeUnique<FullScreenPass>(
        render_texture_to_screen_shader,
        std::move(descriptor_table),
        m_image_format,
        m_extent,
        nullptr
    );

    m_render_texture_to_screen_pass->Create();
}

void FullScreenPass::CreateMergeHalfResTexturesPass()
{
    HYP_SCOPE;

    if (!ShouldRenderHalfRes()) {
        return;
    }

    GPUBufferRef merge_half_res_textures_uniform_buffer;

    { // Create uniform buffer
        MergeHalfResTexturesUniforms uniforms;
        uniforms.dimensions = m_extent;

        merge_half_res_textures_uniform_buffer = g_rendering_api->MakeGPUBuffer(GPUBufferType::CONSTANT_BUFFER, sizeof(uniforms));
        HYPERION_ASSERT_RESULT(merge_half_res_textures_uniform_buffer->Create());
        merge_half_res_textures_uniform_buffer->Copy(sizeof(uniforms), &uniforms);
    }

    ShaderRef merge_half_res_textures_shader = g_shader_manager->GetOrCreate(NAME("MergeHalfResTextures"));
    AssertThrow(merge_half_res_textures_shader.IsValid());

    const DescriptorTableDeclaration descriptor_table_decl = merge_half_res_textures_shader->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable();
    DescriptorTableRef descriptor_table = g_rendering_api->MakeDescriptorTable(descriptor_table_decl);

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        const DescriptorSetRef &descriptor_set = descriptor_table->GetDescriptorSet(NAME("MergeHalfResTexturesDescriptorSet"), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(NAME("InTexture"), GetAttachment(0)->GetImageView());
        descriptor_set->SetElement(NAME("UniformBuffer"), merge_half_res_textures_uniform_buffer);
    }

    DeferCreate(descriptor_table);

    m_merge_half_res_textures_pass = MakeUnique<FullScreenPass>(
        merge_half_res_textures_shader,
        std::move(descriptor_table),
        m_image_format,
        m_extent,
        nullptr
    );

    m_merge_half_res_textures_pass->Create();
}

void FullScreenPass::CreateDescriptors()
{
}

void FullScreenPass::RenderPreviousTextureToScreen(FrameBase *frame, ViewRenderResource *view)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    const uint32 frame_index = frame->GetFrameIndex();

    AssertThrow(m_render_texture_to_screen_pass != nullptr);

    
    if (ShouldRenderHalfRes()) {
        const Vec2i viewport_offset = (Vec2i(m_framebuffer->GetExtent().x, 0) / 2) * (g_engine->GetRenderState()->frame_counter & 1);
        const Vec2i viewport_extent = Vec2i(m_framebuffer->GetExtent().x / 2, m_framebuffer->GetExtent().y);

        // render previous frame's result to screen
        frame->GetCommandList().Add<BindGraphicsPipeline>(
            m_render_texture_to_screen_pass->GetRenderGroup()->GetPipeline(),
            viewport_offset,
            viewport_extent
        );
    } else {
        // render previous frame's result to screen
        frame->GetCommandList().Add<BindGraphicsPipeline>(m_render_texture_to_screen_pass->GetRenderGroup()->GetPipeline());
    }

    frame->GetCommandList().Add<BindDescriptorTable>(
        m_render_texture_to_screen_pass->GetRenderGroup()->GetPipeline()->GetDescriptorTable(),
        m_render_texture_to_screen_pass->GetRenderGroup()->GetPipeline(),
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            {
                NAME("Global"),
                {
                    { NAME("ScenesBuffer"), ShaderDataOffset<SceneShaderData>(*view->GetScene()) },
                    { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(*view->GetCamera()) }
                }
            }
        },
        frame_index
    );

    const uint32 scene_descriptor_set_index = m_render_texture_to_screen_pass->GetRenderGroup()->GetPipeline()->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Scene"));

    if (view != nullptr && scene_descriptor_set_index != ~0u) {
        frame->GetCommandList().Add<BindDescriptorSet>(
            view->GetDescriptorSets()[frame->GetFrameIndex()],
            m_render_texture_to_screen_pass->GetRenderGroup()->GetPipeline(),
            ArrayMap<Name, uint32> { },
            scene_descriptor_set_index
        );
    }

    m_full_screen_quad->GetRenderResource().Render(frame->GetCommandList());
}

void FullScreenPass::CopyResultToPreviousTexture(FrameBase *frame, ViewRenderResource *view)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    const uint32 frame_index = frame->GetFrameIndex();

    AssertThrow(m_previous_texture.IsValid());

    const ImageRef &src_image = m_framebuffer->GetAttachment(0)->GetImage();
    const ImageRef &dst_image = m_previous_texture->GetRenderResource().GetImage();

    frame->GetCommandList().Add<InsertBarrier>(src_image, renderer::ResourceState::COPY_SRC);
    frame->GetCommandList().Add<InsertBarrier>(dst_image, renderer::ResourceState::COPY_DST);

    frame->GetCommandList().Add<Blit>(src_image, dst_image);

    frame->GetCommandList().Add<InsertBarrier>(src_image, renderer::ResourceState::SHADER_RESOURCE);
    frame->GetCommandList().Add<InsertBarrier>(dst_image, renderer::ResourceState::SHADER_RESOURCE);
}

void FullScreenPass::MergeHalfResTextures(FrameBase *frame, ViewRenderResource *view)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    const uint32 frame_index = frame->GetFrameIndex();

    AssertThrow(m_merge_half_res_textures_pass != nullptr);

    m_merge_half_res_textures_pass->Render(frame, view);
}

void FullScreenPass::Render(FrameBase *frame, ViewRenderResource *view)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    const uint32 frame_index = frame->GetFrameIndex();

    frame->GetCommandList().Add<BeginFramebuffer>(m_framebuffer, frame_index);

    RenderToFramebuffer(frame, view, m_framebuffer);

    frame->GetCommandList().Add<EndFramebuffer>(m_framebuffer, frame_index);

    if (ShouldRenderHalfRes()) {
        MergeHalfResTextures(frame, view);
    }

    if (UsesTemporalBlending()) {
        if (!ShouldRenderHalfRes()) {
            CopyResultToPreviousTexture(frame, view);
        }

        m_temporal_blending->Render(frame, view);
    }
}

void FullScreenPass::RenderToFramebuffer(FrameBase *frame, ViewRenderResource *view, const FramebufferRef &framebuffer)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    // render previous frame's result to screen
    if (!m_is_first_frame && m_render_texture_to_screen_pass != nullptr) {
        RenderPreviousTextureToScreen(frame, view);
    }

    const TResourceHandle<EnvProbeRenderResource> &env_probe_render_resource_handle = g_engine->GetRenderState()->GetActiveEnvProbe();
    const TResourceHandle<EnvGridRenderResource> &env_grid_render_resource_handle = g_engine->GetRenderState()->GetActiveEnvGrid();

    m_render_group->GetPipeline()->SetPushConstants(m_push_constant_data.Data(), m_push_constant_data.Size());
    
    if (ShouldRenderHalfRes()) {
        AssertThrowMsg(framebuffer != nullptr, "Framebuffer must be set before rendering to it, if rendering at half res");

        const Vec2i viewport_offset = (Vec2i(framebuffer->GetExtent().x, 0) / 2) * (g_engine->GetRenderState()->frame_counter & 1);
        const Vec2i viewport_extent = Vec2i(framebuffer->GetExtent().x / 2, framebuffer->GetExtent().y);

        frame->GetCommandList().Add<BindGraphicsPipeline>(
            m_render_group->GetPipeline(),
            viewport_offset,
            viewport_extent
        );
    } else {
        frame->GetCommandList().Add<BindGraphicsPipeline>(m_render_group->GetPipeline());
    }

    frame->GetCommandList().Add<BindDescriptorTable>(
        m_render_group->GetPipeline()->GetDescriptorTable(),
        m_render_group->GetPipeline(),
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            {
                NAME("Global"),
                {
                    { NAME("ScenesBuffer"), ShaderDataOffset<SceneShaderData>(*view->GetScene()) },
                    { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(*view->GetCamera()) },
                    { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(env_grid_render_resource_handle.Get(), 0) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(env_probe_render_resource_handle.Get(), 0) }
                }
            }
        },
        frame->GetFrameIndex()
    );

    const uint32 scene_descriptor_set_index = m_render_group->GetPipeline()->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Scene"));

    if (view != nullptr && scene_descriptor_set_index != ~0u) {
        frame->GetCommandList().Add<BindDescriptorSet>(
            view->GetDescriptorSets()[frame->GetFrameIndex()],
            m_render_group->GetPipeline(),
            ArrayMap<Name, uint32> { },
            scene_descriptor_set_index
        );
    }

    m_full_screen_quad->GetRenderResource().Render(frame->GetCommandList());

    m_is_first_frame = false;
}

void FullScreenPass::Begin(FrameBase *frame, ViewRenderResource *view)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    const uint32 frame_index = frame->GetFrameIndex();

    frame->GetCommandList().Add<BeginFramebuffer>(m_framebuffer, frame_index);

    if (ShouldRenderHalfRes()) {
        const Vec2i viewport_offset = (Vec2i(m_framebuffer->GetExtent().x, 0) / 2) * (g_engine->GetRenderState()->frame_counter & 1);
        const Vec2i viewport_extent = Vec2i(m_framebuffer->GetExtent().x / 2, m_framebuffer->GetExtent().y);

        frame->GetCommandList().Add<BindGraphicsPipeline>(
            m_render_group->GetPipeline(),
            viewport_offset,
            viewport_extent
        );
    } else {
        frame->GetCommandList().Add<BindGraphicsPipeline>(m_render_group->GetPipeline());
    }
}

void FullScreenPass::End(FrameBase *frame, ViewRenderResource *view)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    const uint32 frame_index = frame->GetFrameIndex();

    // render previous frame's result to screen
    if (!m_is_first_frame && m_render_texture_to_screen_pass != nullptr) {
        RenderPreviousTextureToScreen(frame, view);
    }

    frame->GetCommandList().Add<EndFramebuffer>(m_framebuffer, frame_index);

    if (ShouldRenderHalfRes()) {
        MergeHalfResTextures(frame, view);
    }

    if (UsesTemporalBlending()) {
        if (!ShouldRenderHalfRes()) {
            CopyResultToPreviousTexture(frame, view);
        }

        m_temporal_blending->Render(frame, view);
    }

    m_is_first_frame = false;
}

} // namespace hyperion
