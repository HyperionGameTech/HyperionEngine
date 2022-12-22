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
    renderer::ImageView *shadow_map_image_view;

    RENDER_COMMAND(CreateShadowMapDescriptors)(
        SizeType shadow_map_index,
        renderer::ImageView *shadow_map_image_view
    ) : shadow_map_index(shadow_map_index),
        shadow_map_image_view(shadow_map_image_view)
    {
    }

    virtual Result operator()()
    {
        for (UInt i = 0; i < max_frames_in_flight; i++) {
            auto *descriptor_set = Engine::Get()->GetGPUInstance()->GetDescriptorPool()
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
    renderer::Image *shadow_map_image;
    renderer::ImageView *shadow_map_image_view;

    RENDER_COMMAND(CreateShadowMapImage)(renderer::Image *shadow_map_image, renderer::ImageView *shadow_map_image_view)
        : shadow_map_image(shadow_map_image),
          shadow_map_image_view(shadow_map_image_view)
    {
    }

    virtual Result operator()()
    {
        HYPERION_BUBBLE_ERRORS(shadow_map_image->Create(Engine::Get()->GetGPUDevice()));
        HYPERION_BUBBLE_ERRORS(shadow_map_image_view->Create(Engine::Get()->GetGPUDevice(), shadow_map_image));

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
                Engine::Get()->GetGPUDevice(),
                &Engine::Get()->GetGPUInstance()->GetDescriptorPool()
            ));
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(DestroyShadowPassData) : RenderCommand
{
    renderer::Image *shadow_map_image;
    renderer::ImageView *shadow_map_image_view;
    renderer::DescriptorSet *descriptor_sets;

    RENDER_COMMAND(DestroyShadowPassData)(renderer::Image *shadow_map_image, renderer::ImageView *shadow_map_image_view, renderer::DescriptorSet *descriptor_sets)
        : shadow_map_image(shadow_map_image),
          shadow_map_image_view(shadow_map_image_view),
          descriptor_sets(descriptor_sets)
    {
    }

    virtual Result operator()()
    {
        auto result = Result::OK;

        HYPERION_PASS_ERRORS(shadow_map_image->Destroy(Engine::Get()->GetGPUDevice()), result);
        HYPERION_PASS_ERRORS(shadow_map_image_view->Destroy(Engine::Get()->GetGPUDevice()), result);

        for (UInt i = 0; i < max_frames_in_flight; i++) {
            HYPERION_PASS_ERRORS(descriptor_sets[i].Destroy(Engine::Get()->GetGPUDevice()), result);
        }

        return result;
    }
};

#pragma endregion

ShadowPass::ShadowPass()
    : FullScreenPass(),
      m_shadow_mode(ShadowMode::CONTACT_HARDENED),
      m_max_distance(100.0f),
      m_shadow_map_index(~0u),
      m_dimensions { 2048, 2048 }
{
}

ShadowPass::~ShadowPass() = default;

void ShadowPass::CreateShader()
{
    m_shader = CreateObject<Shader>(Engine::Get()->GetShaderCompiler().GetCompiledShader(
        "Shadows",
        ShaderProps(renderer::static_mesh_vertex_attributes | renderer::skeleton_vertex_attributes)
    ));

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
        m_attachments.PushBack(std::make_unique<Attachment>(
            std::make_unique<renderer::FramebufferImage2D>(
                m_dimensions,
                InternalFormat::RG32F,
                FilterMode::TEXTURE_FILTER_NEAREST
            ),
            RenderPassStage::SHADER
        ));

        HYPERION_ASSERT_RESULT(m_attachments.Back()->AddAttachmentUsage(
            Engine::Get()->GetGPUInstance()->GetDevice(),
            renderer::LoadOperation::CLEAR,
            renderer::StoreOperation::STORE,
            &attachment_usage
        ));

        m_framebuffer->AddAttachmentUsage(attachment_usage);
    }

    { // standard depth texture
        m_attachments.PushBack(std::make_unique<Attachment>(
            std::make_unique<renderer::FramebufferImage2D>(
                m_dimensions,
                Engine::Get()->GetDefaultFormat(TEXTURE_FORMAT_DEFAULT_DEPTH),
                nullptr
            ),
            RenderPassStage::SHADER
        ));

        HYPERION_ASSERT_RESULT(m_attachments.Back()->AddAttachmentUsage(
            Engine::Get()->GetGPUInstance()->GetDevice(),
            renderer::LoadOperation::CLEAR,
            renderer::StoreOperation::STORE,
            &attachment_usage
        ));

        m_framebuffer->AddAttachmentUsage(attachment_usage);
    }

    // should be created in render thread
    for (auto &attachment : m_attachments) {
        HYPERION_ASSERT_RESULT(attachment->Create(Engine::Get()->GetGPUInstance()->GetDevice()));
    }

    InitObject(m_framebuffer);
}

