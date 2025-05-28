/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/DirectionalLightShadowRenderer.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/ShaderGlobals.hpp>
#include <rendering/Shadows.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderState.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/RenderTexture.hpp>
#include <rendering/RenderView.hpp>
#include <rendering/RenderLight.hpp>
#include <rendering/RenderShadowMap.hpp>

#include <rendering/backend/RenderingAPI.hpp>
#include <rendering/backend/RendererComputePipeline.hpp>
#include <rendering/backend/RendererShader.hpp>
#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererDescriptorSet.hpp>

#include <core/threading/TaskSystem.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <scene/Texture.hpp>
#include <scene/World.hpp>
#include <scene/View.hpp>

#include <scene/camera/OrthoCamera.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <Engine.hpp>

namespace hyperion {

// #define HYP_SHADOW_RENDER_COLLECTION_ASYNC

using renderer::LoadOperation;
using renderer::StoreOperation;

static const InternalFormat shadow_map_formats[uint32(ShadowMapFilterMode::MAX)] = {
    InternalFormat::R32F, // STANDARD
    InternalFormat::R32F, // PCF
    InternalFormat::R32F, // CONTACT_HARDENED
    InternalFormat::RG32F // VSM
};

#pragma region ShadowPass

ShadowPass::ShadowPass(
    const Handle<Scene>& parent_scene,
    const TResourceHandle<RenderWorld>& world_resource_handle,
    const TResourceHandle<RenderCamera>& camera_resource_handle,
    const TResourceHandle<RenderShadowMap>& shadow_map_resource_handle,
    const TResourceHandle<RenderView>& view_statics_resource_handle,
    const TResourceHandle<RenderView>& view_dynamics_resource_handle,
    const ShaderRef& shader,
    RerenderShadowsSemaphore* rerender_semaphore)
    : FullScreenPass(shadow_map_formats[uint32(shadow_map_resource_handle->GetFilterMode())], shadow_map_resource_handle->GetExtent(), nullptr),
      m_parent_scene(parent_scene),
      m_world_resource_handle(world_resource_handle),
      m_camera_resource_handle(camera_resource_handle),
      m_shadow_map_resource_handle(shadow_map_resource_handle),
      m_view_statics_resource_handle(view_statics_resource_handle),
      m_view_dynamics_resource_handle(view_dynamics_resource_handle),
      m_rerender_semaphore(rerender_semaphore)
{
    AssertThrow(m_rerender_semaphore != nullptr);

    SetShader(shader);
}

ShadowPass::~ShadowPass()
{
    m_parent_scene.Reset();
    m_shadow_map_statics.Reset();
    m_shadow_map_dynamics.Reset();

    SafeRelease(std::move(m_blur_shadow_map_pipeline));
}

void ShadowPass::CreateFramebuffer()
{
    m_framebuffer = g_rendering_api->MakeFramebuffer(GetExtent());

    // depth, depth^2 texture (for variance shadow map)
    AttachmentRef moments_attachment = m_framebuffer->AddAttachment(
        0,
        GetFormat(),
        ImageType::TEXTURE_TYPE_2D,
        LoadOperation::CLEAR,
        StoreOperation::STORE);

    moments_attachment->SetClearColor(MathUtil::Infinity<Vec4f>());

    // standard depth texture
    m_framebuffer->AddAttachment(
        1,
        g_rendering_api->GetDefaultFormat(renderer::DefaultImageFormatType::DEPTH),
        ImageType::TEXTURE_TYPE_2D,
        LoadOperation::CLEAR,
        StoreOperation::STORE);

    DeferCreate(m_framebuffer);
}

void ShadowPass::CreateShadowMap()
{
    AssertThrow(m_world_resource_handle);

    const ShadowMapAtlasElement& atlas_element = m_shadow_map_resource_handle->GetAtlasElement();
    AssertThrow(atlas_element.atlas_index != ~0u);

    FixedArray<Handle<Texture>*, 2> shadow_map_textures {
        &m_shadow_map_statics,
        &m_shadow_map_dynamics
    };

    for (Handle<Texture>* texture_ptr : shadow_map_textures)
    {
        Handle<Texture>& texture = *texture_ptr;

        texture = CreateObject<Texture>(TextureDesc {
            ImageType::TEXTURE_TYPE_2D,
            GetFormat(),
            Vec3u { GetExtent().x, GetExtent().y, 1 },
            FilterMode::TEXTURE_FILTER_NEAREST,
            FilterMode::TEXTURE_FILTER_NEAREST,
            WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE,
            1,
            ImageFormatCapabilities::STORAGE | ImageFormatCapabilities::SAMPLED });

        InitObject(texture);

        texture->SetPersistentRenderResourceEnabled(true);
    }
}

void ShadowPass::CreateCombineShadowMapsPass()
{
    ShaderRef shader = g_shader_manager->GetOrCreate(NAME("CombineShadowMaps"), { { "STAGE_DYNAMICS" } });
    AssertThrow(shader.IsValid());

    DescriptorTableDeclaration descriptor_table_decl = shader->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable();

    DescriptorTableRef descriptor_table = g_rendering_api->MakeDescriptorTable(descriptor_table_decl);

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    {
        const DescriptorSetRef& descriptor_set = descriptor_table->GetDescriptorSet(NAME("CombineShadowMapsDescriptorSet"), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(NAME("PrevTexture"), m_shadow_map_statics->GetRenderResource().GetImageView());
        descriptor_set->SetElement(NAME("InTexture"), m_shadow_map_dynamics->GetRenderResource().GetImageView());
    }

    DeferCreate(descriptor_table);

    m_combine_shadow_maps_pass = MakeUnique<FullScreenPass>(shader, descriptor_table, GetFormat(), GetExtent(), m_gbuffer);
    m_combine_shadow_maps_pass->Create();
}

void ShadowPass::CreateComputePipelines()
{
    ShaderRef blur_shadow_map_shader = g_shader_manager->GetOrCreate(NAME("BlurShadowMap"));
    AssertThrow(blur_shadow_map_shader.IsValid());

    renderer::DescriptorTableDeclaration descriptor_table_decl = blur_shadow_map_shader->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable();

    DescriptorTableRef descriptor_table = g_rendering_api->MakeDescriptorTable(descriptor_table_decl);

    // have to create descriptor sets specifically for compute shader,
    // holding framebuffer attachment image (src), and our final shadowmap image (dst)
    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++)
    {
        const DescriptorSetRef& descriptor_set = descriptor_table->GetDescriptorSet(NAME("BlurShadowMapDescriptorSet"), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(NAME("InputTexture"), m_framebuffer->GetAttachment(0)->GetImageView());
        descriptor_set->SetElement(NAME("OutputTexture"), m_shadow_map_resource_handle->GetImageView());
    }

    DeferCreate(descriptor_table);

    m_blur_shadow_map_pipeline = g_rendering_api->MakeComputePipeline(
        blur_shadow_map_shader,
        descriptor_table);

    DeferCreate(m_blur_shadow_map_pipeline);
}

void ShadowPass::Create()
{
    CreateShadowMap();
    CreateFramebuffer();
    CreateCombineShadowMapsPass();
    CreateComputePipelines();
}

void ShadowPass::Render(FrameBase* frame, RenderView* view)
{
    Threads::AssertOnThread(g_render_thread);

    if (!m_camera_resource_handle)
    {
        return;
    }

    const ImageRef& framebuffer_image = GetFramebuffer()->GetAttachment(0)->GetImage();

    if (framebuffer_image == nullptr)
    {
        return;
    }

    AssertThrow(m_parent_scene.IsValid());

    { // Render each shadow map as needed
        if (m_rerender_semaphore->IsInSignalState())
        {
            HYP_LOG(Shadows, Debug, "Rerendering static objects for shadow map");

            m_view_statics_resource_handle->GetRenderCollector().CollectDrawCalls(
                frame,
                m_view_statics_resource_handle.Get(),
                Bitset((1 << BUCKET_OPAQUE)),
                nullptr);

            m_view_statics_resource_handle->GetRenderCollector().ExecuteDrawCalls(
                frame,
                m_view_statics_resource_handle.Get(),
                Bitset((1 << BUCKET_OPAQUE)),
                nullptr);

            // copy static framebuffer image
            frame->GetCommandList().Add<InsertBarrier>(framebuffer_image, renderer::ResourceState::COPY_SRC);
            frame->GetCommandList().Add<InsertBarrier>(m_shadow_map_statics->GetRenderResource().GetImage(), renderer::ResourceState::COPY_DST);

            frame->GetCommandList().Add<Blit>(framebuffer_image, m_shadow_map_statics->GetRenderResource().GetImage());

            frame->GetCommandList().Add<InsertBarrier>(framebuffer_image, renderer::ResourceState::SHADER_RESOURCE);
            frame->GetCommandList().Add<InsertBarrier>(m_shadow_map_statics->GetRenderResource().GetImage(), renderer::ResourceState::SHADER_RESOURCE);

            m_rerender_semaphore->Release(1);
        }

        { // Render dynamics
            m_view_dynamics_resource_handle->GetRenderCollector().CollectDrawCalls(
                frame,
                m_view_dynamics_resource_handle.Get(),
                Bitset((1 << BUCKET_OPAQUE)),
                nullptr);

            m_view_dynamics_resource_handle->GetRenderCollector().ExecuteDrawCalls(
                frame,
                m_view_dynamics_resource_handle.Get(),
                Bitset((1 << BUCKET_OPAQUE)),
                nullptr);

            // copy dynamic framebuffer image
            frame->GetCommandList().Add<InsertBarrier>(framebuffer_image, renderer::ResourceState::COPY_SRC);
            frame->GetCommandList().Add<InsertBarrier>(m_shadow_map_dynamics->GetRenderResource().GetImage(), renderer::ResourceState::COPY_DST);

            frame->GetCommandList().Add<Blit>(framebuffer_image, m_shadow_map_dynamics->GetRenderResource().GetImage());

            frame->GetCommandList().Add<InsertBarrier>(framebuffer_image, renderer::ResourceState::SHADER_RESOURCE);
            frame->GetCommandList().Add<InsertBarrier>(m_shadow_map_dynamics->GetRenderResource().GetImage(), renderer::ResourceState::SHADER_RESOURCE);
        }
    }

    const ShadowMapAtlasElement& atlas_element = m_shadow_map_resource_handle->GetAtlasElement();

    const ImageViewRef& shadow_map_image_view = m_shadow_map_resource_handle->GetImageView();
    AssertThrow(shadow_map_image_view != nullptr);

    const ImageRef& shadow_map_image = shadow_map_image_view->GetImage();
    AssertThrow(shadow_map_image != nullptr);

    { // Combine static and dynamic shadow maps
        AttachmentBase* attachment = m_combine_shadow_maps_pass->GetFramebuffer()->GetAttachment(0);
        AssertThrow(attachment != nullptr);

        // Note: view unused here
        m_combine_shadow_maps_pass->Render(frame, m_view_statics_resource_handle.Get());

        // Copy combined shadow map to the final shadow map
        frame->GetCommandList().Add<InsertBarrier>(attachment->GetImage(), renderer::ResourceState::COPY_SRC);
        frame->GetCommandList().Add<InsertBarrier>(
            shadow_map_image,
            renderer::ResourceState::COPY_DST,
            renderer::ImageSubResource { .base_array_layer = atlas_element.atlas_index });

        // copy the image
        frame->GetCommandList().Add<Blit>(
            attachment->GetImage(),
            shadow_map_image,
            Rect<uint32> {
                .x0 = 0,
                .y0 = 0,
                .x1 = GetExtent().x,
                .y1 = GetExtent().y },
            Rect<uint32> {
                .x0 = atlas_element.offset_coords.x,
                .y0 = atlas_element.offset_coords.y,
                .x1 = atlas_element.offset_coords.x + atlas_element.dimensions.x,
                .y1 = atlas_element.offset_coords.y + atlas_element.dimensions.y },
            0,                        /* src_mip */
            0,                        /* dst_mip */
            0,                        /* src_face */
            atlas_element.atlas_index /* dst_face */
        );

        // put the images back into a state for reading
        frame->GetCommandList().Add<InsertBarrier>(attachment->GetImage(), renderer::ResourceState::SHADER_RESOURCE);
        frame->GetCommandList().Add<InsertBarrier>(
            shadow_map_image,
            renderer::ResourceState::SHADER_RESOURCE,
            renderer::ImageSubResource { .base_array_layer = atlas_element.atlas_index });
    }

    if (m_shadow_map_resource_handle->GetFilterMode() == ShadowMapFilterMode::VSM)
    {
        struct alignas(128)
        {
            Vec2u image_dimensions;
            Vec2u dimensions;
            Vec2u offset;
        } push_constants;

        push_constants.image_dimensions = shadow_map_image->GetExtent().GetXY();
        push_constants.dimensions = atlas_element.dimensions;
        push_constants.offset = atlas_element.offset_coords;

        m_blur_shadow_map_pipeline->SetPushConstants(&push_constants, sizeof(push_constants));

        // blur the image using compute shader
        frame->GetCommandList().Add<BindComputePipeline>(m_blur_shadow_map_pipeline);

        // bind descriptor set containing info needed to blur
        frame->GetCommandList().Add<BindDescriptorTable>(
            m_blur_shadow_map_pipeline->GetDescriptorTable(),
            m_blur_shadow_map_pipeline,
            ArrayMap<Name, ArrayMap<Name, uint32>> {},
            frame->GetFrameIndex());

        // put our shadow map in a state for writing
        frame->GetCommandList().Add<InsertBarrier>(
            shadow_map_image,
            renderer::ResourceState::UNORDERED_ACCESS,
            renderer::ImageSubResource { .base_array_layer = atlas_element.atlas_index });

        frame->GetCommandList().Add<DispatchCompute>(
            m_blur_shadow_map_pipeline,
            Vec3u {
                (atlas_element.dimensions.x + 7) / 8,
                (atlas_element.dimensions.y + 7) / 8,
                1 });

        // put shadow map back into readable state
        frame->GetCommandList().Add<InsertBarrier>(
            shadow_map_image,
            renderer::ResourceState::SHADER_RESOURCE,
            renderer::ImageSubResource { .base_array_layer = atlas_element.atlas_index });
    }
}

#pragma endregion ShadowPass

#pragma region DirectionalLightShadowRenderer

DirectionalLightShadowRenderer::DirectionalLightShadowRenderer(
    Name name,
    const Handle<Scene>& parent_scene,
    const TResourceHandle<RenderLight>& render_light,
    Vec2u resolution,
    ShadowMapFilterMode filter_mode)
    : RenderSubsystem(name),
      m_parent_scene(parent_scene),
      m_render_light(render_light),
      m_resolution(resolution),
      m_filter_mode(filter_mode)
{
    m_camera = CreateObject<Camera>(m_resolution.x, m_resolution.y);
    m_camera->SetName(NAME("DirectionalLightShadowRendererCamera"));
    m_camera->AddCameraController(MakeRefCountedPtr<OrthoCameraController>());

    InitObject(m_camera);

    m_view_statics = CreateObject<View>(ViewDesc {
        .viewport = Viewport { .extent = Vec2i(m_resolution), .position = Vec2i::Zero() },
        .scene = m_parent_scene,
        .camera = m_camera,
        .entity_collection_flags = ViewEntityCollectionFlags::COLLECT_STATIC });

    InitObject(m_view_statics);

    m_view_dynamics = CreateObject<View>(ViewDesc {
        .viewport = Viewport { .extent = Vec2i(m_resolution), .position = Vec2i::Zero() },
        .scene = m_parent_scene,
        .camera = m_camera,
        .entity_collection_flags = ViewEntityCollectionFlags::COLLECT_DYNAMIC });

    InitObject(m_view_dynamics);

    CreateShader();
}

DirectionalLightShadowRenderer::~DirectionalLightShadowRenderer()
{
    m_shadow_pass.Reset();
}

// called from render thread
void DirectionalLightShadowRenderer::Init()
{
    RenderShadowMap* shadow_render_map = m_parent_scene->GetWorld()->GetRenderResource().GetShadowMapManager()->AllocateShadowMap(ShadowMapType::DIRECTIONAL_SHADOW_MAP, m_filter_mode, m_resolution);
    AssertThrowMsg(shadow_render_map != nullptr, "Failed to allocate shadow map");

    m_shadow_map_resource_handle = TResourceHandle<RenderShadowMap>(*shadow_render_map);

    AssertThrow(m_render_light);
    m_render_light->SetShadowMapResourceHandle(TResourceHandle<RenderShadowMap>(m_shadow_map_resource_handle));

    m_shadow_pass = MakeUnique<ShadowPass>(
        m_parent_scene,
        TResourceHandle<RenderWorld>(m_parent_scene->GetWorld()->GetRenderResource()),
        TResourceHandle<RenderCamera>(m_camera->GetRenderResource()),
        m_shadow_map_resource_handle,
        TResourceHandle<RenderView>(m_view_statics->GetRenderResource()),
        TResourceHandle<RenderView>(m_view_dynamics->GetRenderResource()),
        m_shader,
        &m_rerender_semaphore);
    m_shadow_pass->Create();

    const RenderableAttributeSet override_attributes(
        MeshAttributes {},
        MaterialAttributes {
            .shader_definition = m_shadow_pass->GetShader()->GetCompiledShader()->GetDefinition(),
            .cull_faces = m_shadow_map_resource_handle->GetFilterMode() == ShadowMapFilterMode::VSM ? FaceCullMode::BACK : FaceCullMode::FRONT });

    m_view_statics->GetRenderResource().GetRenderCollector().SetOverrideAttributes(override_attributes);
    m_view_dynamics->GetRenderResource().GetRenderCollector().SetOverrideAttributes(override_attributes);

    m_camera->GetRenderResource().SetFramebuffer(m_shadow_pass->GetFramebuffer());
}

void DirectionalLightShadowRenderer::OnRemoved()
{
    if (m_render_light)
    {
        m_render_light->SetShadowMapResourceHandle(TResourceHandle<RenderShadowMap>());
    }

    m_shadow_pass.Reset();
    m_camera.Reset();

    if (m_shadow_map_resource_handle)
    {
        RenderShadowMap* shadow_render_map = m_shadow_map_resource_handle.Get();

        m_shadow_map_resource_handle.Reset();

        if (!m_parent_scene->GetWorld()->GetRenderResource().GetShadowMapManager()->FreeShadowMap(shadow_render_map))
        {
            HYP_LOG(Shadows, Error, "Failed to free shadow map!");
        }
    }

    RenderSubsystem::OnRemoved();
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

    AssertThrow(m_camera != nullptr);
    m_camera->Update(delta);

    Octree& octree = m_parent_scene->GetOctree();
    octree.CalculateVisibility(m_camera);

#ifdef HYP_SHADOW_RENDER_COLLECTION_ASYNC
    Task<typename RenderProxyTracker::Diff> statics_collection_task = TaskSystem::GetInstance().Enqueue([this, delta]
        {
            m_view_statics->Update(delta);

            return m_view_statics->GetLastCollectionResult();
        });

    Task<void> dynamics_collection_task = TaskSystem::GetInstance().Enqueue([this, delta]
        {
            m_view_dynamics->Update(delta);
        });
#else
    m_view_statics->Update(delta);
    m_view_dynamics->Update(delta);

    typename RenderProxyTracker::Diff statics_collection_result = m_view_statics->GetLastCollectionResult();
#endif

    Octree const* fitting_octant = nullptr;
    octree.GetFittingOctant(m_aabb, fitting_octant);

    if (!fitting_octant)
    {
        fitting_octant = &octree;
    }

    const HashCode octant_hash_statics = fitting_octant->GetOctantID().GetHashCode().Add(fitting_octant->GetEntryListHash<EntityTag::STATIC>()).Add(fitting_octant->GetEntryListHash<EntityTag::LIGHT>());

#ifdef HYP_SHADOW_RENDER_COLLECTION_ASYNC
    typename RenderProxyTracker::Diff statics_collection_result = statics_collection_task.Await();
#endif

    // Need to re-render static objects if:
    // * octant's statics hash code has changed
    // * camera view has changed
    // * static objects have been added, removed or changed
    bool needs_statics_rerender = false;
    needs_statics_rerender |= m_cached_view_matrix != m_camera->GetViewMatrix();
    needs_statics_rerender |= m_cached_octant_hash_code_statics != octant_hash_statics;
    needs_statics_rerender |= statics_collection_result.NeedsUpdate();

    if (needs_statics_rerender)
    {
        m_cached_view_matrix = m_camera->GetViewMatrix();

        // Force static objects to re-render for a few frames
        m_rerender_semaphore.Produce(1);

        m_cached_view_matrix = m_camera->GetViewMatrix();
        m_cached_octant_hash_code_statics = octant_hash_statics;
    }

#ifdef HYP_SHADOW_RENDER_COLLECTION_ASYNC
    dynamics_collection_task.Await();
#endif

    m_shadow_map_resource_handle->SetBufferData(ShadowMapShaderData {
        .projection = m_camera->GetProjectionMatrix(),
        .view = m_camera->GetViewMatrix(),
        .aabb_max = Vec4f(m_aabb.max, 1.0f),
        .aabb_min = Vec4f(m_aabb.min, 1.0f) });
}

void DirectionalLightShadowRenderer::OnRender(FrameBase* frame)
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_render_thread);

    AssertThrow(m_shadow_pass != nullptr);
    m_shadow_pass->Render(frame, nullptr);
}

void DirectionalLightShadowRenderer::CreateShader()
{
    ShaderProperties properties;
    properties.SetRequiredVertexAttributes(static_mesh_vertex_attributes);

    switch (m_filter_mode)
    {
    case ShadowMapFilterMode::VSM:
        properties.Set("MODE_VSM");
        break;
    case ShadowMapFilterMode::CONTACT_HARDENED:
        properties.Set("MODE_CONTACT_HARDENED");
        break;
    case ShadowMapFilterMode::PCF:
        properties.Set("MODE_PCF");
        break;
    case ShadowMapFilterMode::STANDARD: // fallthrough
    default:
        properties.Set("MODE_STANDARD");
        break;
    }

    m_shader = g_shader_manager->GetOrCreate(
        NAME("Shadows"),
        properties);
}

#pragma endregion DirectionalLightShadowRenderer

} // namespace hyperion
