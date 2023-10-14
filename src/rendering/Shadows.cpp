#include "Shadows.hpp"
#include <Engine.hpp>
#include <rendering/RenderEnvironment.hpp>

#include <scene/camera/OrthoCamera.hpp>

#include <asset/ByteReader.hpp>
#include <util/fs/FsUtil.hpp>

namespace hyperion::v2 {

using renderer::DescriptorKey;
using renderer::Image;
using renderer::ImageDescriptor;
using renderer::ImageSamplerDescriptor;
using renderer::SamplerDescriptor;
using renderer::StorageImageDescriptor;
using renderer::StorageImage2D;

#pragma region Render commands

struct RENDER_COMMAND(CreateShadowMapDescriptors) : RenderCommand
{
    SizeType shadow_map_index;
    ImageViewRef shadow_map_image_view;

    RENDER_COMMAND(CreateShadowMapDescriptors)(
        SizeType shadow_map_index,
        const ImageViewRef &shadow_map_image_view
    ) : shadow_map_index(shadow_map_index),
        shadow_map_image_view(shadow_map_image_view)
    {
    }

    virtual Result operator()()
    {
        for (UInt i = 0; i < max_frames_in_flight; i++) {
            auto *descriptor_set = g_engine->GetGPUInstance()->GetDescriptorPool()
                .GetDescriptorSet(DescriptorSet::scene_buffer_mapping[i]);

            auto *shadow_map_descriptor = descriptor_set
                ->GetOrAddDescriptor<ImageDescriptor>(DescriptorKey::SHADOW_MAPS);

            shadow_map_descriptor->SetElementSRV(
                UInt(shadow_map_index),
                shadow_map_image_view
            );
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateShadowMapImage) : RenderCommand
{
    ImageRef shadow_map_image;
    ImageViewRef shadow_map_image_view;

    RENDER_COMMAND(CreateShadowMapImage)(const ImageRef &shadow_map_image, const ImageViewRef &shadow_map_image_view)
        : shadow_map_image(shadow_map_image),
          shadow_map_image_view(shadow_map_image_view)
    {
    }

    virtual Result operator()()
    {
        HYPERION_BUBBLE_ERRORS(shadow_map_image->Create(g_engine->GetGPUDevice()));
        HYPERION_BUBBLE_ERRORS(shadow_map_image_view->Create(g_engine->GetGPUDevice(), shadow_map_image));

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateShadowMapBlurDescriptorSets) : RenderCommand
{
    renderer::DescriptorSet *descriptor_sets;

    RENDER_COMMAND(CreateShadowMapBlurDescriptorSets)(renderer::DescriptorSet *descriptor_sets)
        : descriptor_sets(descriptor_sets)
    {
    }

    virtual Result operator()()
    {
        for (UInt i = 0; i < max_frames_in_flight; i++) {
            HYPERION_BUBBLE_ERRORS(descriptor_sets[i].Create(
                g_engine->GetGPUDevice(),
                &g_engine->GetGPUInstance()->GetDescriptorPool()
            ));
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(DestroyShadowPassData) : RenderCommand
{
    ImageRef shadow_map_image;
    ImageViewRef shadow_map_image_view;
    renderer::DescriptorSet *descriptor_sets;

    RENDER_COMMAND(DestroyShadowPassData)(const ImageRef &shadow_map_image, const ImageViewRef &shadow_map_image_view, renderer::DescriptorSet *descriptor_sets)
        : shadow_map_image(shadow_map_image),
          shadow_map_image_view(shadow_map_image_view),
          descriptor_sets(descriptor_sets)
    {
    }

    virtual Result operator()()
    {
        auto result = Result::OK;

        HYPERION_PASS_ERRORS(shadow_map_image->Destroy(g_engine->GetGPUDevice()), result);
        HYPERION_PASS_ERRORS(shadow_map_image_view->Destroy(g_engine->GetGPUDevice()), result);

        for (UInt i = 0; i < max_frames_in_flight; i++) {
            HYPERION_PASS_ERRORS(descriptor_sets[i].Destroy(g_engine->GetGPUDevice()), result);
        }

        return result;
    }
};

struct RENDER_COMMAND(UpdateShadowMapRenderData) : RenderCommand
{
    UInt shadow_map_index;
    Matrix4 view_matrix;
    Matrix4 projection_matrix;
    BoundingBox aabb;
    Extent2D dimensions;
    ShadowFlags flags;

    RENDER_COMMAND(UpdateShadowMapRenderData)(
        UInt shadow_map_index,
        const Matrix4 &view_matrix,
        const Matrix4 &projection_matrix,
        const BoundingBox &aabb,
        Extent2D dimensions,
        ShadowFlags flags
    ) : shadow_map_index(shadow_map_index),
        view_matrix(view_matrix),
        projection_matrix(projection_matrix),
        aabb(aabb),
        dimensions(dimensions),
        flags(flags)
    {
    }

    virtual Result operator()() override
    {
        
        g_engine->GetRenderData()->shadow_map_data.Set(
            shadow_map_index,
            {
                .projection = projection_matrix,
                .view = view_matrix,
                .aabb_max = Vector4(aabb.max, 1.0f),
                .aabb_min = Vector4(aabb.min, 1.0f),
                .dimensions = dimensions,
                .flags = UInt32(flags)
            }
        );

        HYPERION_RETURN_OK;
    }
};

#pragma endregion

ShadowPass::ShadowPass(const Handle<Scene> &parent_scene)
    : FullScreenPass(),
      m_parent_scene(parent_scene),
      m_shadow_mode(ShadowMode::PCF),
      m_shadow_map_index(~0u),
      m_dimensions { 2048, 2048 }
{
}

ShadowPass::~ShadowPass() = default;

void ShadowPass::CreateShader()
{
    ShaderProperties properties;
    properties.SetRequiredVertexAttributes(renderer::static_mesh_vertex_attributes);

    switch (m_shadow_mode) {
    case ShadowMode::VSM:
        properties.Set("MODE_VSM");
        break;
    case ShadowMode::CONTACT_HARDENED:
        properties.Set("MODE_CONTACT_HARDENED");
        break;
    case ShadowMode::PCF:
        properties.Set("MODE_PCF");
        break;
    case ShadowMode::STANDARD: // fallthrough
    default:
        properties.Set("MODE_STANDARD");
        break;
    }

    m_shader = g_shader_manager->GetOrCreate(
        HYP_NAME(Shadows),
        properties
    );

    InitObject(m_shader);
}

void ShadowPass::CreateFramebuffer()
{
    /* Add the filters' renderpass */
    m_framebuffer = CreateObject<Framebuffer>(
        m_dimensions,
        renderer::RenderPassStage::SHADER,
        renderer::RenderPass::Mode::RENDER_PASS_SECONDARY_COMMAND_BUFFER
    );

    AttachmentUsage *attachment_usage;

    { // depth, depth^2 texture (for variance shadow map)
        m_attachments.PushBack(RenderObjects::Make<renderer::Attachment>(
            RenderObjects::Make<Image>(renderer::FramebufferImage2D(
                m_dimensions,
                InternalFormat::RG32F,
                FilterMode::TEXTURE_FILTER_NEAREST
            )),
            RenderPassStage::SHADER
        ));

        HYPERION_ASSERT_RESULT(m_attachments.Back()->AddAttachmentUsage(
            g_engine->GetGPUInstance()->GetDevice(),
            renderer::LoadOperation::CLEAR,
            renderer::StoreOperation::STORE,
            &attachment_usage
        ));

        m_framebuffer->AddAttachmentUsage(attachment_usage);
    }

    { // standard depth texture
        m_attachments.PushBack(RenderObjects::Make<renderer::Attachment>(
            RenderObjects::Make<Image>(renderer::FramebufferImage2D(
                m_dimensions,
                g_engine->GetDefaultFormat(TEXTURE_FORMAT_DEFAULT_DEPTH),
                nullptr
            )),
            RenderPassStage::SHADER
        ));

        HYPERION_ASSERT_RESULT(m_attachments.Back()->AddAttachmentUsage(
            g_engine->GetGPUInstance()->GetDevice(),
            renderer::LoadOperation::CLEAR,
            renderer::StoreOperation::STORE,
            &attachment_usage
        ));

        m_framebuffer->AddAttachmentUsage(attachment_usage);
    }

    // should be created in render thread
    for (auto &attachment : m_attachments) {
        HYPERION_ASSERT_RESULT(attachment->Create(g_engine->GetGPUInstance()->GetDevice()));
    }

    InitObject(m_framebuffer);
}

void ShadowPass::CreateDescriptors()
{
    AssertThrow(m_shadow_map_index != ~0u);

    RenderCommands::Push<RENDER_COMMAND(CreateShadowMapDescriptors)>(
        m_shadow_map_index,
        m_shadow_map_image_view
    );
}

void ShadowPass::CreateShadowMap()
{
    m_shadow_map_image = RenderObjects::Make<Image>(StorageImage2D(
        m_dimensions,
        InternalFormat::RG32F
    ));

    m_shadow_map_image_view = RenderObjects::Make<ImageView>();

    RenderCommands::Push<RENDER_COMMAND(CreateShadowMapImage)>(m_shadow_map_image, m_shadow_map_image_view);
}

void ShadowPass::CreateComputePipelines()
{
    // have to create descriptor sets specifically for compute shader,
    // holding framebuffer attachment image (src), and our final shadowmap image (dst)
    for (UInt i = 0; i < max_frames_in_flight; i++) {
        m_blur_descriptor_sets[i].AddDescriptor<ImageDescriptor>(0)
            ->SetElementSRV(0, m_framebuffer->GetAttachmentUsages().front()->GetImageView());

        m_blur_descriptor_sets[i].AddDescriptor<SamplerDescriptor>(1)
            ->SetElementSampler(0, &g_engine->GetPlaceholderData().GetSamplerLinear());

        m_blur_descriptor_sets[i].AddDescriptor<StorageImageDescriptor>(2)
            ->SetElementUAV(0, m_shadow_map_image_view);
    }

    RenderCommands::Push<RENDER_COMMAND(CreateShadowMapBlurDescriptorSets)>(m_blur_descriptor_sets.Data());

    m_blur_shadow_map = CreateObject<ComputePipeline>(
        g_shader_manager->GetOrCreate(HYP_NAME(BlurShadowMap)),
        Array<const DescriptorSet *> { &m_blur_descriptor_sets[0] }
    );

    InitObject(m_blur_shadow_map);
}

void ShadowPass::Create()
{
    CreateShadowMap();
    CreateShader();
    CreateFramebuffer();
    CreateDescriptors();
    CreateComputePipelines();

    {
        m_camera = CreateObject<Camera>(m_dimensions.width, m_dimensions.height);

        m_camera->SetCameraController(UniquePtr<OrthoCameraController>::Construct());
        m_camera->SetFramebuffer(m_framebuffer);

        InitObject(m_camera);

        m_render_list.SetCamera(m_camera);
    }

    CreateCommandBuffers();

    HYP_SYNC_RENDER(); // force init stuff
}

void ShadowPass::Destroy()
{
    m_camera.Reset();
    m_render_list.Reset();
    m_parent_scene.Reset();

    RenderCommands::Push<RENDER_COMMAND(DestroyShadowPassData)>(
        m_shadow_map_image,
        m_shadow_map_image_view,
        m_blur_descriptor_sets.Data()
    );

    FullScreenPass::Destroy(); // flushes render queue, releases command buffers...
}

void ShadowPass::Render(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    Image *framebuffer_image = m_attachments.Front()->GetImage();

    if (framebuffer_image == nullptr) {
        return;
    }

    CommandBuffer *command_buffer = frame->GetCommandBuffer();

    AssertThrow(m_parent_scene.IsValid());

    g_engine->GetRenderState().BindScene(m_parent_scene.Get());

    m_render_list.CollectDrawCalls(
        frame,
        Bitset((1 << BUCKET_OPAQUE)),
        nullptr
    );

    m_render_list.ExecuteDrawCalls(
        frame,
        Bitset((1 << BUCKET_OPAQUE)),
        nullptr
    );

    g_engine->GetRenderState().UnbindScene();

    if (m_shadow_mode == ShadowMode::VSM) {
        // blur the image using compute shader
        m_blur_shadow_map->GetPipeline()->Bind(
            command_buffer,
            Pipeline::PushConstantData {
                .blur_shadow_map_data = {
                    .image_dimensions = ShaderVec2<UInt32>(m_framebuffer->GetExtent())
                }
            }
        );

        // bind descriptor set containing info needed to blur
        command_buffer->BindDescriptorSet(
            g_engine->GetGPUInstance()->GetDescriptorPool(),
            m_blur_shadow_map->GetPipeline(),
            &m_blur_descriptor_sets[frame->GetFrameIndex()],
            DescriptorSet::Index(0)
        );

        // put our shadow map in a state for writing
        m_shadow_map_image->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::UNORDERED_ACCESS);

        m_blur_shadow_map->GetPipeline()->Dispatch(
            command_buffer,
            Extent3D {
                (m_framebuffer->GetExtent().width + 7) / 8,
                (m_framebuffer->GetExtent().height + 7) / 8,
                1
            }
        );

        // put shadow map back into readable state
        m_shadow_map_image->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);
    } else {
        framebuffer_image->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::COPY_SRC);
        m_shadow_map_image->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::COPY_DST);

        m_shadow_map_image->Blit(
            command_buffer,
            framebuffer_image
        );

        framebuffer_image->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);
        m_shadow_map_image->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);
    }
}

ShadowMapRenderer::ShadowMapRenderer()
    : RenderComponent()
{
}

ShadowMapRenderer::~ShadowMapRenderer()
{
    if (m_shadow_pass) {
        m_shadow_pass->Destroy(); // flushes render queue
        m_shadow_pass.Reset();
    }
}

// called from render thread
void ShadowMapRenderer::Init()
{
    AssertThrow(IsValidComponent());

    m_shadow_pass.Reset(new ShadowPass(Handle<Scene>(GetParent()->GetScene()->GetID())));

    m_shadow_pass->SetShadowMapIndex(GetComponentIndex());
    m_shadow_pass->Create();
}

// called from game thread
void ShadowMapRenderer::InitGame()
{
    Threads::AssertOnThread(THREAD_GAME);
}

void ShadowMapRenderer::OnUpdate(GameCounter::TickUnit delta)
{
    Threads::AssertOnThread(THREAD_GAME);

    AssertThrow(m_shadow_pass != nullptr);
    AssertThrow(m_shadow_pass->GetCamera().IsValid());
    AssertThrow(m_shadow_pass->GetShader().IsValid());

    m_shadow_pass->GetCamera()->Update(delta);

    GetParent()->GetScene()->CollectEntities(
        m_shadow_pass->GetRenderList(),
        m_shadow_pass->GetCamera(),
        Bitset((1 << BUCKET_OPAQUE)),
        RenderableAttributeSet(
            MeshAttributes { },
            MaterialAttributes {
                .bucket = BUCKET_INTERNAL,
                .cull_faces = m_shadow_pass->GetShadowMode() == ShadowMode::VSM
                    ? FaceCullMode::BACK
                    : FaceCullMode::FRONT
            },
            m_shadow_pass->GetShader()->GetCompiledShader().GetDefinition()
        ),
        true // temp
    );
    
    m_shadow_pass->GetRenderList().UpdateRenderGroups();
}

void ShadowMapRenderer::OnRender(Frame *frame)
{
    Threads::AssertOnThread(THREAD_RENDER);

    AssertThrow(m_shadow_pass != nullptr);
    
    m_shadow_pass->Render(frame);
}

void ShadowMapRenderer::SetCameraData(const ShadowMapCameraData &camera_data)
{
    AssertThrow(m_shadow_pass != nullptr);

    ShadowFlags flags = SHADOW_FLAGS_NONE;

    switch (m_shadow_pass->GetShadowMode()) {
    case ShadowMode::VSM:
        flags |= SHADOW_FLAGS_VSM;
        break;
    case ShadowMode::CONTACT_HARDENED:
        flags |= SHADOW_FLAGS_CONTACT_HARDENED;
        break;
    case ShadowMode::PCF:
        flags |= SHADOW_FLAGS_PCF;
        break;
    default:
        break;
    }

    PUSH_RENDER_COMMAND(
        UpdateShadowMapRenderData,
        m_shadow_pass->GetShadowMapIndex(),
        camera_data.view,
        camera_data.projection,
        camera_data.aabb,
        m_shadow_pass->GetDimensions(),
        flags
    );
}

void ShadowMapRenderer::OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index /*prev_index*/)
{
    AssertThrowMsg(false, "Not implemented");
}

} // namespace hyperion::v2