void ShadowPass::CreateDescriptors()
{
    AssertThrow(m_shadow_map_index != ~0u);

    RenderCommands::Push<RENDER_COMMAND(CreateShadowMapDescriptors)>(
        m_shadow_map_index,
        m_shadow_map_image_view.get()
    );
}

void ShadowPass::CreateShadowMap()
{
    m_shadow_map_image = std::make_unique<StorageImage2D>(
        m_dimensions,
        InternalFormat::RG32F
    );

    m_shadow_map_image_view = std::make_unique<ImageView>();

    RenderCommands::Push<RENDER_COMMAND(CreateShadowMapImage)>(m_shadow_map_image.get(), m_shadow_map_image_view.get());
}

void ShadowPass::CreateComputePipelines()
{
    // have to create descriptor sets specifically for compute shader,
    // holding framebuffer attachment image (src), and our final shadowmap image (dst)
    for (UInt i = 0; i < max_frames_in_flight; i++) {
        m_blur_descriptor_sets[i].AddDescriptor<ImageDescriptor>(0)
            ->SetSubDescriptor({
                .element_index = 0u,
                .image_view = m_framebuffer->GetAttachmentUsages().front()->GetImageView()
            });

        m_blur_descriptor_sets[i].AddDescriptor<SamplerDescriptor>(1)
            ->SetSubDescriptor({
                .element_index = 0u,
                .sampler = &Engine::Get()->GetPlaceholderData().GetSamplerNearest()
            });

        m_blur_descriptor_sets[i].AddDescriptor<StorageImageDescriptor>(2)
            ->SetSubDescriptor({
                .element_index = 0u,
                .image_view = m_shadow_map_image_view.get()
            });
    }

    RenderCommands::Push<RENDER_COMMAND(CreateShadowMapBlurDescriptorSets)>(m_blur_descriptor_sets.Data());

    m_blur_shadow_map = CreateObject<ComputePipeline>(
        CreateObject<Shader>(Engine::Get()->GetShaderCompiler().GetCompiledShader("BlurShadowMap")),
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

    m_scene = CreateObject<Scene>(CreateObject<Camera>(m_dimensions.width, m_dimensions.height));

    m_scene->SetName("Shadow scene");
    m_scene->SetOverrideRenderableAttributes(RenderableAttributeSet(
        MeshAttributes { },
        MaterialAttributes {
            .bucket = BUCKET_SHADOW,
            .cull_faces = FaceCullMode::BACK
        },
        m_shader->GetID()
    ));

    m_scene->GetCamera()->SetCameraController(UniquePtr<OrthoCameraController>::Construct());
    m_scene->GetCamera()->SetFramebuffer(m_framebuffer);

    InitObject(m_scene);

    CreateCommandBuffers();

    HYP_SYNC_RENDER(); // force init stuff
}

void ShadowPass::Destroy()
{
    if (m_scene) {
        Engine::Get()->GetWorld()->RemoveScene(m_scene->GetID());
        m_scene.Reset();
    }

    RenderCommands::Push<RENDER_COMMAND(DestroyShadowPassData)>(
        m_shadow_map_image.get(),
        m_shadow_map_image_view.get(),
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

    auto *command_buffer = frame->GetCommandBuffer();

    m_scene->Render(frame);

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
            Engine::Get()->GetGPUInstance()->GetDescriptorPool(),
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

ShadowRenderer::ShadowRenderer(Handle<Light> &&light)
    : ShadowRenderer(std::move(light), Vector3::Zero(), 25.0f)
{
}

ShadowRenderer::ShadowRenderer(Handle<Light> &&light, const BoundingBox &aabb)
    : ShadowRenderer(std::move(light), aabb.GetCenter(), aabb.GetExtent().Max() * 0.5f)
{
}

ShadowRenderer::ShadowRenderer(Handle<Light> &&light, const Vector3 &origin, float max_distance)
    : EngineComponentBase(),
      RenderComponent()
{
    m_shadow_pass.SetLight(std::move(light));
    m_shadow_pass.SetOrigin(origin);
    m_shadow_pass.SetMaxDistance(max_distance);
}

ShadowRenderer::~ShadowRenderer()
{
    Teardown();
}

// called from render thread
void ShadowRenderer::Init()
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init();

    AssertThrow(IsValidComponent());
    m_shadow_pass.SetShadowMapIndex(GetComponentIndex());
    m_shadow_pass.Create();

    SetReady(true);

    OnTeardown([this]() {
        m_shadow_pass.Destroy(); // flushes render queue

        SetReady(false);
    });
}

// called from game thread
void ShadowRenderer::InitGame()
{
    Threads::AssertOnThread(THREAD_GAME);
    AssertReady();

    m_shadow_pass.GetScene()->SetParentScene(Handle<Scene>(GetParent()->GetScene()->GetID()));
    Engine::Get()->GetWorld()->AddScene(Handle<Scene>(m_shadow_pass.GetScene()));
}

void ShadowRenderer::OnUpdate(GameCounter::TickUnit delta)
{
    // Threads::AssertOnThread(THREAD_GAME);

    AssertReady();
    
    UpdateSceneCamera();
}

void ShadowRenderer::OnRender(Frame *frame)
{
    // Threads::AssertOnThread(THREAD_RENDER);

    AssertReady();

    if (const auto &camera = m_shadow_pass.GetScene()->GetCamera()) {
        const CameraDrawProxy &draw_proxy = camera->GetDrawProxy();

        const BoundingBox aabb(
            draw_proxy.position + draw_proxy.clip_near,
            draw_proxy.position + draw_proxy.clip_far
        );

        Engine::Get()->GetRenderData()->shadow_maps.Set(
            m_shadow_pass.GetShadowMapIndex(),
            {
                .projection = draw_proxy.projection,
                .view = draw_proxy.view,
                .aabb_max = Vector4(aabb.max, 1.0f),
                .aabb_min = Vector4(aabb.min, 1.0f)
            }
        );
    } else {
        const BoundingBox &aabb = BoundingBox::empty;

        Engine::Get()->GetRenderData()->shadow_maps.Set(
            m_shadow_pass.GetShadowMapIndex(),
            {
                .projection = Matrix4::Identity(),
                .view = Matrix4::Identity(),
                .aabb_max = Vector4(aabb.max, 1.0f),
                .aabb_min = Vector4(aabb.min, 1.0f),
            }
        );
    }

    m_shadow_pass.Render(frame);
}

void ShadowRenderer::UpdateSceneCamera()
{
    // runs in game thread

    const BoundingBox aabb = m_shadow_pass.GetAABB();
    const Vector3 center = aabb.GetCenter();

    const Vector3 light_direction = m_shadow_pass.GetLight()
        ? m_shadow_pass.GetLight()->GetPosition().Normalized() * -1.0f
        : Vector3::Zero();

    Handle<Camera> &camera = m_shadow_pass.GetScene()->GetCamera();

    if (!camera) {
        return;
    }

    camera->SetTranslation(center + light_direction);
    camera->SetTarget(center);
    
    auto corners = aabb.GetCorners();

    auto maxes = MathUtil::MinSafeValue<Vector3>(),
        mins = MathUtil::MaxSafeValue<Vector3>();

    for (auto &corner : corners) {
        corner = camera->GetViewMatrix() * corner;

        maxes = MathUtil::Max(maxes, corner);
        mins = MathUtil::Min(mins, corner);
    }

    camera->SetToOrthographicProjection(
        mins.x, maxes.x,
        mins.y, maxes.y,
        mins.z, maxes.z
    );
}

void ShadowRenderer::OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index /*prev_index*/)
{
    //m_shadow_pass.SetShadowMapIndex(new_index);
    AssertThrowMsg(false, "Not implemented");

    // TODO: Remove descriptor, set new descriptor
}

} // namespace hyperion::v2
