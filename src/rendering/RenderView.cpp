/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <rendering/RenderView.hpp>
#include <rendering/RenderWorld.hpp>
#include <rendering/RenderTexture.hpp>
#include <rendering/RenderMesh.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderEnvProbe.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderEnvGrid.hpp>
#include <rendering/RenderLight.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/RenderState.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/DepthPyramidRenderer.hpp>
#include <rendering/PostFX.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/SSGI.hpp>
#include <rendering/SSRRenderer.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/ShaderGlobals.hpp>
#include <rendering/debug/DebugDrawer.hpp>

#include <scene/View.hpp>
#include <scene/Texture.hpp>
#include <scene/Mesh.hpp>
#include <scene/Material.hpp>
#include <scene/Light.hpp>

#include <core/math/MathUtil.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <Engine.hpp>

namespace hyperion {

static const Vec2u mip_chain_extent { 512, 512 };
static const InternalFormat mip_chain_format = InternalFormat::R10G10B10A2;

#pragma region RenderCollector helpers

static RenderableAttributeSet GetRenderableAttributesForProxy(const RenderProxy &proxy, const RenderableAttributeSet *override_attributes = nullptr)
{
    HYP_SCOPE;
    
    const Handle<Mesh> &mesh = proxy.mesh;
    AssertThrow(mesh.IsValid());

    const Handle<Material> &material = proxy.material;
    AssertThrow(material.IsValid());

    RenderableAttributeSet attributes {
        mesh->GetMeshAttributes(),
        material->GetRenderAttributes()
    };

    if (override_attributes) {
        if (const ShaderDefinition &override_shader_definition = override_attributes->GetShaderDefinition()) {
            attributes.SetShaderDefinition(override_shader_definition);
        }
        
        const ShaderDefinition &shader_definition = override_attributes->GetShaderDefinition().IsValid()
            ? override_attributes->GetShaderDefinition()
            : attributes.GetShaderDefinition();

#ifdef HYP_DEBUG_MODE
        AssertThrow(shader_definition.IsValid());
#endif

        // Check for varying vertex attributes on the override shader compared to the entity's vertex
        // attributes. If there is not a match, we should switch to a version of the override shader that
        // has matching vertex attribs.
        const VertexAttributeSet mesh_vertex_attributes = attributes.GetMeshAttributes().vertex_attributes;

        MaterialAttributes new_material_attributes = override_attributes->GetMaterialAttributes();
        new_material_attributes.shader_definition = shader_definition;

        if (mesh_vertex_attributes != new_material_attributes.shader_definition.GetProperties().GetRequiredVertexAttributes()) {
            new_material_attributes.shader_definition.properties.SetRequiredVertexAttributes(mesh_vertex_attributes);
        }

        // do not override bucket!
        new_material_attributes.bucket = attributes.GetMaterialAttributes().bucket;

        attributes.SetMaterialAttributes(new_material_attributes);
    }

    return attributes;
}

static void AddRenderProxy(
    EntityDrawCollection *collection,
    RenderProxyList &proxy_list,
    RenderProxy &&proxy,
    CameraRenderResource *camera_render_resource,
    ViewRenderResource *view_render_resource,
    const RenderableAttributeSet &attributes,
    Bucket bucket
)
{
    HYP_SCOPE;

    const ID<Entity> entity = proxy.entity.GetID();

    // Add proxy to group
    Handle<RenderGroup> &render_group = collection->GetProxyGroups()[uint32(bucket)][attributes];

    if (!render_group.IsValid()) {
        HYP_LOG(RenderCollection, Debug, "Creating RenderGroup for attributes:\n"
            "\tVertex Attributes: {}\n"
            "\tBucket: {}\n"
            "\tShader Definition: {}  {}\n"
            "\tCull Mode: {}\n"
            "\tFlags: {}\n"
            "\tDrawable layer: {}",
            attributes.GetMeshAttributes().vertex_attributes.flag_mask,
            uint32(attributes.GetMaterialAttributes().bucket),
            attributes.GetShaderDefinition().GetName(),
            attributes.GetShaderDefinition().GetProperties().GetHashCode().Value(),
            uint32(attributes.GetMaterialAttributes().cull_faces),
            uint32(attributes.GetMaterialAttributes().flags),
            attributes.GetDrawableLayer());

        EnumFlags<RenderGroupFlags> render_group_flags = RenderGroupFlags::DEFAULT;

        // Disable occlusion culling for translucent objects
        const Bucket bucket = attributes.GetMaterialAttributes().bucket;

        if (bucket == BUCKET_TRANSLUCENT || bucket == BUCKET_DEBUG) {
            render_group_flags &= ~(RenderGroupFlags::OCCLUSION_CULLING | RenderGroupFlags::INDIRECT_RENDERING);
        }

        // Create RenderGroup
        render_group = CreateObject<RenderGroup>(
            g_shader_manager->GetOrCreate(attributes.GetShaderDefinition()),
            attributes,
            render_group_flags
        );

        FramebufferRef framebuffer;

        if (camera_render_resource != nullptr) {
            framebuffer = camera_render_resource->GetFramebuffer();
        }

        if (framebuffer != nullptr) {
            render_group->AddFramebuffer(framebuffer);
        } else {
            AssertThrow(view_render_resource != nullptr);
            AssertThrow(view_render_resource->GetView()->GetFlags() & ViewFlags::GBUFFER);

            GBuffer *gbuffer = view_render_resource->GetGBuffer();
            AssertThrow(gbuffer != nullptr);

            const FramebufferRef &bucket_framebuffer = gbuffer->GetBucket(attributes.GetMaterialAttributes().bucket).GetFramebuffer();
            AssertThrow(bucket_framebuffer != nullptr);

            render_group->AddFramebuffer(bucket_framebuffer);
        }

        InitObject(render_group);
    }

    auto iter = proxy_list.Add(entity, std::move(proxy));

    render_group->AddRenderProxy(iter->second);
}

static bool RemoveRenderProxy(EntityDrawCollection *collection, RenderProxyList &proxy_list, ID<Entity> entity, const RenderableAttributeSet &attributes, Bucket bucket)
{
    HYP_SCOPE;

    auto &render_groups_by_attributes = collection->GetProxyGroups()[uint32(bucket)];

    auto it = render_groups_by_attributes.Find(attributes);
    AssertThrow(it != render_groups_by_attributes.End());

    const Handle<RenderGroup> &render_group = it->second;
    AssertThrow(render_group.IsValid());

    const bool removed = render_group->RemoveRenderProxy(entity);

    proxy_list.MarkToRemove(entity);

    return removed;
}

#pragma endregion RenderCollector helpers

#pragma region ViewRenderResource

ViewRenderResource::ViewRenderResource(View *view)
    : m_view(view),
      m_renderer_config(RendererConfig::FromConfig()),
      m_viewport(view ? view->GetViewport() : Viewport { }),
      m_priority(view ? view->GetPriority() : 0)
{
    m_light_render_resource_handles.Resize(uint32(LightType::MAX));
}

ViewRenderResource::~ViewRenderResource()
{
    m_light_render_resource_handles.Clear();
}

void ViewRenderResource::Initialize_Internal()
{
    HYP_SCOPE;

    // Reclaim any claimed resources that were unclaimed in Destroy_Internal
    EntityDrawCollection *collection = m_render_collector.GetDrawCollection();
    RenderProxyList &proxy_list = collection->GetProxyList();

    Array<RenderProxy *> proxies;
    proxy_list.GetCurrent(proxies);

    for (RenderProxy *proxy : proxies) {
        proxy->ClaimRenderResource();
    }

    if (!m_view || (m_view->GetFlags() & ViewFlags::GBUFFER)) {
        AssertThrow(m_viewport.extent.Volume() > 0);

        CreateRenderer();
    }
}

void ViewRenderResource::Destroy_Internal()
{
    HYP_SCOPE;

    // Unclaim all the claimed resources
    EntityDrawCollection *collection = m_render_collector.GetDrawCollection();
    RenderProxyList &proxy_list = collection->GetProxyList();

    Array<RenderProxy *> proxies;
    proxy_list.GetCurrent(proxies);

    for (RenderProxy *proxy : proxies) {
        proxy->UnclaimRenderResource();
    }

    // NOTE: We don't clear out the proxy list, we need to know which proxies to reclaim if this is reactivated

    if (m_gbuffer) {
        DestroyRenderer();

        SafeRelease(std::move(m_final_pass_descriptor_set));
    }
}

void ViewRenderResource::Update_Internal()
{
    HYP_SCOPE;
}

void ViewRenderResource::CreateRenderer()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    m_gbuffer = MakeUnique<GBuffer>(Vec2u(m_viewport.extent));
    m_gbuffer->Create();

