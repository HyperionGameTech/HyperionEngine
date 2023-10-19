#include "./FullScreenPass.hpp"
#include <Engine.hpp>
#include <Types.hpp>

#include <asset/ByteReader.hpp>
#include <util/fs/FsUtil.hpp>
#include <util/MeshBuilder.hpp>

namespace hyperion::v2 {

using renderer::VertexAttribute;
using renderer::VertexAttributeSet;
using renderer::Descriptor;
using renderer::DescriptorSet;
using renderer::ImageDescriptor;
using renderer::ImageSamplerDescriptor;
using renderer::CommandBufferType;
using renderer::FillMode;

#pragma region Render commands

struct RENDER_COMMAND(CreateCommandBuffers) : renderer::RenderCommand
{
    FixedArray<CommandBufferRef, max_frames_in_flight> command_buffers;

    RENDER_COMMAND(CreateCommandBuffers)(const FixedArray<CommandBufferRef, max_frames_in_flight> &command_buffers)
        : command_buffers(command_buffers)
    {
    }

    virtual Result operator()()
    {
        for (UInt i = 0; i < max_frames_in_flight; i++) {
            HYPERION_BUBBLE_ERRORS(command_buffers[i]->Create(
                g_engine->GetGPUDevice(),
                g_engine->GetGPUInstance()->GetGraphicsCommandPool()
            ));
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion

FullScreenPass::FullScreenPass(InternalFormat image_format, Extent2D extent)
    : FullScreenPass(Handle<Shader>(), image_format, extent)
{
}

FullScreenPass::FullScreenPass(
    const Handle<Shader> &shader,
    InternalFormat image_format,
    Extent2D extent
) : FullScreenPass(
        shader,
        DescriptorKey::UNUSED,
        ~0u,
        image_format,
        extent
    )
{
}

FullScreenPass::FullScreenPass(
    const Handle<Shader> &shader,
    const Array<DescriptorSetRef> &used_descriptor_sets,
    InternalFormat image_format,
    Extent2D extent
) : FullScreenPass(
        shader,
        DescriptorKey::UNUSED,
        ~0u,
        image_format,
        extent
    )
{
    m_used_descriptor_sets.Set(used_descriptor_sets);
}

FullScreenPass::FullScreenPass(
    const Handle<Shader> &shader,
    DescriptorKey descriptor_key,
    UInt sub_descriptor_index,
    InternalFormat image_format,
    Extent2D extent
) : m_shader(shader),
    m_descriptor_key(descriptor_key),
    m_sub_descriptor_index(sub_descriptor_index),
    m_image_format(image_format),
    m_extent(extent)
{
}

FullScreenPass::~FullScreenPass() = default;

void FullScreenPass::Create()
{
    InitObject(m_shader);

    CreateQuad();
    CreateCommandBuffers();
    CreateFramebuffer();
    CreatePipeline();
    CreateDescriptors();
    
    // HYP_SYNC_RENDER();
}

void FullScreenPass::SetShader(const Handle<Shader> &shader)
{
    if (m_shader == shader) {
        return;
    }

    m_shader = std::move(shader);
}

void FullScreenPass::CreateQuad()
{
    m_full_screen_quad = MeshBuilder::Quad();
    InitObject(m_full_screen_quad);
}

void FullScreenPass::CreateCommandBuffers()
{
    for (UInt i = 0; i < max_frames_in_flight; i++) {
        m_command_buffers[i] = MakeRenderObject<CommandBuffer>(CommandBufferType::COMMAND_BUFFER_SECONDARY);
    }

    // create command buffers in render thread
    PUSH_RENDER_COMMAND(CreateCommandBuffers, m_command_buffers);
}

void FullScreenPass::CreateFramebuffer()
{
    if (m_extent.Size() == 0) {
        // TODO: Make non render-thread dependent
        m_extent = g_engine->GetGPUInstance()->GetSwapchain()->extent;
    }

    m_framebuffer = CreateObject<Framebuffer>(
        m_extent,
        renderer::RenderPassStage::SHADER,
        renderer::RenderPassMode::RENDER_PASS_SECONDARY_COMMAND_BUFFER
    );

    renderer::AttachmentUsage *attachment_usage;

    m_attachments.PushBack(MakeRenderObject<renderer::Attachment>(
        MakeRenderObject<Image>(renderer::FramebufferImage2D(
            m_extent,
            m_image_format,
            nullptr
        )),
        renderer::RenderPassStage::SHADER
    ));

    HYPERION_ASSERT_RESULT(m_attachments.Back()->AddAttachmentUsage(
        g_engine->GetGPUInstance()->GetDevice(),
        renderer::LoadOperation::CLEAR,
        renderer::StoreOperation::STORE,
        &attachment_usage
    ));

    m_framebuffer->AddAttachmentUsage(attachment_usage);

    for (auto &attachment : m_attachments) {
        HYPERION_ASSERT_RESULT(attachment->Create(g_engine->GetGPUInstance()->GetDevice()));
    }
    
    InitObject(m_framebuffer);
}

void FullScreenPass::CreatePipeline()
{
    CreatePipeline(RenderableAttributeSet(
        MeshAttributes {
            .vertex_attributes = renderer::static_mesh_vertex_attributes
        },
        MaterialAttributes {
            .bucket = Bucket::BUCKET_INTERNAL,
            .fill_mode = FillMode::FILL,
            .flags = MaterialAttributes::RENDERABLE_ATTRIBUTE_FLAGS_NONE
        }
    ));
}

void FullScreenPass::CreatePipeline(const RenderableAttributeSet &renderable_attributes)
{
    if (m_used_descriptor_sets.HasValue()) {
        m_render_group = CreateObject<RenderGroup>(
            Handle<Shader>(m_shader),
            renderable_attributes,
            m_used_descriptor_sets.Get()
        );
    } else {
        m_render_group = CreateObject<RenderGroup>(
            Handle<Shader>(m_shader),
            renderable_attributes
        );
    }

    m_render_group->AddFramebuffer(Handle<Framebuffer>(m_framebuffer));

    g_engine->AddRenderGroup(m_render_group);
    InitObject(m_render_group);
}

void FullScreenPass::CreateDescriptors()
{
}

void FullScreenPass::Destroy()
{
    // TODO: Move all attachment ops into render thread
    for (auto &attachment : m_attachments) {
        m_framebuffer->RemoveAttachmentUsage(attachment);
    }

    SafeRelease(std::move(m_attachments));

    if (m_render_group) {
        m_render_group->RemoveFramebuffer(m_framebuffer->GetID());
    }

    m_framebuffer.Reset();
    m_render_group.Reset();
    m_full_screen_quad.Reset();

    SafeRelease(std::move(m_command_buffers));

    HYP_SYNC_RENDER();
}

void FullScreenPass::Record(UInt frame_index)
{
    Threads::AssertOnThread(THREAD_RENDER);

    auto *command_buffer = m_command_buffers[frame_index].Get();

    auto record_result = command_buffer->Record(
        g_engine->GetGPUInstance()->GetDevice(),
        m_render_group->GetPipeline()->GetConstructionInfo().render_pass,
        [this, frame_index](CommandBuffer *cmd) {
            m_render_group->GetPipeline()->push_constants = m_push_constant_data;
            m_render_group->GetPipeline()->Bind(cmd);

            cmd->BindDescriptorSet(
                g_engine->GetGPUInstance()->GetDescriptorPool(),
                m_render_group->GetPipeline(),
                DescriptorSet::global_buffer_mapping[frame_index],
                DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL
            );

            cmd->BindDescriptorSet(
                g_engine->GetGPUInstance()->GetDescriptorPool(),
                m_render_group->GetPipeline(),
                DescriptorSet::scene_buffer_mapping[frame_index],
                DescriptorSet::DESCRIPTOR_SET_INDEX_SCENE,
                FixedArray {
                    HYP_RENDER_OBJECT_OFFSET(Scene, g_engine->GetRenderState().GetScene().id.ToIndex()),
                    HYP_RENDER_OBJECT_OFFSET(Light, 0),
                    HYP_RENDER_OBJECT_OFFSET(EnvGrid, g_engine->GetRenderState().bound_env_grid.ToIndex()),
                    HYP_RENDER_OBJECT_OFFSET(EnvProbe, g_engine->GetRenderState().GetActiveEnvProbe().ToIndex()),
                    HYP_RENDER_OBJECT_OFFSET(Camera, g_engine->GetRenderState().GetCamera().id.ToIndex())
                }
            );
            
#if HYP_FEATURES_BINDLESS_TEXTURES
            cmd->BindDescriptorSet(
                g_engine->GetGPUInstance()->GetDescriptorPool(),
                m_render_group->GetPipeline(),
                DescriptorSet::bindless_textures_mapping[frame_index],
                DescriptorSet::DESCRIPTOR_SET_INDEX_BINDLESS
            );
#else
            cmd->BindDescriptorSet(
                g_engine->GetGPUInstance()->GetDescriptorPool(),
                m_render_group->GetPipeline(),
                DescriptorSet::DESCRIPTOR_SET_INDEX_MATERIAL_TEXTURES
            );
#endif

            cmd->BindDescriptorSet(
                g_engine->GetGPUInstance()->GetDescriptorPool(),
                m_render_group->GetPipeline(),
                DescriptorSet::DESCRIPTOR_SET_INDEX_VOXELIZER
            );
            
#if HYP_FEATURES_ENABLE_RAYTRACING && HYP_FEATURES_BINDLESS_TEXTURES
          //  if (!g_engine->GetGPUDevice()->GetFeatures().IsRaytracingDisabled()) {
                cmd->BindDescriptorSet(
                    g_engine->GetGPUInstance()->GetDescriptorPool(),
                    m_render_group->GetPipeline(),
                    DescriptorSet::DESCRIPTOR_SET_INDEX_RAYTRACING,
                    DescriptorSet::DESCRIPTOR_SET_INDEX_RAYTRACING
                );
         //   }
#endif

            m_full_screen_quad->Render(cmd);

            HYPERION_RETURN_OK;
        });

    HYPERION_ASSERT_RESULT(record_result);
}

void FullScreenPass::Render(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    const auto frame_index = frame->GetFrameIndex();

    m_framebuffer->BeginCapture(frame_index, frame->GetCommandBuffer());

    auto *secondary_command_buffer = m_command_buffers[frame_index].Get();
    HYPERION_ASSERT_RESULT(secondary_command_buffer->SubmitSecondary(frame->GetCommandBuffer()));

    m_framebuffer->EndCapture(frame_index, frame->GetCommandBuffer());
}

void FullScreenPass::Begin(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    const UInt frame_index = frame->GetFrameIndex();

    auto *command_buffer = m_command_buffers[frame_index].Get();

    command_buffer->Begin(g_engine->GetGPUDevice(), m_render_group->GetPipeline()->GetConstructionInfo().render_pass);

    m_render_group->GetPipeline()->Bind(command_buffer);
}

void FullScreenPass::End(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    const UInt frame_index = frame->GetFrameIndex();

    auto *command_buffer = m_command_buffers[frame_index].Get();
    command_buffer->End(g_engine->GetGPUDevice());

    m_framebuffer->BeginCapture(frame_index, frame->GetCommandBuffer());
    HYPERION_ASSERT_RESULT(command_buffer->SubmitSecondary(frame->GetCommandBuffer()));
    m_framebuffer->EndCapture(frame_index, frame->GetCommandBuffer());
}

} // namespace hyperion::v2
