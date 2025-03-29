/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/FullScreenPass.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderMesh.hpp>
#include <rendering/RenderEnvProbe.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/EnvGrid.hpp>
#include <rendering/RenderState.hpp>
#include <rendering/TemporalBlending.hpp>

#include <rendering/backend/RendererFramebuffer.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>

#include <scene/Mesh.hpp>

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

struct RENDER_COMMAND(CreateCommandBuffers) : renderer::RenderCommand
{
    FixedArray<CommandBufferRef, max_frames_in_flight>  command_buffers;

    RENDER_COMMAND(CreateCommandBuffers)(const FixedArray<CommandBufferRef, max_frames_in_flight> &command_buffers)
        : command_buffers(command_buffers)
    {
    }

    virtual ~RENDER_COMMAND(CreateCommandBuffers)() override = default;

    virtual RendererResult operator()() override
    {
        for (uint32 i = 0; i < max_frames_in_flight; i++) {
#ifdef HYP_VULKAN
            command_buffers[i]->GetPlatformImpl().command_pool = g_engine->GetGPUDevice()->GetGraphicsQueue().command_pools[0];
#endif

            HYPERION_BUBBLE_ERRORS(command_buffers[i]->Create(g_engine->GetGPUDevice()));
        }

        HYPERION_RETURN_OK;
    }
};

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

FullScreenPass::FullScreenPass()
    : FullScreenPass(InternalFormat::NONE, Vec2u::Zero())
{
}

FullScreenPass::FullScreenPass(InternalFormat image_format, Vec2u extent)
    : FullScreenPass(nullptr, image_format, extent)
{
}

FullScreenPass::FullScreenPass(
    const ShaderRef &shader,
    const DescriptorTableRef &descriptor_table,
    InternalFormat image_format,
    Vec2u extent
) : FullScreenPass(
        shader,
        image_format,
        extent
    )
{
    m_descriptor_table.Set(descriptor_table);
}

FullScreenPass::FullScreenPass(
    const ShaderRef &shader,
    InternalFormat image_format,
    Vec2u extent
) : m_shader(shader),
    m_image_format(image_format),
    m_extent(extent),
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
    SafeRelease(std::move(m_command_buffers));

    if (m_is_initialized) {
        // Prevent dangling reference from render commands
        HYP_SYNC_RENDER();
    }
}

const ImageViewRef &FullScreenPass::GetFinalImageView() const
{
    if (UsesTemporalBlending()) {
        AssertThrow(m_temporal_blending != nullptr);

        return m_temporal_blending->GetResultTexture()->GetImageView();
    }

    if (ShouldRenderHalfRes()) {
        return m_merge_half_res_textures_pass->GetFinalImageView();
    }

    const AttachmentRef &color_attachment = GetAttachment(0);

    if (!color_attachment.IsValid()) {
        return ImageViewRef::Null();
    }

    return color_attachment->GetImageView();
}

