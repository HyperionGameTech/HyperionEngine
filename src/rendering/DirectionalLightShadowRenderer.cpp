/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/DirectionalLightShadowRenderer.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/ShaderGlobals.hpp>
#include <rendering/Shadows.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderState.hpp>

#include <rendering/backend/RendererComputePipeline.hpp>
#include <rendering/backend/RendererShader.hpp>
#include <rendering/backend/RendererDescriptorSet.hpp>

#include <core/threading/TaskSystem.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <scene/camera/OrthoCamera.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

// #define HYP_SHADOW_RENDER_COLLECTION_ASYNC

using renderer::RenderPassStage;
using renderer::RenderPassMode;
using renderer::LoadOperation;
using renderer::StoreOperation;

static const InternalFormat shadow_map_formats[uint32(ShadowMode::MAX)] = {
    InternalFormat::R32F,   // STANDARD
    InternalFormat::R32F,   // PCF
    InternalFormat::R32F,   // CONTACT_HARDENED
    InternalFormat::RG32F   // VSM
};

#pragma region Render commands

struct RENDER_COMMAND(SetShadowMapInGlobalDescriptorSet) : renderer::RenderCommand
{
    uint32          shadow_map_index;
    ImageViewRef    shadow_map_image_view;

    RENDER_COMMAND(SetShadowMapInGlobalDescriptorSet)(
        uint32 shadow_map_index,
        ImageViewRef shadow_map_image_view
    ) : shadow_map_index(shadow_map_index),
        shadow_map_image_view(std::move(shadow_map_image_view))
    {
        AssertThrow(this->shadow_map_image_view != nullptr);
    }

    virtual ~RENDER_COMMAND(SetShadowMapInGlobalDescriptorSet)() override = default;

