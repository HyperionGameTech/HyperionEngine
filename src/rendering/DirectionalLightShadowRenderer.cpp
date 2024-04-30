/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/DirectionalLightShadowRenderer.hpp>

#include <rendering/RenderEnvironment.hpp>

#include <scene/camera/OrthoCamera.hpp>

#include <util/fs/FsUtil.hpp>

#include <Engine.hpp>

namespace hyperion {

using renderer::Image;
using renderer::StorageImage2D;

static const InternalFormat shadow_map_formats[uint(ShadowMode::MAX)] = {
    InternalFormat::R32F,   // STANDARD
    InternalFormat::R32F,   // PCF
    InternalFormat::R32F,   // CONTACT_HARDENED
    InternalFormat::RG32F   // VSM
};

#pragma region Render commands

struct RENDER_COMMAND(SetShadowMapInGlobalDescriptorSet) : renderer::RenderCommand
{
    uint            shadow_map_index;
    ImageViewRef    shadow_map_image_view;

    RENDER_COMMAND(SetShadowMapInGlobalDescriptorSet)(
        uint shadow_map_index,
        ImageViewRef shadow_map_image_view
    ) : shadow_map_index(shadow_map_index),
        shadow_map_image_view(std::move(shadow_map_image_view))
    {
        AssertThrow(this->shadow_map_image_view != nullptr);
    }

    virtual ~RENDER_COMMAND(SetShadowMapInGlobalDescriptorSet)() override = default;

