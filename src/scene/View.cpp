/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <scene/View.hpp>
#include <scene/Scene.hpp>
#include <scene/Light.hpp>
#include <scene/camera/Camera.hpp>

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/EntityTag.hpp>

#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>
#include <scene/ecs/components/LightComponent.hpp>

#include <rendering/RenderView.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderLight.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <Engine.hpp>

namespace hyperion {

#pragma region View

View::View()
    : View(ViewDesc { })
{
}

View::View(const ViewDesc &view_desc)
    : m_render_resource(nullptr),
      m_flags(view_desc.flags),
      m_viewport(view_desc.viewport),
      m_scene(view_desc.scene),
      m_camera(view_desc.camera),
      m_priority(view_desc.priority)
{
}

View::~View()
{
    if (m_render_resource) {
        FreeResource(m_render_resource);
        m_render_resource = nullptr;
    }
}

void View::Init()
{
    if (IsInitCalled()) {
        return;
    }

    HypObject::Init();

    AddDelegateHandler(g_engine->GetDelegates().OnShutdown.Bind([this]()
    {
        if (m_render_resource) {
            FreeResource(m_render_resource);
            m_render_resource = nullptr;
        }
    }));

    AssertThrowMsg(m_camera.IsValid(), "Camera is not valid for View with ID #%u", GetID().Value());
    AssertThrowMsg(m_scene.IsValid(), "Scene is not valid for View with ID #%u", GetID().Value());

    InitObject(m_scene);
    InitObject(m_camera);

    m_render_resource = AllocateResource<ViewRenderResource>(this);
    m_render_resource->SetScene(TResourceHandle<SceneRenderResource>(m_scene->GetRenderResource()));
    m_render_resource->SetCamera(TResourceHandle<CameraRenderResource>(m_camera->GetRenderResource()));
    m_render_resource->SetViewport(m_viewport);

    if (m_lights.Any()) {
        Array<TResourceHandle<LightRenderResource>> lights;
        lights.Reserve(m_lights.Size());

        for (const Handle<Light> &light : m_lights) {
            if (!light) {
                continue;
            }

            InitObject(light);

            lights.PushBack(TResourceHandle<LightRenderResource>(light->GetRenderResource()));
        }

        m_render_resource->SetLights(std::move(lights));
    }

    SetReady(true);
}

void View::Update(GameCounter::TickUnit delta)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread | ThreadCategory::THREAD_CATEGORY_TASK);

    AssertReady();

    CollectLights();
    CollectEntities();
}

void View::SetViewport(const Viewport &viewport)
{
    m_viewport = viewport;

    if (IsInitCalled()) {
        m_render_resource->SetViewport(viewport);
    }
}

void View::SetPriority(int priority)
{
    m_priority = priority;

    if (IsInitCalled()) {
        m_render_resource->SetPriority(priority);
    }
}

void View::AddLight(const Handle<Light> &light)
{
    if (!light) {
        return;
    }

    InitObject(light);

    auto it = m_lights.Find(light);

    if (it != m_lights.End()) {
        return;
    }

    m_lights.PushBack(light);

    if (IsInitCalled()) {
        m_render_resource->AddLight(TResourceHandle<LightRenderResource>(light->GetRenderResource()));
    }
}

void View::RemoveLight(const Handle<Light> &light)
{
    if (!light) {
        return;
    }

    auto it = m_lights.Find(light);

    if (it == m_lights.End()) {
        return;
    }

    if (IsInitCalled()) {
        if (!light->IsReady()) {
            return;
        }

        m_render_resource->RemoveLight(&light->GetRenderResource());
    }

    m_lights.Erase(it);
}

RenderCollector::CollectionResult View::CollectEntities(bool skip_frustum_culling)
{
    AssertReady();

    return CollectEntities(m_render_resource->GetRenderCollector(), skip_frustum_culling);
}