    virtual RendererResult operator()() override
    {
        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Scene"), frame_index)
                ->SetElement(NAME("ShadowMapTextures"), shadow_map_index, shadow_map_image_view);
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(UnsetShadowMapInGlobalDescriptorSet) : renderer::RenderCommand
{
    uint32  shadow_map_index;

    RENDER_COMMAND(UnsetShadowMapInGlobalDescriptorSet)(
        uint32 shadow_map_index
    ) : shadow_map_index(shadow_map_index)
    {
    }

    virtual ~RENDER_COMMAND(UnsetShadowMapInGlobalDescriptorSet)() override = default;

    virtual RendererResult operator()() override
    {
        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(NAME("Scene"), frame_index)
                ->SetElement(NAME("ShadowMapTextures"), shadow_map_index, g_engine->GetPlaceholderData()->GetImageView2D1x1R8());
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateShadowMapImage) : renderer::RenderCommand
{
    ImageRef        shadow_map_image;
    ImageViewRef    shadow_map_image_view;

    RENDER_COMMAND(CreateShadowMapImage)(const ImageRef &shadow_map_image, const ImageViewRef &shadow_map_image_view)
        : shadow_map_image(shadow_map_image),
          shadow_map_image_view(shadow_map_image_view)
    {
    }

    virtual ~RENDER_COMMAND(CreateShadowMapImage)() override = default;

    virtual RendererResult operator()() override
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

    virtual RendererResult operator()() override
    {
        RendererResult result;

        HYPERION_PASS_ERRORS(shadow_map_image->Destroy(g_engine->GetGPUDevice()), result);
        HYPERION_PASS_ERRORS(shadow_map_image_view->Destroy(g_engine->GetGPUDevice()), result);

        return result;
    }
};

struct RENDER_COMMAND(UpdateShadowMapRenderData) : renderer::RenderCommand
{
    uint32                  shadow_map_index;
    Vec2u                   resolution;
    Matrix4                 view_matrix;
    Matrix4                 projection_matrix;
    BoundingBox             aabb;
    EnumFlags<ShadowFlags>  flags;

    RENDER_COMMAND(UpdateShadowMapRenderData)(
        uint32 shadow_map_index,
        const Vec2u &resolution,
        const Matrix4 &view_matrix,
        const Matrix4 &projection_matrix,
        const BoundingBox &aabb,
        EnumFlags<ShadowFlags> flags
    ) : shadow_map_index(shadow_map_index),
        resolution(resolution),
        view_matrix(view_matrix),
        projection_matrix(projection_matrix),
        aabb(aabb),
        flags(flags)
    {
    }

    virtual ~RENDER_COMMAND(UpdateShadowMapRenderData)() override = default;

    virtual RendererResult operator()() override
    {
        g_engine->GetRenderData()->shadow_map_data->Set(
            shadow_map_index,
            ShadowShaderData {
                .projection = projection_matrix,
                .view       = view_matrix,
                .aabb_max   = Vec4f(aabb.max, 1.0f),
                .aabb_min   = Vec4f(aabb.min, 1.0f),
                .dimensions = resolution,
                .flags      = uint32(flags)
            }
        );

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render commands

#pragma region ShadowPass

ShadowPass::ShadowPass(
    const Handle<Scene> &parent_scene,
    const ShaderRef &shader,
    ShadowMode shadow_mode,
    Vec2u extent, 
    RenderCollector *render_collector_statics,
    RenderCollector *render_collector_dynamics,
    RerenderShadowsSemaphore *rerender_semaphore
) : FullScreenPass(shadow_map_formats[uint32(shadow_mode)], extent),
    m_parent_scene(parent_scene),
    m_shadow_mode(shadow_mode),
    m_shadow_map_index(~0u),
    m_render_collector_statics(render_collector_statics),
    m_render_collector_dynamics(render_collector_dynamics),
    m_rerender_semaphore(rerender_semaphore)
{
    AssertThrow(m_render_collector_statics != nullptr);
    AssertThrow(m_render_collector_dynamics != nullptr);
    AssertThrow(m_rerender_semaphore != nullptr);

    SetShader(shader);
}

ShadowPass::~ShadowPass()
{
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
}

void ShadowPass::CreateFramebuffer()
{
    /* Add the filters' renderpass */
    m_framebuffer = MakeRenderObject<Framebuffer>(
        GetExtent(),
        RenderPassStage::SHADER,
        RenderPassMode::RENDER_PASS_SECONDARY_COMMAND_BUFFER
    );

    // depth, depth^2 texture (for variance shadow map)
    AttachmentRef moments_attachment = m_framebuffer->AddAttachment(
        0,
        GetFormat(),
        ImageType::TEXTURE_TYPE_2D,
        RenderPassStage::SHADER,
        LoadOperation::CLEAR,
        StoreOperation::STORE
    );

    moments_attachment->SetClearColor(MathUtil::Infinity<Vec4f>());

    // standard depth texture
    m_framebuffer->AddAttachment(
        1,
        g_engine->GetDefaultFormat(TEXTURE_FORMAT_DEFAULT_DEPTH),
        ImageType::TEXTURE_TYPE_2D,
        RenderPassStage::SHADER,
        LoadOperation::CLEAR,
        StoreOperation::STORE
    );

    DeferCreate(m_framebuffer, g_engine->GetGPUDevice());
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

        texture = CreateObject<Texture>(TextureDesc {
            ImageType::TEXTURE_TYPE_2D,
            GetFormat(),
            Vec3u { GetExtent().x, GetExtent().y, 1 },
            FilterMode::TEXTURE_FILTER_NEAREST,
            FilterMode::TEXTURE_FILTER_NEAREST,
            WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE
        });

        texture->GetImage()->SetIsRWTexture(true);

        InitObject(texture);
    }
}

void ShadowPass::CreateCombineShadowMapsPass()
{
    ShaderRef shader = g_shader_manager->GetOrCreate(NAME("CombineShadowMaps"), {{ "STAGE_DYNAMICS" }});
    AssertThrow(shader.IsValid());

    DescriptorTableDeclaration descriptor_table_decl = shader->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable();

    DescriptorTableRef descriptor_table = MakeRenderObject<DescriptorTable>(descriptor_table_decl);

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        const DescriptorSetRef &descriptor_set = descriptor_table->GetDescriptorSet(NAME("CombineShadowMapsDescriptorSet"), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(NAME("PrevTexture"), m_shadow_map_statics->GetImageView());
        descriptor_set->SetElement(NAME("InTexture"), m_shadow_map_dynamics->GetImageView());
    }

    DeferCreate(descriptor_table, g_engine->GetGPUInstance()->GetDevice());

    m_combine_shadow_maps_pass = MakeUnique<FullScreenPass>(shader, descriptor_table, GetFormat(), GetExtent());
    m_combine_shadow_maps_pass->Create();
}

void ShadowPass::CreateComputePipelines()
{
    ShaderRef blur_shadow_map_shader = g_shader_manager->GetOrCreate(NAME("BlurShadowMap"));
    AssertThrow(blur_shadow_map_shader.IsValid());

    renderer::DescriptorTableDeclaration descriptor_table_decl = blur_shadow_map_shader->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable();

    DescriptorTableRef descriptor_table = MakeRenderObject<DescriptorTable>(descriptor_table_decl);

    // have to create descriptor sets specifically for compute shader,
    // holding framebuffer attachment image (src), and our final shadowmap image (dst)
    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        const DescriptorSetRef &descriptor_set = descriptor_table->GetDescriptorSet(NAME("BlurShadowMapDescriptorSet"), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(NAME("InputTexture"), m_framebuffer->GetAttachment(0)->GetImageView());
        descriptor_set->SetElement(NAME("OutputTexture"), m_shadow_map_all->GetImageView());
    }

    DeferCreate(descriptor_table, g_engine->GetGPUInstance()->GetDevice());

    m_blur_shadow_map_pipeline = MakeRenderObject<ComputePipeline>(
        blur_shadow_map_shader,
        descriptor_table
    );

    DeferCreate(m_blur_shadow_map_pipeline, g_engine->GetGPUInstance()->GetDevice());
}

void ShadowPass::Create()
{
    CreateShadowMap();
    CreateFramebuffer();
    CreateDescriptors();
    CreateCombineShadowMapsPass();
    CreateComputePipelines();
    CreateCommandBuffers();
}

void ShadowPass::Render(Frame *frame)
{
    Threads::AssertOnThread(g_render_thread);

    const ImageRef &framebuffer_image = GetFramebuffer()->GetAttachment(0)->GetImage();

    if (framebuffer_image == nullptr) {
        return;
    }

    const CommandBufferRef &command_buffer = frame->GetCommandBuffer();

    AssertThrow(m_parent_scene.IsValid());

    g_engine->GetRenderState()->SetActiveScene(m_parent_scene.Get());

    { // Render each shadow map as needed
        if (m_rerender_semaphore->IsInSignalState()) {
            HYP_LOG(Shadows, Debug, "Rerendering static objects for shadow map");

            m_render_collector_statics->CollectDrawCalls(
                frame,
                Bitset((1 << BUCKET_OPAQUE)),
                nullptr
            );

            m_render_collector_statics->ExecuteDrawCalls(
                frame,
                Bitset((1 << BUCKET_OPAQUE)),
                nullptr
            );

            // copy static framebuffer image
            framebuffer_image->InsertBarrier(command_buffer, renderer::ResourceState::COPY_SRC);
            m_shadow_map_statics->GetImage()->InsertBarrier(command_buffer, renderer::ResourceState::COPY_DST);

            m_shadow_map_statics->GetImage()->Blit(command_buffer, framebuffer_image);

            framebuffer_image->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);
            m_shadow_map_statics->GetImage()->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);

            m_rerender_semaphore->Release(1);
        }

        { // Render dynamics
            m_render_collector_dynamics->CollectDrawCalls(
                frame,
                Bitset((1 << BUCKET_OPAQUE)),
                nullptr
            );

            m_render_collector_dynamics->ExecuteDrawCalls(
                frame,
                Bitset((1 << BUCKET_OPAQUE)),
                nullptr
            );

            // copy dynamic framebuffer image
            framebuffer_image->InsertBarrier(command_buffer, renderer::ResourceState::COPY_SRC);
            m_shadow_map_dynamics->GetImage()->InsertBarrier(command_buffer, renderer::ResourceState::COPY_DST);

            m_shadow_map_dynamics->GetImage()->Blit(command_buffer, framebuffer_image);

            framebuffer_image->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);
            m_shadow_map_dynamics->GetImage()->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);
        }
    }