const ImageViewRef &FullScreenPass::GetPreviousFrameColorImageView() const
{
    // If we're rendering at half res, we use the same image we render to but at an offset.
    if (ShouldRenderHalfRes()) {
        const AttachmentRef &color_attachment = GetAttachment(0);

        if (!color_attachment.IsValid()) {
            return ImageViewRef::Null();
        }

        return color_attachment->GetImageView();
    }

    if (m_previous_texture.IsValid()) {
        return m_previous_texture->GetImageView();
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
    CreateCommandBuffers();
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

const AttachmentRef &FullScreenPass::GetAttachment(uint32 attachment_index) const
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

    AssertThrow(new_size.Volume() != 0);

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

void FullScreenPass::CreateCommandBuffers()
{
    HYP_SCOPE;

    for (uint32 i = 0; i < max_frames_in_flight; i++) {
        m_command_buffers[i] = MakeRenderObject<CommandBuffer>(CommandBufferType::COMMAND_BUFFER_SECONDARY);
    }

    // create command buffers in render thread
    PUSH_RENDER_COMMAND(CreateCommandBuffers, m_command_buffers);
}

void FullScreenPass::CreateFramebuffer()
{
    HYP_SCOPE;

    if (m_extent.x * m_extent.y == 0) {
        m_extent = g_engine->GetDeferredRenderer()->GetGBuffer()->GetResolution();
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

    m_framebuffer = MakeRenderObject<Framebuffer>(
        framebuffer_extent,
        renderer::RenderPassStage::SHADER,
        renderer::RenderPassMode::RENDER_PASS_SECONDARY_COMMAND_BUFFER
    );

    ImageRef attachment_image = MakeRenderObject<Image>(renderer::FramebufferImage2D(
        framebuffer_extent,
        m_image_format
    ));

    attachment_image->SetIsAttachmentTexture(true);

    DeferCreate(attachment_image, g_engine->GetGPUDevice());

    AttachmentRef attachment = MakeRenderObject<Attachment>(
        attachment_image,
        renderer::RenderPassStage::SHADER,
        ShouldRenderHalfRes() ? renderer::LoadOperation::LOAD : renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE
    );

    attachment->SetBinding(0);

    if (m_blend_function != BlendFunction::None()) {
        attachment->SetAllowBlending(true);
    }

    DeferCreate(attachment, g_engine->GetGPUDevice());
    
    m_framebuffer->AddAttachment(attachment);

    DeferCreate(m_framebuffer, g_engine->GetGPUDevice());
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
            : GetAttachment(0)->GetImageView()
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
    DescriptorTableRef descriptor_table = MakeRenderObject<DescriptorTable>(descriptor_table_decl);

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        const DescriptorSetRef &descriptor_set = descriptor_table->GetDescriptorSet(NAME("RenderTextureToScreenDescriptorSet"), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(NAME("InTexture"), GetPreviousFrameColorImageView());
    }

    DeferCreate(descriptor_table, g_engine->GetGPUDevice());

    m_render_texture_to_screen_pass = MakeUnique<FullScreenPass>(
        render_texture_to_screen_shader,
        std::move(descriptor_table),
        m_image_format,
        m_extent
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

        merge_half_res_textures_uniform_buffer = MakeRenderObject<GPUBuffer>(GPUBufferType::CONSTANT_BUFFER);
        HYPERION_ASSERT_RESULT(merge_half_res_textures_uniform_buffer->Create(g_engine->GetGPUDevice(), sizeof(uniforms)));

        merge_half_res_textures_uniform_buffer->Copy(g_engine->GetGPUDevice(), sizeof(uniforms), &uniforms);
    }

    ShaderRef merge_half_res_textures_shader = g_shader_manager->GetOrCreate(NAME("MergeHalfResTextures"));
    AssertThrow(merge_half_res_textures_shader.IsValid());

    const DescriptorTableDeclaration descriptor_table_decl = merge_half_res_textures_shader->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable();
    DescriptorTableRef descriptor_table = MakeRenderObject<DescriptorTable>(descriptor_table_decl);

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        const DescriptorSetRef &descriptor_set = descriptor_table->GetDescriptorSet(NAME("MergeHalfResTexturesDescriptorSet"), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(NAME("InTexture"), GetAttachment(0)->GetImageView());
        descriptor_set->SetElement(NAME("UniformBuffer"), merge_half_res_textures_uniform_buffer);
    }

    DeferCreate(descriptor_table, g_engine->GetGPUDevice());

    m_merge_half_res_textures_pass = MakeUnique<FullScreenPass>(
        merge_half_res_textures_shader,
        std::move(descriptor_table),
        m_image_format,
        m_extent
    );

    m_merge_half_res_textures_pass->Create();
}

void FullScreenPass::CreateDescriptors()
{
}

void FullScreenPass::Record(uint32 frame_index)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    const SceneRenderResource *scene_render_resource = g_engine->GetRenderState()->GetActiveScene();
    const CameraRenderResource *camera_render_resource = &g_engine->GetRenderState()->GetActiveCamera();
    const TResourceHandle<EnvProbeRenderResource> &env_probe_render_resource = g_engine->GetRenderState()->GetActiveEnvProbe();
    EnvGrid *env_grid = g_engine->GetRenderState()->GetActiveEnvGrid();

    const CommandBufferRef &command_buffer = m_command_buffers[frame_index];

    command_buffer->Begin(g_engine->GetGPUDevice(), m_render_group->GetPipeline()->GetRenderPass());

    m_render_group->GetPipeline()->SetPushConstants(m_push_constant_data.Data(), m_push_constant_data.Size());
    
    if (ShouldRenderHalfRes()) {
        const Vec2i viewport_offset = (Vec2i(m_framebuffer->GetExtent().x, 0) / 2) * (g_engine->GetRenderState()->frame_counter & 1);
        const Vec2i viewport_extent = Vec2i(m_framebuffer->GetExtent().x / 2, m_framebuffer->GetExtent().y);

        m_render_group->GetPipeline()->Bind(command_buffer, viewport_offset, viewport_extent);
    } else {
        m_render_group->GetPipeline()->Bind(command_buffer);
    }

    m_render_group->GetPipeline()->GetDescriptorTable()->Bind<GraphicsPipelineRef>(
        command_buffer,
        frame_index,
        m_render_group->GetPipeline(),
        {
            {
                NAME("Scene"),
                {
                    { NAME("ScenesBuffer"), ShaderDataOffset<SceneShaderData>(scene_render_resource, 0) },
                    { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(camera_render_resource, 0) },
                    { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(env_grid ? env_grid->GetComponentIndex() : 0) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(env_probe_render_resource ? env_probe_render_resource->GetBufferIndex() : 0) }
                }
            }
        }
    );

    m_full_screen_quad->GetRenderResource().Render(command_buffer);

    command_buffer->End(g_engine->GetGPUDevice());
}

void FullScreenPass::RenderPreviousTextureToScreen(Frame *frame)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    const uint32 frame_index = frame->GetFrameIndex();

    const SceneRenderResource *scene_render_resource = g_engine->GetRenderState()->GetActiveScene();
    const CameraRenderResource *camera_render_resource = &g_engine->GetRenderState()->GetActiveCamera();

    AssertThrow(m_render_texture_to_screen_pass != nullptr);

    m_render_texture_to_screen_pass->GetCommandBuffer(frame_index)->Record(
        g_engine->GetGPUInstance()->GetDevice(),
        m_render_group->GetPipeline()->GetRenderPass(),
        [&](CommandBuffer *cmd)
        {
            if (ShouldRenderHalfRes()) {
                const Vec2i viewport_offset = (Vec2i(m_framebuffer->GetExtent().x, 0) / 2) * (g_engine->GetRenderState()->frame_counter & 1);
                const Vec2i viewport_extent = Vec2i(m_framebuffer->GetExtent().x / 2, m_framebuffer->GetExtent().y);

                // render previous frame's result to screen
                m_render_texture_to_screen_pass->GetRenderGroup()->GetPipeline()->Bind(cmd, viewport_offset, viewport_extent);
            } else {
                // render previous frame's result to screen
                m_render_texture_to_screen_pass->GetRenderGroup()->GetPipeline()->Bind(cmd);
            }
            
            m_render_texture_to_screen_pass->GetRenderGroup()->GetPipeline()->GetDescriptorTable()->Bind<GraphicsPipelineRef>(
                cmd,
                frame_index,
                m_render_texture_to_screen_pass->GetRenderGroup()->GetPipeline(),
                {
                    {
                        NAME("Scene"),
                        {
                            { NAME("ScenesBuffer"), ShaderDataOffset<SceneShaderData>(scene_render_resource, 0) },
                            { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(camera_render_resource, 0) }
                        }
                    }
                }
            );

            m_full_screen_quad->GetRenderResource().Render(cmd);

            HYPERION_RETURN_OK;
        });

    HYPERION_ASSERT_RESULT(m_render_texture_to_screen_pass->GetCommandBuffer(frame_index)->SubmitSecondary(frame->GetCommandBuffer()));
}

void FullScreenPass::CopyResultToPreviousTexture(Frame *frame)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    const uint32 frame_index = frame->GetFrameIndex();

    AssertThrow(m_previous_texture.IsValid());

    const ImageRef &src_image = m_framebuffer->GetAttachment(0)->GetImage();
    const ImageRef &dst_image = m_previous_texture->GetImage();

    src_image->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::COPY_SRC);
    dst_image->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::COPY_DST);

    dst_image->Blit(
        frame->GetCommandBuffer(),
        src_image
    );

    src_image->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::SHADER_RESOURCE);
    dst_image->InsertBarrier(frame->GetCommandBuffer(), renderer::ResourceState::SHADER_RESOURCE);
}