    const FramebufferRef &opaque_fbo = m_gbuffer->GetBucket(Bucket::BUCKET_OPAQUE).GetFramebuffer();
    const FramebufferRef &lightmap_fbo = m_gbuffer->GetBucket(Bucket::BUCKET_LIGHTMAP).GetFramebuffer();
    const FramebufferRef &translucent_fbo = m_gbuffer->GetBucket(Bucket::BUCKET_TRANSLUCENT).GetFramebuffer();

    m_env_grid_radiance_pass = MakeUnique<EnvGridPass>(EnvGridPassMode::RADIANCE, m_gbuffer.Get());
    m_env_grid_radiance_pass->Create();

    m_env_grid_irradiance_pass = MakeUnique<EnvGridPass>(EnvGridPassMode::IRRADIANCE, m_gbuffer.Get());
    m_env_grid_irradiance_pass->Create();

    m_ssgi = MakeUnique<SSGI>(SSGIConfig::FromConfig(), m_gbuffer.Get());
    m_ssgi->Create();

    m_post_processing = MakeUnique<PostProcessing>();
    m_post_processing->Create();

    m_indirect_pass = MakeUnique<DeferredPass>(DeferredPassMode::INDIRECT_LIGHTING, m_gbuffer.Get());
    m_indirect_pass->Create();

    m_direct_pass = MakeUnique<DeferredPass>(DeferredPassMode::DIRECT_LIGHTING, m_gbuffer.Get());
    m_direct_pass->Create();

    AttachmentBase *depth_attachment = opaque_fbo->GetAttachment(GBUFFER_RESOURCE_MAX - 1);
    AssertThrow(depth_attachment != nullptr);

    m_depth_pyramid_renderer = MakeUnique<DepthPyramidRenderer>(depth_attachment->GetImageView());
    m_depth_pyramid_renderer->Create();

    m_mip_chain = CreateObject<Texture>(TextureDesc {
        ImageType::TEXTURE_TYPE_2D,
        mip_chain_format,
        Vec3u { mip_chain_extent.x, mip_chain_extent.y, 1 },
        FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP,
        FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP,
        WrapMode::TEXTURE_WRAP_CLAMP_TO_EDGE
    });

    InitObject(m_mip_chain);

    m_mip_chain->SetPersistentRenderResourceEnabled(true);

