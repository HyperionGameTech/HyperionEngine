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
#include <rendering/Deferred.hpp>
#include <rendering/DepthPyramidRenderer.hpp>
#include <rendering/PostFX.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/SSGI.hpp>
#include <rendering/SSRRenderer.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/lightmapper/RenderLightmapVolume.hpp>
#include <rendering/debug/DebugDrawer.hpp>

#include <scene/View.hpp>
#include <scene/Texture.hpp>
#include <scene/Mesh.hpp>
#include <scene/Material.hpp>
#include <scene/EnvGrid.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/Light.hpp>
#include <scene/lightmapper/LightmapVolume.hpp>

#include <core/math/MathUtil.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region RenderCollector helpers

static RenderableAttributeSet GetRenderableAttributesForProxy(const RenderProxy& proxy, const RenderableAttributeSet* override_attributes = nullptr)
{
    HYP_SCOPE;

    const Handle<Mesh>& mesh = proxy.mesh;
    AssertThrow(mesh.IsValid());

    const Handle<Material>& material = proxy.material;
    AssertThrow(material.IsValid());

    RenderableAttributeSet attributes {
        mesh->GetMeshAttributes(),
        material->GetRenderAttributes()
    };

    if (override_attributes)
    {
        if (const ShaderDefinition& override_shader_definition = override_attributes->GetShaderDefinition())
        {
            attributes.SetShaderDefinition(override_shader_definition);
        }

        const ShaderDefinition& shader_definition = override_attributes->GetShaderDefinition().IsValid()
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

        if (mesh_vertex_attributes != new_material_attributes.shader_definition.GetProperties().GetRequiredVertexAttributes())
        {
            new_material_attributes.shader_definition.properties.SetRequiredVertexAttributes(mesh_vertex_attributes);
        }

        // do not override bucket!
        new_material_attributes.bucket = attributes.GetMaterialAttributes().bucket;

        attributes.SetMaterialAttributes(new_material_attributes);
    }

    return attributes;
}

static void UpdateRenderableAttributesDynamic(const RenderProxy* proxy, RenderableAttributeSet& attributes)
{
    HYP_SCOPE;

    union
    {
        struct
        {
            bool has_instancing : 1;
            bool has_forward_lighting : 1;
        };

        uint64 overridden;
    };

    overridden = 0;

    has_instancing = proxy->instance_data.enable_auto_instancing || proxy->instance_data.num_instances > 1;
    has_forward_lighting = attributes.GetMaterialAttributes().bucket == RB_TRANSLUCENT;

    if (!overridden)
    {
        return;
    }

    bool shader_definition_changed = false;
    ShaderDefinition shader_definition = attributes.GetShaderDefinition();

    if (has_instancing)
    {
        shader_definition.GetProperties().Set("INSTANCING");
        shader_definition_changed = true;
    }

    if (has_forward_lighting)
    {
        shader_definition.GetProperties().Set("FORWARD_LIGHTING");
        shader_definition_changed = true;
    }

    if (shader_definition_changed)
    {
        // Update the shader definition in the attributes
        attributes.SetShaderDefinition(shader_definition);
    }
}