    g_engine->GetRenderState()->UnsetActiveScene();

    { // Combine static and dynamic shadow maps
        const AttachmentRef &attachment = m_combine_shadow_maps_pass->GetFramebuffer()->GetAttachment(0);
        AssertThrow(attachment.IsValid());

        m_combine_shadow_maps_pass->Record(frame->GetFrameIndex());
        m_combine_shadow_maps_pass->Render(frame);

        // Copy combined shadow map to the final shadow map
        attachment->GetImage()->InsertBarrier(command_buffer, renderer::ResourceState::COPY_SRC);
        m_shadow_map_all->GetImage()->InsertBarrier(command_buffer, renderer::ResourceState::COPY_DST);

        m_shadow_map_all->GetImage()->Blit(
            command_buffer,
            attachment->GetImage()
        );

        attachment->GetImage()->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);
        m_shadow_map_dynamics->GetImage()->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);
    }

    if (m_shadow_mode == ShadowMode::VSM) {
        struct alignas(128)
        {
            Vec2u   image_dimensions;
        } push_constants;

        push_constants.image_dimensions = Vec2u(m_framebuffer->GetExtent());

        m_blur_shadow_map_pipeline->SetPushConstants(&push_constants, sizeof(push_constants));

        // blur the image using compute shader
        m_blur_shadow_map_pipeline->Bind(command_buffer);

        // bind descriptor set containing info needed to blur
        m_blur_shadow_map_pipeline->GetDescriptorTable()->Bind(frame, m_blur_shadow_map_pipeline, { });

        // put our shadow map in a state for writing
        m_shadow_map_all->GetImage()->InsertBarrier(command_buffer, renderer::ResourceState::UNORDERED_ACCESS);

        m_blur_shadow_map_pipeline->Dispatch(
            command_buffer,
            Vec3u {
                (m_framebuffer->GetWidth() + 7) / 8,
                (m_framebuffer->GetHeight() + 7) / 8,
                1
            }
        );

        // put shadow map back into readable state
        m_shadow_map_all->GetImage()->InsertBarrier(command_buffer, renderer::ResourceState::SHADER_RESOURCE);
    }
}