RenderCollector::CollectionResult View::CollectEntities(RenderCollector &render_collector, bool skip_frustum_culling)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread | ThreadCategory::THREAD_CATEGORY_TASK);

    AssertReady();

    if (!m_camera.IsValid()) {
        return m_render_resource->UpdateRenderCollector();
    }

    const ID<Camera> camera_id = m_camera.GetID();

    const VisibilityStateSnapshot visibility_state_snapshot = m_scene->GetOctree().GetVisibilityState().GetSnapshot(camera_id);
    
    RenderProxyList &proxy_list = render_collector.GetDrawCollection()->GetProxyList(ThreadType::THREAD_TYPE_GAME);

    uint32 num_collected_entities = 0;
    uint32 num_skipped_entities = 0;

    for (auto [entity_id, mesh_component, transform_component, bounding_box_component, visibility_state_component] : m_scene->GetEntityManager()->GetEntitySet<MeshComponent, TransformComponent, BoundingBoxComponent, VisibilityStateComponent>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT)) {
        if (!skip_frustum_culling && !(visibility_state_component.flags & VISIBILITY_STATE_FLAG_ALWAYS_VISIBLE)) {
#ifndef HYP_DISABLE_VISIBILITY_CHECK
            if (!visibility_state_component.visibility_state) {
#ifdef HYP_VISIBILITY_CHECK_DEBUG
                ++num_skipped_entities;
#endif
                continue;
            }

            if (!visibility_state_component.visibility_state->GetSnapshot(camera_id).ValidToParent(visibility_state_snapshot)) {
#ifdef HYP_VISIBILITY_CHECK_DEBUG
                ++num_skipped_entities;
#endif

                continue;
            }
#endif
        }

        ++num_collected_entities;

        AssertThrow(mesh_component.proxy != nullptr);

        render_collector.PushRenderProxy(proxy_list, *mesh_component.proxy);
    }
    
#ifdef HYP_VISIBILITY_CHECK_DEBUG
    HYP_LOG(Scene, Debug, "Collected {} entities for camera {}, {} skipped", num_collected_entities, camera->GetName(), num_skipped_entities);
#endif

    return m_render_resource->UpdateRenderCollector();
}

RenderCollector::CollectionResult View::CollectDynamicEntities(bool skip_frustum_culling)
{
    AssertReady();

    return CollectDynamicEntities(m_render_resource->GetRenderCollector(), skip_frustum_culling);
}

RenderCollector::CollectionResult View::CollectDynamicEntities(RenderCollector &render_collector, bool skip_frustum_culling)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread | ThreadCategory::THREAD_CATEGORY_TASK);

    AssertReady();

    if (!m_camera.IsValid()) {
        // if camera is invalid, update without adding any entities
        return m_render_resource->UpdateRenderCollector();
    }
    
    RenderProxyList &proxy_list = render_collector.GetDrawCollection()->GetProxyList(ThreadType::THREAD_TYPE_GAME);

    const ID<Camera> camera_id = m_camera.GetID();
    
    const VisibilityStateSnapshot visibility_state_snapshot = m_scene->GetOctree().GetVisibilityState().GetSnapshot(camera_id);

    for (auto [entity_id, mesh_component, transform_component, bounding_box_component, visibility_state_component, _] : m_scene->GetEntityManager()->GetEntitySet<MeshComponent, TransformComponent, BoundingBoxComponent, VisibilityStateComponent, EntityTagComponent<EntityTag::DYNAMIC>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT)) {
        if (!skip_frustum_culling && !(visibility_state_component.flags & VISIBILITY_STATE_FLAG_ALWAYS_VISIBLE)) {
#ifndef HYP_DISABLE_VISIBILITY_CHECK
            if (!visibility_state_component.visibility_state) {
                continue;
            }


            if (!visibility_state_component.visibility_state->GetSnapshot(camera_id).ValidToParent(visibility_state_snapshot)) {
#ifdef HYP_VISIBILITY_CHECK_DEBUG
                HYP_LOG(Scene, Debug, "Skipping entity #{} for camera #{} due to visibility state being invalid.",
                    entity_id.Value(), camera_id.Value());
#endif

                continue;
            }
#endif
        }

        AssertThrow(mesh_component.proxy != nullptr);

        render_collector.PushRenderProxy(proxy_list, *mesh_component.proxy);
    }

    return m_render_resource->UpdateRenderCollector();
}