    m_hbao = MakeUnique<HBAO>(HBAOConfig::FromConfig(), m_gbuffer.Get());
    m_hbao->Create();

    // m_dof_blur = MakeUnique<DOFBlur>(m_gbuffer->GetResolution(), m_gbuffer);
    // m_dof_blur->Create();

    CreateCombinePass();

    m_reflections_pass = MakeUnique<ReflectionsPass>(m_gbuffer.Get(), m_mip_chain->GetRenderResource().GetImageView(), m_combine_pass->GetFinalImageView());
    m_reflections_pass->Create();

    m_tonemap_pass = MakeUnique<TonemapPass>(m_gbuffer.Get());
    m_tonemap_pass->Create();

    // We'll render the lightmap pass into the translucent framebuffer after deferred shading has been applied to OPAQUE objects.
    m_lightmap_pass = MakeUnique<LightmapPass>(translucent_fbo, m_gbuffer.Get());
    m_lightmap_pass->Create();
    
    m_temporal_aa = MakeUnique<TemporalAA>(m_gbuffer->GetExtent(), m_combine_pass->GetFinalImageView(), m_gbuffer.Get());
    m_temporal_aa->Create();
    
    CreateDescriptorSets();
    CreateFinalPassDescriptorSet();
}

void ViewRenderResource::CreateDescriptorSets()
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_render_thread);

    const renderer::DescriptorSetDeclaration *declaration = g_engine->GetGlobalDescriptorTable()->GetDeclaration().FindDescriptorSetDeclaration(NAME("Scene"));
    AssertThrow(declaration != nullptr);

    const renderer::DescriptorSetLayout layout(*declaration);

    FixedArray<DescriptorSetRef, max_frames_in_flight> descriptor_sets;

    static const FixedArray<Name, GBUFFER_RESOURCE_MAX> gbuffer_texture_names {
        NAME("GBufferAlbedoTexture"),
        NAME("GBufferNormalsTexture"),
        NAME("GBufferMaterialTexture"),
        NAME("GBufferLightmapTexture"),
        NAME("GBufferVelocityTexture"),
        NAME("GBufferMaskTexture"),
        NAME("GBufferWSNormalsTexture"),
        NAME("GBufferTranslucentTexture")
    };

    const FramebufferRef &opaque_fbo = m_gbuffer->GetBucket(Bucket::BUCKET_OPAQUE).GetFramebuffer();
    const FramebufferRef &lightmap_fbo = m_gbuffer->GetBucket(Bucket::BUCKET_LIGHTMAP).GetFramebuffer();
    const FramebufferRef &translucent_fbo = m_gbuffer->GetBucket(Bucket::BUCKET_TRANSLUCENT).GetFramebuffer();

    // depth attachment goes into separate slot
    AttachmentBase *depth_attachment = opaque_fbo->GetAttachment(GBUFFER_RESOURCE_MAX - 1);
    AssertThrow(depth_attachment != nullptr);

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        DescriptorSetRef descriptor_set = g_rendering_api->MakeDescriptorSet(layout);

        if (g_rendering_api->GetRenderConfig().IsDynamicDescriptorIndexingSupported()) {
            uint32 gbuffer_element_index = 0;

            // not including depth texture here (hence the - 1)
            for (uint32 attachment_index = 0; attachment_index < GBUFFER_RESOURCE_MAX - 1; attachment_index++) {
                descriptor_set->SetElement(NAME("GBufferTextures"), gbuffer_element_index++, opaque_fbo->GetAttachment(attachment_index)->GetImageView());
            }

            // add translucent bucket's albedo
            descriptor_set->SetElement(NAME("GBufferTextures"), gbuffer_element_index++, translucent_fbo->GetAttachment(0)->GetImageView());
        } else {
            for (uint32 attachment_index = 0; attachment_index < GBUFFER_RESOURCE_MAX - 1; attachment_index++) {
                descriptor_set->SetElement(gbuffer_texture_names[attachment_index], opaque_fbo->GetAttachment(attachment_index)->GetImageView());
            }

            // add translucent bucket's albedo
            descriptor_set->SetElement(NAME("GBufferTranslucentTexture"), translucent_fbo->GetAttachment(0)->GetImageView());
        }

        descriptor_set->SetElement(NAME("GBufferDepthTexture"), depth_attachment->GetImageView());
        
        descriptor_set->SetElement(NAME("GBufferMipChain"), m_mip_chain->GetRenderResource().GetImageView());

        descriptor_set->SetElement(NAME("PostProcessingUniforms"), m_post_processing->GetUniformBuffer());

        descriptor_set->SetElement(NAME("DepthPyramidResult"), m_depth_pyramid_renderer->GetResultImageView());

        descriptor_set->SetElement(NAME("TAAResultTexture"), m_temporal_aa->GetResultTexture()->GetRenderResource().GetImageView());

        if (m_reflections_pass->ShouldRenderSSR()) {
            descriptor_set->SetElement(NAME("SSRResultTexture"), m_reflections_pass->GetSSRRenderer()->GetFinalResultTexture()->GetRenderResource().GetImageView());
        } else {
            descriptor_set->SetElement(NAME("SSRResultTexture"), g_engine->GetPlaceholderData()->GetImageView2D1x1R8());
        }

        if (m_ssgi) {
            descriptor_set->SetElement(NAME("SSGIResultTexture"), m_ssgi->GetFinalResultTexture()->GetRenderResource().GetImageView());
        } else {
            descriptor_set->SetElement(NAME("SSGIResultTexture"), g_engine->GetPlaceholderData()->GetImageView2D1x1R8());
        }

        if (m_hbao) {
            descriptor_set->SetElement(NAME("SSAOResultTexture"), m_hbao->GetFinalImageView());
        } else {
            descriptor_set->SetElement(NAME("SSAOResultTexture"), g_engine->GetPlaceholderData()->GetImageView2D1x1R8());
        }

        descriptor_set->SetElement(NAME("DeferredResult"), m_combine_pass->GetFinalImageView());

        descriptor_set->SetElement(NAME("DeferredIndirectResultTexture"), m_indirect_pass->GetFinalImageView());

        descriptor_set->SetElement(NAME("ReflectionProbeResultTexture"), m_reflections_pass->GetFinalImageView());

        descriptor_set->SetElement(NAME("EnvGridRadianceResultTexture"), m_env_grid_radiance_pass->GetFinalImageView());
        descriptor_set->SetElement(NAME("EnvGridIrradianceResultTexture"), m_env_grid_irradiance_pass->GetFinalImageView());

        HYPERION_ASSERT_RESULT(descriptor_set->Create());

        descriptor_sets[frame_index] = std::move(descriptor_set);
    }

    m_descriptor_sets = std::move(descriptor_sets);
}