void FullScreenPass::MergeHalfResTextures(Frame *frame)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    const uint32 frame_index = frame->GetFrameIndex();

    const SceneRenderResource *scene_render_resource = g_engine->GetRenderState()->GetActiveScene();
    const CameraRenderResource *camera_render_resource = &g_engine->GetRenderState()->GetActiveCamera();

    AssertThrow(m_merge_half_res_textures_pass != nullptr);

    m_merge_half_res_textures_pass->Record(frame_index);
    m_merge_half_res_textures_pass->Render(frame);
}

void FullScreenPass::Render(Frame *frame)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    const uint32 frame_index = frame->GetFrameIndex();

    m_framebuffer->BeginCapture(frame->GetCommandBuffer(), frame_index);

    RenderToFramebuffer(frame, m_framebuffer);

    m_framebuffer->EndCapture(frame->GetCommandBuffer(), frame_index);

    if (ShouldRenderHalfRes()) {
        MergeHalfResTextures(frame);
    }

    if (UsesTemporalBlending()) {
        if (!ShouldRenderHalfRes()) {
            CopyResultToPreviousTexture(frame);
        }

        m_temporal_blending->Render(frame);
    }
}

void FullScreenPass::RenderToFramebuffer(Frame *frame, const FramebufferRef &framebuffer)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    AssertThrow(framebuffer->IsCapturing());

    // render previous frame's result to screen
    if (!m_is_first_frame && m_render_texture_to_screen_pass != nullptr) {
        RenderPreviousTextureToScreen(frame);
    }

    const CommandBufferRef &secondary_command_buffer = m_command_buffers[frame->GetFrameIndex()];
    HYPERION_ASSERT_RESULT(secondary_command_buffer->SubmitSecondary(frame->GetCommandBuffer()));

    m_is_first_frame = false;
}