#pragma endregion ShadowPass

#pragma region DirectionalLightShadowRenderer

DirectionalLightShadowRenderer::DirectionalLightShadowRenderer(Name name, Vec2u resolution, ShadowMode shadow_mode)
    : RenderSubsystem(name),
      m_resolution(resolution),
      m_shadow_mode(shadow_mode)
{
    m_camera = CreateObject<Camera>(m_resolution.x, m_resolution.y);
    m_camera->SetName(NAME("DirectionalLightShadowRendererCamera"));
    m_camera->AddCameraController(MakeRefCountedPtr<OrthoCameraController>());

    CreateShader();
}

DirectionalLightShadowRenderer::~DirectionalLightShadowRenderer()
{
    // Prevent render components from using shadow pass pointer after deletion
    HYP_SYNC_RENDER();
    m_shadow_pass.Reset();

    m_render_collector_statics.Reset();
    m_render_collector_dynamics.Reset();
}

// called from render thread
void DirectionalLightShadowRenderer::Init()
{
    AssertThrow(IsValidComponent());

    m_shadow_pass = MakeUnique<ShadowPass>(
        GetParent()->GetScene()->HandleFromThis(),
        m_shader,
        m_shadow_mode,
        m_resolution,
        &m_render_collector_statics,
        &m_render_collector_dynamics,
        &m_rerender_semaphore
    );
    m_shadow_pass->SetShadowMapIndex(GetComponentIndex());
    m_shadow_pass->Create();

    m_camera->SetFramebuffer(m_shadow_pass->GetFramebuffer());
    InitObject(m_camera);

    m_render_collector_statics.SetCamera(m_camera);
    m_render_collector_dynamics.SetCamera(m_camera);
}

// called from game thread
void DirectionalLightShadowRenderer::InitGame()
{
    Threads::AssertOnThread(g_game_thread);
}