void ViewRenderResource::CreateCombinePass()
{
    HYP_SCOPE;

    Threads::AssertOnThread(g_render_thread);

    // The combine pass will render into the translucent bucket's framebuffer with the shaded result.
    const FramebufferRef &translucent_fbo = m_gbuffer->GetBucket(Bucket::BUCKET_TRANSLUCENT).GetFramebuffer();
    AssertThrow(translucent_fbo != nullptr);

    ShaderRef render_texture_to_screen_shader = g_shader_manager->GetOrCreate(NAME("RenderTextureToScreen_UI"));
    AssertThrow(render_texture_to_screen_shader.IsValid());

    const renderer::DescriptorTableDeclaration descriptor_table_decl = render_texture_to_screen_shader->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable();
    DescriptorTableRef descriptor_table = g_rendering_api->MakeDescriptorTable(descriptor_table_decl);

    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        const DescriptorSetRef &descriptor_set = descriptor_table->GetDescriptorSet(NAME("RenderTextureToScreenDescriptorSet"), frame_index);
        AssertThrow(descriptor_set != nullptr);

        descriptor_set->SetElement(NAME("InTexture"), m_indirect_pass->GetFinalImageView());
    }

    DeferCreate(descriptor_table);

    m_combine_pass = MakeUnique<FullScreenPass>(
        render_texture_to_screen_shader,
        std::move(descriptor_table),
        translucent_fbo,
        translucent_fbo->GetAttachment(0)->GetFormat(),
        translucent_fbo->GetExtent(),
        nullptr
    );

    m_combine_pass->Create();
}

void ViewRenderResource::CreateFinalPassDescriptorSet()
{
    ShaderRef render_texture_to_screen_shader = g_shader_manager->GetOrCreate(NAME("RenderTextureToScreen_UI"));
    AssertThrow(render_texture_to_screen_shader.IsValid());

    const ImageViewRef &input_image_view = m_renderer_config.taa_enabled
        ? m_temporal_aa->GetResultTexture()->GetRenderResource().GetImageView()
        : m_combine_pass->GetFinalImageView();

    AssertThrow(input_image_view.IsValid());

    const renderer::DescriptorTableDeclaration descriptor_table_decl = render_texture_to_screen_shader->GetCompiledShader()->GetDescriptorUsages().BuildDescriptorTable();

    renderer::DescriptorSetDeclaration *decl = descriptor_table_decl.FindDescriptorSetDeclaration(NAME("RenderTextureToScreenDescriptorSet"));
    AssertThrow(decl != nullptr);

    const renderer::DescriptorSetLayout layout(*decl);

    DescriptorSetRef descriptor_set = g_rendering_api->MakeDescriptorSet(layout);
    AssertThrow(descriptor_set.IsValid());

    descriptor_set->SetElement(NAME("InTexture"), input_image_view);

    DeferCreate(descriptor_set);

    m_final_pass_descriptor_set = std::move(descriptor_set);
}

void ViewRenderResource::DestroyRenderer()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    m_depth_pyramid_renderer.Reset();

    m_hbao.Reset();

    m_temporal_aa.Reset();

    // m_dof_blur->Destroy();

    m_ssgi.Reset();

    m_post_processing->Destroy();
    m_post_processing.Reset();

    m_combine_pass.Reset();

    m_env_grid_radiance_pass.Reset();
    m_env_grid_irradiance_pass.Reset();

    m_reflections_pass.Reset();

    m_lightmap_pass.Reset();

    m_tonemap_pass.Reset();

    m_mip_chain.Reset();

    m_indirect_pass.Reset();
    m_direct_pass.Reset();

    m_gbuffer->Destroy();
}