RenderCollector::CollectionResult View::CollectStaticEntities(bool skip_frustum_culling)
{
    AssertReady();

    return CollectStaticEntities(m_render_resource->GetRenderCollector(), skip_frustum_culling);
}

RenderCollector::CollectionResult View::CollectStaticEntities(RenderCollector &render_collector, bool skip_frustum_culling)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_game_thread | ThreadCategory::THREAD_CATEGORY_TASK);

    AssertReady();

    if (!m_camera.IsValid()) {
        // if camera is invalid, update without adding any entities
        return m_render_resource->UpdateRenderCollector();
    }
    
    RenderProxyList &proxy_list = render_collector.GetDrawCollection()->GetProxyList(ThreadType::THREAD_TYPE_GAME);

    const ID<Camera> camera_id = m_camera.GetID();
    
    const VisibilityStateSnapshot visibility_state_snapshot = m_scene->GetOctree().GetVisibilityState().GetSnapshot(camera_id);

    for (auto [entity_id, mesh_component, transform_component, bounding_box_component, visibility_state_component, _] : m_scene->GetEntityManager()->GetEntitySet<MeshComponent, TransformComponent, BoundingBoxComponent, VisibilityStateComponent, EntityTagComponent<EntityTag::STATIC>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT)) {
        if (!skip_frustum_culling && !(visibility_state_component.flags & VISIBILITY_STATE_FLAG_ALWAYS_VISIBLE)) {
#ifndef HYP_DISABLE_VISIBILITY_CHECK
            if (!visibility_state_component.visibility_state) {
                continue;
            }

            if (!visibility_state_component.visibility_state->GetSnapshot(camera_id).ValidToParent(visibility_state_snapshot)) {
#ifdef HYP_VISIBILITY_CHECK_DEBUG
                HYP_LOG(Scene, Debug, "Skipping entity #{} for camera #{} due to visibility state being invalid.",
                    entity_id.Value(), camera_id.Value());
#endif

                continue;
            }
#endif
        }

        AssertThrow(mesh_component.proxy != nullptr);

        render_collector.PushRenderProxy(proxy_list, *mesh_component.proxy);
    }

    return m_render_resource->UpdateRenderCollector();
}

void View::CollectLights()
{
    HYP_SCOPE;

    for (auto [entity_id, light_component, transform_component, bounding_box_component, visibility_state_component] : m_scene->GetEntityManager()->GetEntitySet<LightComponent, TransformComponent, BoundingBoxComponent, VisibilityStateComponent>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT)) {
        if (!(visibility_state_component.flags & VISIBILITY_STATE_FLAG_ALWAYS_VISIBLE)) {
            bool is_light_in_frustum = false;
            is_light_in_frustum = m_camera->GetFrustum().ContainsAABB(bounding_box_component.world_aabb);

            switch (light_component.light->GetLightType()) {
            case LightType::DIRECTIONAL:
                is_light_in_frustum = true;
                break;
            case LightType::POINT:
                is_light_in_frustum = m_camera->GetFrustum().ContainsBoundingSphere(light_component.light->GetBoundingSphere());
                break;
            case LightType::SPOT:
                // @TODO Implement frustum culling for spot lights
                break;
            case LightType::AREA_RECT:
                is_light_in_frustum = true;
                break;
            default:
                break;
            }

            light_component.light->SetIsVisible(camera.GetID(), is_light_in_frustum);
        }
    }
}

#pragma endregion View

} // namespace hyperion