void DirectionalLightShadowRenderer::OnUpdate(GameCounter::TickUnit delta)
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_game_thread);

    AssertThrow(m_shader.IsValid());

    m_camera->Update(delta);

    Octree &octree = GetParent()->GetScene()->GetOctree();
    octree.CalculateVisibility(m_camera);

    RenderableAttributeSet renderable_attribute_set(
        MeshAttributes { },
        MaterialAttributes {
            .shader_definition  = m_shader->GetCompiledShader()->GetDefinition(),
            .cull_faces         = m_shadow_mode == ShadowMode::VSM ? FaceCullMode::BACK : FaceCullMode::FRONT
        }
    );

    // Render data update
    EnumFlags<ShadowFlags> flags = ShadowFlags::NONE;

    switch (m_shadow_mode) {
    case ShadowMode::VSM:
        flags |= ShadowFlags::VSM;
        break;
    case ShadowMode::CONTACT_HARDENED:
        flags |= ShadowFlags::CONTACT_HARDENED;
        break;
    case ShadowMode::PCF:
        flags |= ShadowFlags::PCF;
        break;
    default:
        break;
    }

#ifdef HYP_SHADOW_RENDER_COLLECTION_ASYNC
    Task<RenderCollector::CollectionResult> statics_collection_task = TaskSystem::GetInstance().Enqueue([this, renderable_attribute_set]
    {
        return GetParent()->GetScene()->CollectStaticEntities(
            m_render_collector_statics,
            m_camera,
            renderable_attribute_set
        );
    });

    Task<void> dynamics_collection_task = TaskSystem::GetInstance().Enqueue([this, renderable_attribute_set]
    {
        GetParent()->GetScene()->CollectDynamicEntities(
            m_render_collector_dynamics,
            m_camera,
            renderable_attribute_set
        );
    });
#else
    RenderCollector::CollectionResult statics_collection_result = GetParent()->GetScene()->CollectStaticEntities(
        m_render_collector_statics,
        m_camera,
        renderable_attribute_set
    );

    GetParent()->GetScene()->CollectDynamicEntities(
        m_render_collector_dynamics,
        m_camera,
        renderable_attribute_set
    );
#endif

    Octree const *fitting_octant = &octree;
    octree.GetFittingOctant(m_aabb, fitting_octant);
    
    const HashCode octant_hash_statics = fitting_octant->GetEntryListHash<EntityTag::STATIC>()
        .Add(fitting_octant->GetEntryListHash<EntityTag::LIGHT>());

#ifdef HYP_SHADOW_RENDER_COLLECTION_ASYNC
    RenderCollector::CollectionResult &statics_collection_result = statics_collection_task.Await();
#endif

    // Need to re-render static objects if:
    // * octant's statics hash code has changed
    // * camera view has changed
    // * static objects have been added, removed or changed
    bool needs_statics_rerender = false;
    needs_statics_rerender |= m_cached_view_matrix != m_camera->GetViewMatrix();
    needs_statics_rerender |= m_cached_octant_hash_code_statics != octant_hash_statics;
    needs_statics_rerender |= statics_collection_result.NeedsUpdate();

    if (needs_statics_rerender) {
        HYP_LOG(Shadows, Debug, "statics collection result: {}, {}, {}", statics_collection_result.num_added_entities, statics_collection_result.num_removed_entities, statics_collection_result.num_changed_entities);
        
        m_cached_view_matrix = m_camera->GetViewMatrix();

        // Force static objects to re-render for a few frames
        m_rerender_semaphore.Produce(1);

        m_cached_view_matrix = m_camera->GetViewMatrix();
        m_cached_octant_hash_code_statics = octant_hash_statics;
    }

#ifdef HYP_SHADOW_RENDER_COLLECTION_ASYNC
    dynamics_collection_task.Await();
#endif

    PUSH_RENDER_COMMAND(
        UpdateShadowMapRenderData,
        GetComponentIndex(),
        m_resolution,
        m_camera->GetViewMatrix(),
        m_camera->GetProjectionMatrix(),
        m_aabb,
        flags
    );
}

void DirectionalLightShadowRenderer::OnRender(Frame *frame)
{
    HYP_SCOPE;
    
    Threads::AssertOnThread(g_render_thread);

    AssertThrow(m_shadow_pass != nullptr);

    m_shadow_pass->Render(frame);
}

void DirectionalLightShadowRenderer::OnComponentIndexChanged(RenderSubsystem::Index new_index, RenderSubsystem::Index /*prev_index*/)
{
    AssertThrowMsg(false, "Not implemented");
}

void DirectionalLightShadowRenderer::CreateShader()
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
        NAME("Shadows"),
        properties
    );
}

#pragma endregion DirectionalLightShadowRenderer

} // namespace hyperion