void ViewRenderResource::SetViewport(const Viewport &viewport)
{
    HYP_SCOPE;

    Execute([this, viewport]()
    {
        if (m_viewport == viewport) {
            return;
        }

        m_viewport = viewport;
        
        if (IsInitialized() && m_gbuffer) {
            AssertThrow(m_viewport.extent.Volume() > 0);

            const Vec2u new_size = Vec2u(m_viewport.extent);

            m_gbuffer->Resize(new_size);

            const FramebufferRef &opaque_fbo = m_gbuffer->GetBucket(Bucket::BUCKET_OPAQUE).GetFramebuffer();
            const FramebufferRef &lightmap_fbo = m_gbuffer->GetBucket(Bucket::BUCKET_LIGHTMAP).GetFramebuffer();
            const FramebufferRef &translucent_fbo = m_gbuffer->GetBucket(Bucket::BUCKET_TRANSLUCENT).GetFramebuffer();

            m_hbao->Resize(new_size);

            m_direct_pass->Resize(new_size);
            m_indirect_pass->Resize(new_size);
            // m_combine_pass->Resize(new_size);

            m_combine_pass.Reset();
            CreateCombinePass();

            m_env_grid_radiance_pass->Resize(new_size);
            m_env_grid_irradiance_pass->Resize(new_size);

            m_reflections_pass->Resize(new_size);
            
            m_tonemap_pass->Resize(new_size);

            m_lightmap_pass = MakeUnique<LightmapPass>(translucent_fbo, m_gbuffer.Get());
            m_lightmap_pass->Create();

            AttachmentBase *depth_attachment = opaque_fbo->GetAttachment(GBUFFER_RESOURCE_MAX - 1);
            AssertThrow(depth_attachment != nullptr);

            m_depth_pyramid_renderer = MakeUnique<DepthPyramidRenderer>(depth_attachment->GetImageView());
            m_depth_pyramid_renderer->Create();

            SafeRelease(std::move(m_descriptor_sets));
            CreateDescriptorSets();

            SafeRelease(std::move(m_final_pass_descriptor_set));
            CreateFinalPassDescriptorSet();
        }
    });
}

void ViewRenderResource::SetPriority(int priority)
{
    HYP_SCOPE;

    Execute([this, priority]()
    {
        m_priority = priority;
    });
}

void ViewRenderResource::AddLight(const TResourceHandle<LightRenderResource> &light)
{
    HYP_SCOPE;

    if (!light) {
        return;
    }

    AssertThrow(uint32(light->GetLight()->GetLightType()) < uint32(LightType::MAX));

    Execute([this, light]()
    {
        const LightType light_type = light->GetLight()->GetLightType();

        if (m_light_render_resource_handles[uint32(light_type)].Contains(light)) {
            return;
        }

        m_light_render_resource_handles[uint32(light_type)].PushBack(std::move(light));
    });
}

void ViewRenderResource::RemoveLight(LightRenderResource *light)
{
    HYP_SCOPE;

    if (!light) {
        return;
    }

    AssertThrow(uint32(light->GetLight()->GetLightType()) < uint32(LightType::MAX));

    Execute([this, light]()
    {
        const LightType light_type = light->GetLight()->GetLightType();

        auto it = m_light_render_resource_handles[uint32(light_type)].FindIf([light](const TResourceHandle<LightRenderResource> &item)
        {
            return item.Get() == light;
        });

        if (it == m_light_render_resource_handles[uint32(light_type)].End()) {
            return;
        }

        m_light_render_resource_handles[uint32(light_type)].Erase(it);
    });
}

void ViewRenderResource::SetLights(Array<TResourceHandle<LightRenderResource>> &&lights)
{
    HYP_SCOPE;

    if (lights.Empty()) {
        return;
    }

    Execute([this, lights = std::move(lights)]()
    {
        for (const TResourceHandle<LightRenderResource> &light : lights) {
            if (!light) {
                continue;
            }

            AssertThrow(uint32(light->GetLight()->GetLightType()) < uint32(LightType::MAX));

            const LightType light_type = light->GetLight()->GetLightType();

            if (m_light_render_resource_handles[uint32(light_type)].Contains(light)) {
                continue;
            }

            m_light_render_resource_handles[uint32(light_type)].PushBack(std::move(light));
        }
    });
}

RenderCollector::CollectionResult ViewRenderResource::UpdateRenderCollector(RenderProxyList &render_proxy_list)
{
    HYP_SCOPE;
    Threads::AssertOnThread(~g_render_thread);

    RenderCollector::CollectionResult collection_result { };
    collection_result.num_added_entities = render_proxy_list.GetAdded().Count();
    collection_result.num_removed_entities = render_proxy_list.GetRemoved().Count();
    collection_result.num_changed_entities = render_proxy_list.GetChanged().Count();

    if (collection_result.NeedsUpdate()) {
        Array<ID<Entity>> removed_proxies;
        render_proxy_list.GetRemoved(removed_proxies, true /* include_changed */);

        Array<RenderProxy *> added_proxy_ptrs;
        render_proxy_list.GetAdded(added_proxy_ptrs, true /* include_changed */);

        if (added_proxy_ptrs.Any() || removed_proxies.Any()) {
            Array<RenderProxy, DynamicAllocator> added_proxies;
            added_proxies.Reserve(added_proxy_ptrs.Size());
    
            for (RenderProxy *proxy_ptr : added_proxy_ptrs) {
                added_proxies.PushBack(*proxy_ptr);
            }

            Execute([this, added_proxies = std::move(added_proxies), removed_proxies = std::move(removed_proxies)]() mutable
            {
                AssertDebugMsg(IsInitialized(), "UpdateRenderCollector can only be called on an initialized ViewRenderResource");

                EntityDrawCollection *collection = m_render_collector.GetDrawCollection();
                const RenderableAttributeSet *override_attributes = m_render_collector.GetOverrideAttributes().TryGet();

                RenderProxyList &proxy_list = collection->GetProxyList();

                // Reserve to prevent iterator invalidation (pointers to proxies are stored in the render groups)
                proxy_list.Reserve(added_proxies.Size());

                // Claim the added(+changed) before unclaiming the removed, as we may end up doing extra work for changed entities otherwise (unclaiming then reclaiming)
                for (RenderProxy &proxy : added_proxies) {
                    proxy.ClaimRenderResource();
                }

                for (ID<Entity> entity_id : removed_proxies) {
                    const RenderProxy *proxy = proxy_list.GetElement(entity_id);
                    AssertThrowMsg(proxy != nullptr, "Proxy is missing for Entity #%u", entity_id.Value());

                    proxy->UnclaimRenderResource();

                    const RenderableAttributeSet attributes = GetRenderableAttributesForProxy(*proxy, override_attributes);
                    const Bucket bucket = attributes.GetMaterialAttributes().bucket;

                    AssertThrow(RemoveRenderProxy(collection, proxy_list, entity_id, attributes, bucket));
                }

                for (RenderProxy &proxy : added_proxies) {
                    const Handle<Mesh> &mesh = proxy.mesh;
                    AssertThrow(mesh.IsValid());
                    AssertThrow(mesh->IsReady());

                    const Handle<Material> &material = proxy.material;
                    AssertThrow(material.IsValid());

                    const RenderableAttributeSet attributes = GetRenderableAttributesForProxy(proxy, override_attributes);
                    const Bucket bucket = attributes.GetMaterialAttributes().bucket;

                    AddRenderProxy(collection, proxy_list, std::move(proxy), m_camera_render_resource_handle.Get(), this, attributes, bucket);
                }

                proxy_list.Advance(RenderProxyListAdvanceAction::PERSIST);
                
                // Clear out groups that are no longer used
                collection->RemoveEmptyProxyGroups();
            });
        }
    }

    render_proxy_list.Advance(RenderProxyListAdvanceAction::CLEAR);

    return collection_result;
}