    virtual Result operator()() override
    {
        for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(HYP_NAME(Scene), frame_index)
                ->SetElement(HYP_NAME(ShadowMapTextures), shadow_map_index, shadow_map_image_view);
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(UnsetShadowMapInGlobalDescriptorSet) : renderer::RenderCommand
{
    uint    shadow_map_index;

    RENDER_COMMAND(UnsetShadowMapInGlobalDescriptorSet)(
        uint shadow_map_index
    ) : shadow_map_index(shadow_map_index)
    {
    }

    virtual ~RENDER_COMMAND(UnsetShadowMapInGlobalDescriptorSet)() override = default;

    virtual Result operator()() override
    {
        for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(HYP_NAME(Scene), frame_index)
                ->SetElement(HYP_NAME(ShadowMapTextures), shadow_map_index, g_engine->GetPlaceholderData()->GetImageView2D1x1R8());
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateShadowMapImage) : renderer::RenderCommand
{
    ImageRef shadow_map_image;
    ImageViewRef shadow_map_image_view;

    RENDER_COMMAND(CreateShadowMapImage)(const ImageRef &shadow_map_image, const ImageViewRef &shadow_map_image_view)
        : shadow_map_image(shadow_map_image),
          shadow_map_image_view(shadow_map_image_view)
    {
    }

    virtual ~RENDER_COMMAND(CreateShadowMapImage)() override = default;

    virtual Result operator()() override
    {
        HYPERION_BUBBLE_ERRORS(shadow_map_image->Create(g_engine->GetGPUDevice()));
        HYPERION_BUBBLE_ERRORS(shadow_map_image_view->Create(g_engine->GetGPUDevice(), shadow_map_image));

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(DestroyShadowPassData) : renderer::RenderCommand
{
    ImageRef        shadow_map_image;
    ImageViewRef    shadow_map_image_view;

    RENDER_COMMAND(DestroyShadowPassData)(const ImageRef &shadow_map_image, const ImageViewRef &shadow_map_image_view)
        : shadow_map_image(shadow_map_image),
          shadow_map_image_view(shadow_map_image_view)
    {
    }

    virtual ~RENDER_COMMAND(DestroyShadowPassData)() override = default;

    virtual Result operator()() override
    {
        Result result;

        HYPERION_PASS_ERRORS(shadow_map_image->Destroy(g_engine->GetGPUDevice()), result);
        HYPERION_PASS_ERRORS(shadow_map_image_view->Destroy(g_engine->GetGPUDevice()), result);

        return result;
    }
};

struct RENDER_COMMAND(UpdateShadowMapRenderData) : renderer::RenderCommand
{
    uint            shadow_map_index;
    Matrix4         view_matrix;
    Matrix4         projection_matrix;
    BoundingBox     aabb;
    Extent2D        dimensions;
    ShadowFlags     flags;

    RENDER_COMMAND(UpdateShadowMapRenderData)(
        uint shadow_map_index,
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

    virtual ~RENDER_COMMAND(UpdateShadowMapRenderData)() override = default;

    virtual Result operator()() override
    {
        g_engine->GetRenderData()->shadow_map_data.Set(
            shadow_map_index,
            ShadowShaderData {
                .projection = projection_matrix,
                .view       = view_matrix,
                .aabb_max   = Vec4f(aabb.max, 1.0f),
                .aabb_min   = Vec4f(aabb.min, 1.0f),
                .dimensions = dimensions,
                .flags      = uint32(flags)
            }
        );

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

#pragma region ShadowPass

ShadowPass::ShadowPass(const Handle<Scene> &parent_scene, Extent2D extent, ShadowMode shadow_mode)
    : FullScreenPass(shadow_map_formats[uint(shadow_mode)], extent),
      m_parent_scene(parent_scene),
      m_shadow_mode(shadow_mode),
      m_shadow_map_index(~0u)
{
}

ShadowPass::~ShadowPass() = default;

void ShadowPass::CreateShader()
{
    ShaderProperties properties;
    properties.SetRequiredVertexAttributes(static_mesh_vertex_attributes);

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
        GetExtent(),
        RenderPassStage::SHADER,
        RenderPassMode::RENDER_PASS_SECONDARY_COMMAND_BUFFER
    );

    // Set clear color to infinity for shadow maps
    m_framebuffer->GetRenderPass()->SetClearColor(MathUtil::Infinity<Vec4f>());

    { // depth, depth^2 texture (for variance shadow map)
        auto attachment = MakeRenderObject<Attachment>(
            MakeRenderObject<Image>(renderer::FramebufferImage2D(
                GetExtent(),
                GetFormat(),
                FilterMode::TEXTURE_FILTER_NEAREST,
                FilterMode::TEXTURE_FILTER_NEAREST
            )),
            RenderPassStage::SHADER
        );

        DeferCreate(attachment, g_engine->GetGPUInstance()->GetDevice());
        m_attachments.PushBack(attachment);

        auto attachment_usage = MakeRenderObject<AttachmentUsage>(
            attachment,
            LoadOperation::CLEAR,
            StoreOperation::STORE
        );

        DeferCreate(attachment_usage, g_engine->GetGPUInstance()->GetDevice());
        m_framebuffer->AddAttachmentUsage(std::move(attachment_usage));
    }

    { // standard depth texture
        auto attachment = MakeRenderObject<Attachment>(
            MakeRenderObject<Image>(renderer::FramebufferImage2D(
                GetExtent(),
                g_engine->GetDefaultFormat(TEXTURE_FORMAT_DEFAULT_DEPTH),
                nullptr
            )),
            RenderPassStage::SHADER
        );

        DeferCreate(attachment, g_engine->GetGPUInstance()->GetDevice());
        m_attachments.PushBack(attachment);

        auto attachment_usage = MakeRenderObject<AttachmentUsage>(
            attachment,
            LoadOperation::CLEAR,
            StoreOperation::STORE
        );

        DeferCreate(attachment_usage, g_engine->GetGPUInstance()->GetDevice());
        m_framebuffer->AddAttachmentUsage(std::move(attachment_usage));
    }

    InitObject(m_framebuffer);
}

void ShadowPass::CreateDescriptors()
{
    AssertThrow(m_shadow_map_index != ~0u);

    PUSH_RENDER_COMMAND(
        SetShadowMapInGlobalDescriptorSet,
        m_shadow_map_index,
        m_shadow_map_all->GetImageView()
    );
}

void ShadowPass::CreateShadowMap()
{
    FixedArray<Handle<Texture> *, 3> shadow_map_textures {
        &m_shadow_map_all,
        &m_shadow_map_statics,
        &m_shadow_map_dynamics
    };

    for (Handle<Texture> *texture_ptr : shadow_map_textures) {
        Handle<Texture> &texture = *texture_ptr;

        texture = CreateObject<Texture>(Texture2D(
            GetExtent(),
            GetFormat(),
            FilterMode::TEXTURE_FILTER_NEAREST,
            WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
            nullptr
        ));

        texture->GetImage()->SetIsRWTexture(true);

        InitObject(texture);
    }
}

void ShadowPass::CreateCombineShadowMapsPass()
{
    auto shader = g_shader_manager->GetOrCreate(HYP_NAME(CombineShadowMaps), {{ "STAGE_DYNAMICS" }});
    g_engine->InitObject(shader);

    DescriptorTableDeclaration descriptor_table_decl = shader->GetCompiledShader().GetDescriptorUsages().BuildDescriptorTable();

    DescriptorTableRef descriptor_table = MakeRenderObject<renderer::DescriptorTable>(descriptor_table_decl);

    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        const DescriptorSetRef &descriptor_set = descriptor_table->GetDescriptorSet(HYP_NAME(CombineShadowMapsDescriptorSet), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(HYP_NAME(PrevTexture), m_shadow_map_statics->GetImageView());
        descriptor_set->SetElement(HYP_NAME(InTexture), m_shadow_map_dynamics->GetImageView());
    }

    DeferCreate(descriptor_table, g_engine->GetGPUInstance()->GetDevice());

    m_combine_shadow_maps_pass.Reset(new FullScreenPass(shader, descriptor_table, GetFormat(), GetExtent()));
    m_combine_shadow_maps_pass->Create();
}

void ShadowPass::CreateComputePipelines()
{
    Handle<Shader> blur_shadow_map_shader = g_shader_manager->GetOrCreate(HYP_NAME(BlurShadowMap));
    AssertThrow(blur_shadow_map_shader.IsValid());

    renderer::DescriptorTableDeclaration descriptor_table_decl = blur_shadow_map_shader->GetCompiledShader().GetDescriptorUsages().BuildDescriptorTable();

    DescriptorTableRef descriptor_table = MakeRenderObject<renderer::DescriptorTable>(descriptor_table_decl);

    // have to create descriptor sets specifically for compute shader,
    // holding framebuffer attachment image (src), and our final shadowmap image (dst)
    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        const DescriptorSetRef &descriptor_set = descriptor_table->GetDescriptorSet(HYP_NAME(BlurShadowMapDescriptorSet), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(HYP_NAME(InputTexture), m_framebuffer->GetAttachmentUsages().Front()->GetImageView());
        descriptor_set->SetElement(HYP_NAME(OutputTexture), m_shadow_map_all->GetImageView());
    }

    DeferCreate(descriptor_table, g_engine->GetGPUInstance()->GetDevice());

    m_blur_shadow_map_pipeline = MakeRenderObject<renderer::ComputePipeline>(
        blur_shadow_map_shader->GetShaderProgram(),
        descriptor_table
    );

    DeferCreate(m_blur_shadow_map_pipeline, g_engine->GetGPUInstance()->GetDevice());
}

void ShadowPass::Create()
{
    CreateShadowMap();
    CreateShader();
    CreateFramebuffer();
    CreateDescriptors();
    CreateCombineShadowMapsPass();
    CreateComputePipelines();

    {
        m_camera = CreateObject<Camera>(GetExtent().width, GetExtent().height);

        m_camera->SetCameraController(RC<OrthoCameraController>::Construct());
        m_camera->SetFramebuffer(m_framebuffer);

        InitObject(m_camera);
        
        m_render_list_statics.SetCamera(m_camera);
        m_render_list_dynamics.SetCamera(m_camera);
    }

    CreateCommandBuffers();

    HYP_SYNC_RENDER(); // force init stuff
}

void ShadowPass::Destroy()
{
    m_camera.Reset();
    m_render_list_statics.Reset();
    m_render_list_dynamics.Reset();
    m_parent_scene.Reset();
    m_shadow_map_all.Reset();
    m_shadow_map_statics.Reset();
    m_shadow_map_dynamics.Reset();

    SafeRelease(std::move(m_blur_shadow_map_pipeline));

    if (m_shadow_map_index != ~0u) {
        PUSH_RENDER_COMMAND(
            UnsetShadowMapInGlobalDescriptorSet,
            m_shadow_map_index
        );
    }

    FullScreenPass::Destroy(); // flushes render queue, releases command buffers...
}

void ShadowPass::Render(Frame *frame)
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    Image *framebuffer_image = m_attachments.Front()->GetImage();

    if (framebuffer_image == nullptr) {
        return;
    }

    const CommandBufferRef &command_buffer = frame->GetCommandBuffer();

    AssertThrow(m_parent_scene.IsValid());

    g_engine->GetRenderState().BindScene(m_parent_scene.Get());

    { // Render each shadow map as needed
        const bool needs_statics_rerender = m_camera->GetPreviousViewMatrix() != m_camera->GetViewMatrix();

        if (needs_statics_rerender) {
            m_render_list_statics.CollectDrawCalls(
                frame,
                Bitset((1 << BUCKET_OPAQUE)),
                nullptr
            );

            m_render_list_statics.ExecuteDrawCalls(
                frame,
                Bitset((1 << BUCKET_OPAQUE)),
                nullptr
            );

            // copy static framebuffer image
            framebuffer_image->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::COPY_SRC);
            m_shadow_map_statics->GetImage()->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::COPY_DST);

            m_shadow_map_statics->GetImage()->Blit(
                command_buffer,
                framebuffer_image
            );

            framebuffer_image->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);
            m_shadow_map_statics->GetImage()->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);
        }

        { // Render dynamics
            m_render_list_dynamics.CollectDrawCalls(
                frame,
                Bitset((1 << BUCKET_OPAQUE)),
                nullptr
            );

            m_render_list_dynamics.ExecuteDrawCalls(
                frame,
                Bitset((1 << BUCKET_OPAQUE)),
                nullptr
            );

            // copy dynamic framebuffer image
            framebuffer_image->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::COPY_SRC);
            m_shadow_map_dynamics->GetImage()->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::COPY_DST);

            m_shadow_map_dynamics->GetImage()->Blit(
                command_buffer,
                framebuffer_image
            );

            framebuffer_image->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);
            m_shadow_map_dynamics->GetImage()->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);
        }
    }

    g_engine->GetRenderState().UnbindScene();

    { // Combine static and dynamic shadow maps
        const AttachmentRef &attachment = m_combine_shadow_maps_pass->GetFramebuffer()->GetAttachmentUsages()[0]->GetAttachment();
        AssertThrow(attachment.IsValid());

        m_combine_shadow_maps_pass->Record(frame->GetFrameIndex());
        m_combine_shadow_maps_pass->Render(frame);

        // Copy combined shadow map to the final shadow map
        attachment->GetImage()->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::COPY_SRC);
        m_shadow_map_all->GetImage()->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::COPY_DST);

        m_shadow_map_all->GetImage()->Blit(
            command_buffer,
            attachment->GetImage()
        );

        attachment->GetImage()->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);
        m_shadow_map_dynamics->GetImage()->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);
    }

    if (m_shadow_mode == ShadowMode::VSM) {
        // blur the image using compute shader
        m_blur_shadow_map_pipeline->Bind(
            command_buffer,
            Pipeline::PushConstantData {
                .blur_shadow_map_data = {
                    .image_dimensions = Vec2u(m_framebuffer->GetExtent())
                }
            }
        );

        // bind descriptor set containing info needed to blur
        m_blur_shadow_map_pipeline->GetDescriptorTable()->Bind(frame, m_blur_shadow_map_pipeline, { });

        // put our shadow map in a state for writing
        m_shadow_map_all->GetImage()->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::UNORDERED_ACCESS);

        m_blur_shadow_map_pipeline->Dispatch(
            command_buffer,
            Extent3D {
                (m_framebuffer->GetExtent().width + 7) / 8,
                (m_framebuffer->GetExtent().height + 7) / 8,
                1
            }
        );

        // put shadow map back into readable state
        m_shadow_map_all->GetImage()->GetGPUImage()->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);
    }
}