void FullScreenPass::Begin(Frame *frame)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    const uint32 frame_index = frame->GetFrameIndex();

    const CommandBufferRef &command_buffer = m_command_buffers[frame_index];

    command_buffer->Begin(g_engine->GetGPUDevice(), m_render_group->GetPipeline()->GetRenderPass());

    if (ShouldRenderHalfRes()) {
        const Vec2i viewport_offset = (Vec2i(m_framebuffer->GetExtent().x, 0) / 2) * (g_engine->GetRenderState()->frame_counter & 1);
        const Vec2i viewport_extent = Vec2i(m_framebuffer->GetExtent().x / 2, m_framebuffer->GetExtent().y);

        m_render_group->GetPipeline()->Bind(command_buffer, viewport_offset, viewport_extent);
    } else {
        m_render_group->GetPipeline()->Bind(command_buffer);
    }
}

void FullScreenPass::End(Frame *frame)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    const uint32 frame_index = frame->GetFrameIndex();

    const CommandBufferRef &command_buffer = m_command_buffers[frame_index];
    command_buffer->End(g_engine->GetGPUDevice());

    m_framebuffer->BeginCapture(frame->GetCommandBuffer(), frame_index);

    // render previous frame's result to screen
    if (!m_is_first_frame && m_render_texture_to_screen_pass != nullptr) {
        RenderPreviousTextureToScreen(frame);
    }
    
    HYPERION_ASSERT_RESULT(command_buffer->SubmitSecondary(frame->GetCommandBuffer()));

    m_framebuffer->EndCapture(frame->GetCommandBuffer(), frame_index);

    if (ShouldRenderHalfRes()) {
        MergeHalfResTextures(frame);
    }

    if (UsesTemporalBlending()) {
        if (!ShouldRenderHalfRes()) {
            CopyResultToPreviousTexture(frame);
        }

        m_temporal_blending->Render(frame);
    }

    m_is_first_frame = false;
}

} // namespace hyperion