void ViewRenderResource::PreFrameUpdate(FrameBase *frame)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    AssertThrow(IsInitialized());

    if (m_post_processing) {
        m_post_processing->PerformUpdates();
    }
}

void ViewRenderResource::Render(FrameBase *frame, WorldRenderResource *world_render_resource)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    AssertThrow(IsInitialized());

    if (m_gbuffer) {
        const uint32 frame_index = frame->GetFrameIndex();

        g_engine->GetRenderState()->BindCamera(m_camera_render_resource_handle);
        g_engine->GetRenderState()->SetActiveScene(m_scene_render_resource_handle->GetScene());

        const Handle<RenderEnvironment> &environment = m_scene_render_resource_handle->GetEnvironment();

        const FramebufferRef &opaque_fbo = m_gbuffer->GetBucket(Bucket::BUCKET_OPAQUE).GetFramebuffer();
        const FramebufferRef &lightmap_fbo = m_gbuffer->GetBucket(Bucket::BUCKET_LIGHTMAP).GetFramebuffer();
        const FramebufferRef &translucent_fbo = m_gbuffer->GetBucket(Bucket::BUCKET_TRANSLUCENT).GetFramebuffer();

        const TResourceHandle<EnvProbeRenderResource> &env_probe_render_resource_handle = g_engine->GetRenderState()->GetActiveEnvProbe();
        const TResourceHandle<EnvGridRenderResource> &env_grid_render_resource_handle = g_engine->GetRenderState()->GetActiveEnvGrid();

        const bool is_render_environment_ready = environment && environment->IsReady();

        const bool do_particles = true;
        const bool do_gaussian_splatting = false;//environment && environment->IsReady();

        const bool use_rt_radiance = m_renderer_config.path_tracer_enabled || m_renderer_config.rt_reflections_enabled;
        const bool use_ddgi = m_renderer_config.rt_gi_enabled;
        const bool use_hbao = m_renderer_config.hbao_enabled;
        const bool use_hbil = m_renderer_config.hbil_enabled;
        const bool use_ssgi = m_renderer_config.ssgi_enabled;

        const bool use_env_grid_irradiance = env_grid_render_resource_handle && m_renderer_config.env_grid_gi_enabled;
        const bool use_env_grid_radiance = env_grid_render_resource_handle && m_renderer_config.env_grid_radiance_enabled;

        const bool use_temporal_aa = m_temporal_aa != nullptr && m_renderer_config.taa_enabled;

        const bool use_reflection_probes = g_engine->GetRenderState()->bound_env_probes[ENV_PROBE_TYPE_SKY].Any()
            || g_engine->GetRenderState()->bound_env_probes[ENV_PROBE_TYPE_REFLECTION].Any();

        if (use_temporal_aa) {
            m_camera_render_resource_handle->ApplyJitter();
        }

        struct alignas(128)
        {
            uint32  flags;
            uint32  screen_width;
            uint32  screen_height;
        } deferred_data;

        Memory::MemSet(&deferred_data, 0, sizeof(deferred_data));

        deferred_data.flags |= use_hbao ? DEFERRED_FLAGS_HBAO_ENABLED : 0;
        deferred_data.flags |= use_hbil ? DEFERRED_FLAGS_HBIL_ENABLED : 0;
        deferred_data.flags |= use_rt_radiance ? DEFERRED_FLAGS_RT_RADIANCE_ENABLED : 0;
        deferred_data.flags |= use_ddgi ? DEFERRED_FLAGS_DDGI_ENABLED : 0;

        deferred_data.screen_width = m_gbuffer->GetExtent().x;
        deferred_data.screen_height = m_gbuffer->GetExtent().y;

        CollectDrawCalls(frame);
        
        if (is_render_environment_ready) {
            if (do_particles) {
                environment->GetParticleSystem()->UpdateParticles(frame, this);
            }

            if (do_gaussian_splatting) {
                environment->GetGaussianSplatting()->UpdateSplats(frame);
            }
        }

        m_indirect_pass->SetPushConstants(&deferred_data, sizeof(deferred_data));
        m_direct_pass->SetPushConstants(&deferred_data, sizeof(deferred_data));

        { // render opaque objects into separate framebuffer
            frame->GetCommandList().Add<BeginFramebuffer>(opaque_fbo, frame_index);

            ExecuteDrawCalls(frame, uint64(1u << BUCKET_OPAQUE));

            frame->GetCommandList().Add<EndFramebuffer>(opaque_fbo, frame_index);
        }

        if (use_env_grid_irradiance) { // submit env grid command buffer
            m_env_grid_irradiance_pass->SetPushConstants(&deferred_data, sizeof(deferred_data));
            m_env_grid_irradiance_pass->Render(frame, this);
        }

        if (use_env_grid_radiance) { // submit env grid command buffer
            m_env_grid_radiance_pass->SetPushConstants(&deferred_data, sizeof(deferred_data));
            m_env_grid_radiance_pass->Render(frame, this);
        }

        if (use_reflection_probes) {
            m_reflections_pass->SetPushConstants(&deferred_data, sizeof(deferred_data));
            m_reflections_pass->Render(frame, this);
        }

        // @FIXME: RT Radiance should be moved away from the RenderEnvironment and be part of the RenderView,
        // otherwise the same pass will be executed for each view (shared).
        // DDGI Should be a RenderSubsystem instead of existing on the RenderEnvironment directly,
        // so it is rendered with the WorldRenderResource rather than the ViewRenderResource.
        if (is_render_environment_ready) {
            if (use_rt_radiance) {
                environment->RenderRTRadiance(frame, this);
            }

            if (use_ddgi) {
                environment->RenderDDGIProbes(frame);
            }
        }

        if (use_hbao || use_hbil) {
            m_hbao->Render(frame, this);
        }

        if (use_ssgi) {
            bool is_sky_set = false;

            // Bind sky env probe for SSGI.
            if (g_engine->GetRenderState()->bound_env_probes[ENV_PROBE_TYPE_SKY].Any()) {
                g_engine->GetRenderState()->SetActiveEnvProbe(TResourceHandle<EnvProbeRenderResource>(g_engine->GetRenderState()->bound_env_probes[ENV_PROBE_TYPE_SKY].Front()));

                is_sky_set = true;
            }

            m_ssgi->Render(frame, this);

            if (is_sky_set) {
                g_engine->GetRenderState()->UnsetActiveEnvProbe();
            }
        }

        m_post_processing->RenderPre(frame, this);

        // render indirect and direct lighting into the same framebuffer
        const FramebufferRef &deferred_pass_framebuffer = m_indirect_pass->GetFramebuffer();

        { // deferred lighting on opaque objects
            frame->GetCommandList().Add<BeginFramebuffer>(deferred_pass_framebuffer, frame_index);

            m_indirect_pass->Render(frame, this);
            m_direct_pass->Render(frame, this);

            frame->GetCommandList().Add<EndFramebuffer>(deferred_pass_framebuffer, frame_index);
        }

        { // render objects to be lightmapped, separate from the opaque objects.
            // The lightmap bucket's framebuffer has a color attachment that will write into the opaque framebuffer's color attachment.
            frame->GetCommandList().Add<BeginFramebuffer>(lightmap_fbo, frame_index);

            ExecuteDrawCalls(frame, uint64(1u << BUCKET_LIGHTMAP));

            frame->GetCommandList().Add<EndFramebuffer>(lightmap_fbo, frame_index);
        }

        { // generate mipchain after rendering opaque objects' lighting, now we can use it for transmission
            const ImageRef &src_image = deferred_pass_framebuffer->GetAttachment(0)->GetImage();
            GenerateMipChain(frame, src_image);
        }

        { // translucent objects
            frame->GetCommandList().Add<BeginFramebuffer>(translucent_fbo, frame_index);

            { // Render the deferred lighting into the translucent pass framebuffer with a full screen quad.
                frame->GetCommandList().Add<BindGraphicsPipeline>(m_combine_pass->GetRenderGroup()->GetPipeline());

                frame->GetCommandList().Add<BindDescriptorTable>(
                    m_combine_pass->GetRenderGroup()->GetPipeline()->GetDescriptorTable(),
                    m_combine_pass->GetRenderGroup()->GetPipeline(),
                    ArrayMap<Name, ArrayMap<Name, uint32>> { },
                    frame_index
                );

                m_combine_pass->GetQuadMesh()->GetRenderResource().Render(frame->GetCommandList());
            }
            { // Render the objects to have lightmaps applied into the translucent pass framebuffer with a full screen quad.
                // Apply lightmaps over the now shaded opaque objects.
                m_lightmap_pass->RenderToFramebuffer(frame, this, translucent_fbo);
            }

            bool is_sky_set = false;

            // Bind sky env probe
            if (g_engine->GetRenderState()->bound_env_probes[ENV_PROBE_TYPE_SKY].Any()) {
                g_engine->GetRenderState()->SetActiveEnvProbe(TResourceHandle<EnvProbeRenderResource>(g_engine->GetRenderState()->bound_env_probes[ENV_PROBE_TYPE_SKY].Front()));

                is_sky_set = true;
            }

            // begin translucent with forward rendering
            ExecuteDrawCalls(frame, uint64(1u << BUCKET_TRANSLUCENT));
            ExecuteDrawCalls(frame, uint64(1u << BUCKET_DEBUG));

            if (is_render_environment_ready) {
                if (do_particles) {
                    environment->GetParticleSystem()->Render(frame, this);
                }

                if (do_gaussian_splatting) {
                    environment->GetGaussianSplatting()->Render(frame);
                }
            }

            if (is_sky_set) {
                g_engine->GetRenderState()->UnsetActiveEnvProbe();
            }

            ExecuteDrawCalls(frame, uint64(1u << BUCKET_SKYBOX));

            // render debug draw
            g_engine->GetDebugDrawer()->Render(frame);

            frame->GetCommandList().Add<EndFramebuffer>(translucent_fbo, frame_index);
        }

#if 0
        {
            struct alignas(128)
            {
                Vec2u   image_dimensions;
                uint32  _pad0, _pad1;
                uint32  deferred_flags;
            } deferred_combine_constants;

            deferred_combine_constants.image_dimensions = m_combine_pass->GetFramebuffer()->GetExtent();
            deferred_combine_constants.deferred_flags = deferred_data.flags;

            m_combine_pass->GetRenderGroup()->GetPipeline()->SetPushConstants(&deferred_combine_constants, sizeof(deferred_combine_constants));
            m_combine_pass->Begin(frame, this);

            frame->GetCommandList().Add<BindDescriptorTable>(
                m_combine_pass->GetRenderGroup()->GetPipeline()->GetDescriptorTable(),
                m_combine_pass->GetRenderGroup()->GetPipeline(),
                ArrayMap<Name, ArrayMap<Name, uint32>> {
                    {
                        NAME("Global"),
                        {
                            { NAME("ScenesBuffer"), ShaderDataOffset<SceneShaderData>(*m_scene_render_resource_handle) },
                            { NAME("CamerasBuffer"), ShaderDataOffset<CameraShaderData>(*m_camera_render_resource_handle) },
                            { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(env_grid_render_resource_handle.Get(), 0) },
                            { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(env_probe_render_resource_handle.Get(), 0) }
                        }
                    }
                },
                frame_index
            );

            const uint32 scene_descriptor_set_index = m_combine_pass->GetRenderGroup()->GetPipeline()->GetDescriptorTable()->GetDescriptorSetIndex(NAME("Scene"));

            if (scene_descriptor_set_index != ~0u) {
                frame->GetCommandList().Add<BindDescriptorSet>(
                    m_descriptor_sets[frame_index],
                    m_combine_pass->GetRenderGroup()->GetPipeline(),
                    ArrayMap<Name, uint32> { },
                    scene_descriptor_set_index
                );
            }

            m_combine_pass->GetQuadMesh()->GetRenderResource().Render(frame->GetCommandList());
            m_combine_pass->End(frame, this);
        }
#endif

        { // render depth pyramid
            m_depth_pyramid_renderer->Render(frame);
            // update culling info now that depth pyramid has been rendered
            m_cull_data.depth_pyramid_image_view = m_depth_pyramid_renderer->GetResultImageView();
            m_cull_data.depth_pyramid_dimensions = m_depth_pyramid_renderer->GetExtent();
        }

        m_post_processing->RenderPost(frame, this);

        m_tonemap_pass->Render(frame, this);

        if (use_temporal_aa) {
            m_temporal_aa->Render(frame, this);
        }

        // depth of field
        // m_dof_blur->Render(frame);

        g_engine->GetRenderState()->UnsetActiveScene();
        g_engine->GetRenderState()->UnbindCamera(m_camera_render_resource_handle.Get());
    }
}

