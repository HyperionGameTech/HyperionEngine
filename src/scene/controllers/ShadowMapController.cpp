#include <scene/controllers/ShadowMapController.hpp>
#include <scene/controllers/LightController.hpp>

#include <scene/Scene.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/Shadows.hpp>
#include <Threads.hpp>

namespace hyperion::v2 {

ShadowMapController::ShadowMapController()
    : Controller(true),
      m_shadow_map_renderer(nullptr)
{
}

void ShadowMapController::AddShadowMapRenderer(const Handle<Scene> &scene)
{
    AssertThrow(scene.IsValid());

    if (scene->IsWorldScene()) {
        m_shadow_map_renderer_name = HYP_NAME(TEMP_ShadowMapRenderer0);
        m_shadow_map_renderer_scene = scene;

        m_shadow_map_renderer = scene->GetEnvironment()->AddRenderComponent<ShadowMapRenderer>(m_shadow_map_renderer_name);
    }
}

void ShadowMapController::RemoveShadowMapRenderer()
{
    if (!m_shadow_map_renderer_name.IsValid()) {
        return;
    }

    if (Handle<Scene> scene = m_shadow_map_renderer_scene.Lock()) {
        scene->GetEnvironment()->RemoveRenderComponent<ShadowMapRenderer>(m_shadow_map_renderer_name);

        m_shadow_map_renderer = nullptr;
        m_shadow_map_renderer_name = Name::invalid;
        m_shadow_map_renderer_scene.Reset();
    }
}

void ShadowMapController::OnAttachedToScene(ID<Scene> id)
{
    Threads::AssertOnThread(THREAD_GAME);

    if (m_shadow_map_renderer_name.IsValid()) {
        return;
    }

    if (Handle<Scene> scene = Handle<Scene>(id)) {
        AddShadowMapRenderer(scene);
    }
}

void ShadowMapController::OnDetachedFromScene(ID<Scene> id)
{
    Threads::AssertOnThread(THREAD_GAME);

    if (id == m_shadow_map_renderer_scene.GetID()) {
        RemoveShadowMapRenderer();
    }
}

void ShadowMapController::OnAdded()
{
    Threads::AssertOnThread(THREAD_GAME);

    // Find LightController attached to Entity
    if (LightController *light_controller = GetOwner()->GetController<LightController>()) {
        m_light = light_controller->GetLight();

        if (m_light) {
            // TEMP: Should refactor this so it is more able to be dynamically bound
            // and unbound, not dependent on a static index.
            m_light->SetShadowMapIndex(0);
        }
    }
}

void ShadowMapController::OnRemoved()
{
    Threads::AssertOnThread(THREAD_GAME);

    // OnDetachedFromScene() already handles removing the renderer

    m_light.Reset();
}

void ShadowMapController::OnUpdate(GameCounter::TickUnit delta)
{
    Threads::AssertOnThread(THREAD_GAME);
}

void ShadowMapController::OnTransformUpdate(const Transform &transform)
{
    Threads::AssertOnThread(THREAD_GAME);

    // Update camera data on transform change

    if (!m_light || !m_shadow_map_renderer) {
        return;
    }

    // For directional lights

    BoundingBox aabb = m_light->GetWorldAABB();
    const Vector3 center = aabb.GetCenter();

    const Vector3 light_direction = m_light->GetPosition().Normalized() * -1.0f;

    Handle<Camera> &camera = m_shadow_map_renderer->GetPass().GetScene()->GetCamera();

    if (!camera) {
        return;
    }

    camera->SetTranslation(center + light_direction);
    camera->SetTarget(center);
    
    auto corners = aabb.GetCorners();

    for (auto &corner : corners) {
        corner = camera->GetViewMatrix() * corner;

        aabb.max = MathUtil::Max(aabb.max, corner);
        aabb.min = MathUtil::Min(aabb.min, corner);
    }

    m_shadow_map_renderer->SetCameraData(ShadowMapCameraData {
        camera->GetViewMatrix(),
        camera->GetProjectionMatrix(),
        aabb
    });
}

} // namespace hyperion::v2