#pragma endregion ShadowPass

#pragma region DirectionalLightShadowRenderer

DirectionalLightShadowRenderer::DirectionalLightShadowRenderer(Name name, Extent2D resolution, ShadowMode shadow_mode)
    : RenderComponent(name),
      m_resolution(resolution),
      m_shadow_mode(shadow_mode)
{
}

DirectionalLightShadowRenderer::~DirectionalLightShadowRenderer()
{
    if (m_shadow_pass) {
        m_shadow_pass->Destroy(); // flushes render queue
        m_shadow_pass.Reset();
    }
}

// called from render thread
void DirectionalLightShadowRenderer::Init()
{
    AssertThrow(IsValidComponent());

    m_shadow_pass.Reset(new ShadowPass(Handle<Scene>(GetParent()->GetScene()->GetID()), m_resolution, m_shadow_mode));

    m_shadow_pass->SetShadowMapIndex(GetComponentIndex());
    m_shadow_pass->Create();
}

// called from game thread
void DirectionalLightShadowRenderer::InitGame()
{
    Threads::AssertOnThread(ThreadName::THREAD_GAME);
}

void DirectionalLightShadowRenderer::OnUpdate(GameCounter::TickUnit delta)
{
    Threads::AssertOnThread(ThreadName::THREAD_GAME);

    AssertThrow(m_shadow_pass != nullptr);
    AssertThrow(m_shadow_pass->GetCamera().IsValid());
    AssertThrow(m_shadow_pass->GetShader().IsValid());

    m_shadow_pass->GetCamera()->Update(delta);

    GetParent()->GetScene()->GetOctree().CalculateVisibility(m_shadow_pass->GetCamera());

    RenderableAttributeSet renderable_attribute_set(
        MeshAttributes { },
        MaterialAttributes {
            .shader_definition  = m_shadow_pass->GetShader()->GetCompiledShader().GetDefinition(),
            .cull_faces         = m_shadow_pass->GetShadowMode() == ShadowMode::VSM ? FaceCullMode::BACK : FaceCullMode::FRONT
        }
    );

    const bool needs_statics_rerender = m_shadow_pass->GetCamera()->GetPreviousViewMatrix() != m_shadow_pass->GetCamera()->GetViewMatrix();

    if (needs_statics_rerender) {
        GetParent()->GetScene()->CollectStaticEntities(
            m_shadow_pass->GetRenderListStatics(),
            m_shadow_pass->GetCamera(),
            renderable_attribute_set
        );

        m_shadow_pass->GetRenderListStatics().UpdateRenderGroups();
    }

    GetParent()->GetScene()->CollectDynamicEntities(
        m_shadow_pass->GetRenderListDynamics(),
        m_shadow_pass->GetCamera(),
        renderable_attribute_set
    );

    m_shadow_pass->GetRenderListDynamics().UpdateRenderGroups();
}

void DirectionalLightShadowRenderer::OnRender(Frame *frame)
{
    Threads::AssertOnThread(ThreadName::THREAD_RENDER);

    AssertThrow(m_shadow_pass != nullptr);

    m_shadow_pass->Render(frame);
}

void DirectionalLightShadowRenderer::SetCameraData(const ShadowMapCameraData &camera_data)
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
        m_shadow_pass->GetExtent(),
        flags
    );
}

void DirectionalLightShadowRenderer::OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index /*prev_index*/)
{
    AssertThrowMsg(false, "Not implemented");
}

#pragma endregion DirectionalLightShadowRenderer

} // namespace hyperion