void ViewRenderResource::CollectDrawCalls(FrameBase *frame)
{
    HYP_SCOPE;

    m_render_collector.CollectDrawCalls(
        frame,
        this,
        Bitset((1 << BUCKET_OPAQUE) | (1 << BUCKET_LIGHTMAP) | (1 << BUCKET_SKYBOX) | (1 << BUCKET_TRANSLUCENT) | (1 << BUCKET_DEBUG)),
        &m_cull_data
    );
}

void ViewRenderResource::ExecuteDrawCalls(FrameBase *frame, uint64 bucket_mask)
{
    HYP_SCOPE;

    m_render_collector.ExecuteDrawCalls(
        frame,
        this,
        nullptr,
        Bitset(bucket_mask),
        &m_cull_data
    );
}

void ViewRenderResource::GenerateMipChain(FrameBase *frame, const ImageRef &src_image)
{
    HYP_SCOPE;

    const uint32 frame_index = frame->GetFrameIndex();

    const ImageRef &mipmapped_result = m_mip_chain->GetRenderResource().GetImage();
    AssertThrow(mipmapped_result.IsValid());

    frame->GetCommandList().Add<InsertBarrier>(src_image, renderer::ResourceState::COPY_SRC);
    frame->GetCommandList().Add<InsertBarrier>(mipmapped_result, renderer::ResourceState::COPY_DST);

    // Blit into the mipmap chain img
    frame->GetCommandList().Add<BlitRect>(
        src_image,
        mipmapped_result,
        Rect<uint32> { 0, 0, src_image->GetExtent().x, src_image->GetExtent().y },
        Rect<uint32> { 0, 0, mipmapped_result->GetExtent().x, mipmapped_result->GetExtent().y }
    );

    frame->GetCommandList().Add<GenerateMipmaps>(mipmapped_result);

    frame->GetCommandList().Add<InsertBarrier>(src_image, renderer::ResourceState::SHADER_RESOURCE);
}

#pragma endregion ViewRenderResource

} // namespace hyperion