HYP_DISABLE_OPTIMIZATION;
static void AddRenderProxy(RenderProxyList* render_proxy_list, RenderProxyTracker& render_proxy_tracker, RenderProxy&& proxy, RenderCamera* render_camera, RenderView* render_view, const RenderableAttributeSet& attributes, RenderBucket rb)
{
    HYP_SCOPE;

    // Add proxy to group
    DrawCallCollectionMapping& mapping = render_proxy_list->mappings_by_bucket[rb][attributes];
    Handle<RenderGroup>& rg = mapping.render_group;

    if (!rg.IsValid())
    {
        EnumFlags<RenderGroupFlags> render_group_flags = RenderGroupFlags::DEFAULT;

        // Disable occlusion culling for translucent objects
        const RenderBucket rb = attributes.GetMaterialAttributes().bucket;

        if (rb == RB_TRANSLUCENT || rb == RB_DEBUG)
        {
            render_group_flags &= ~(RenderGroupFlags::OCCLUSION_CULLING | RenderGroupFlags::INDIRECT_RENDERING);
        }

        ShaderDefinition shader_definition = attributes.GetShaderDefinition();

        ShaderRef shader = g_shader_manager->GetOrCreate(shader_definition);

        if (!shader.IsValid())
        {
            HYP_LOG(Rendering, Error, "Failed to create shader for RenderProxy");

            return;
        }

        // Create RenderGroup
        rg = CreateObject<RenderGroup>(shader, attributes, render_group_flags);

        if (render_group_flags & RenderGroupFlags::INDIRECT_RENDERING)
        {
            AssertDebugMsg(mapping.indirect_renderer == nullptr, "Indirect renderer already exists on mapping");

            mapping.indirect_renderer = new IndirectRenderer();
            mapping.indirect_renderer->Create(rg->GetDrawCallCollectionImpl());

            mapping.draw_call_collection.impl = rg->GetDrawCallCollectionImpl();
        }

        AssertThrow(render_view->GetView() != nullptr);

        const ViewOutputTarget& output_target = render_view->GetView()->GetOutputTarget();

        const FramebufferRef& framebuffer = output_target.GetFramebuffer(attributes.GetMaterialAttributes().bucket);
        AssertThrow(framebuffer != nullptr);

        rg->AddFramebuffer(framebuffer);

        InitObject(rg);
    }

    const ID<Entity> entity_id = proxy.entity.GetID();
    auto iter = render_proxy_tracker.Track(entity_id, std::move(proxy));

    rg->AddRenderProxy(&iter->second);
}
HYP_ENABLE_OPTIMIZATION;

static bool RemoveRenderProxy(RenderProxyList* render_proxy_list, RenderProxyTracker& render_proxy_tracker, ID<Entity> entity, const RenderableAttributeSet& attributes, RenderBucket rb)
{
    HYP_SCOPE;

    auto& mappings = render_proxy_list->mappings_by_bucket[rb];

    auto it = mappings.Find(attributes);
    AssertThrow(it != mappings.End());

    const DrawCallCollectionMapping& mapping = it->second;
    AssertThrow(mapping.IsValid());

    const bool removed = mapping.render_group->RemoveRenderProxy(entity);

    render_proxy_tracker.MarkToRemove(entity);

    return removed;
}

#pragma endregion RenderCollector helpers

#pragma region RenderView

RenderView::RenderView(View* view)
    : m_view(view),
      m_viewport(view ? view->GetViewport() : Viewport {}),
      m_priority(view ? view->GetPriority() : 0)
{
}

RenderView::~RenderView()
{
}

void RenderView::Initialize_Internal()
{
    HYP_SCOPE;

    if (m_view)
    {
        AssertThrow(m_view->GetCamera().IsValid());

        m_render_camera = TResourceHandle<RenderCamera>(m_view->GetCamera()->GetRenderResource());
    }
}

void RenderView::Destroy_Internal()
{
    HYP_SCOPE;

    m_render_camera.Reset();
}

GBuffer* RenderView::GetGBuffer() const
{
    if (!m_view)
    {
        return nullptr;
    }

    return m_view->GetOutputTarget().GetGBuffer();
}

void RenderView::Update_Internal()
{
    HYP_SCOPE;
}

void RenderView::CreateRenderer()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);
}

void RenderView::DestroyRenderer()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);
}

void RenderView::SetViewport(const Viewport& viewport)
{
    HYP_SCOPE;

    Execute([this, viewport]()
        {
            if (m_viewport == viewport)
            {
                return;
            }

            m_viewport = viewport;

            if (IsInitialized() && (m_view->GetFlags() & ViewFlags::GBUFFER))
            {
            }
        });
}

void RenderView::SetPriority(int priority)
{
    HYP_SCOPE;

    Execute([this, priority]()
        {
            m_priority = priority;
        });
}

void RenderView::PreRender(FrameBase* frame)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_render_thread);

    AssertThrow(IsInitialized());

    // if (m_post_processing)
    // {
    //     m_post_processing->PerformUpdates();
    // }
}

void RenderView::PostRender(FrameBase* frame)
{
}

#pragma endregion RenderView

} // namespace hyperion
