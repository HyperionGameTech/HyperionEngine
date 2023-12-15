#include <scene/controllers/EnvGridController.hpp>

#include <scene/Scene.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/EnvGrid.hpp>
#include <Threads.hpp>

namespace hyperion::v2 {

EnvGridController::EnvGridController()
    : Controller(true),
      m_env_grid_renderer(nullptr)
{
}

void EnvGridController::AddEnvGridRenderer(const Handle<Scene> &scene)
{
    AssertThrow(scene.IsValid());

    if (scene->IsWorldScene()) {
        m_env_grid_renderer_name = HYP_NAME(TEMP_EnvGridRenderer0);
        m_env_grid_renderer_scene = scene;

        m_env_grid_renderer = scene->GetEnvironment()->AddRenderComponent<EnvGrid>(
            m_env_grid_renderer_name,
            EnvGridType::ENV_GRID_TYPE_LIGHT_FIELD,
            GetOwner()->GetWorldAABB(),
            Extent3D { 8, 4, 8 }
        );
    }

    UpdateGridTransform(GetOwner()->GetTransform());
}

void EnvGridController::RemoveEnvGridRenderer()
{
    if (!m_env_grid_renderer_name.IsValid()) {
        return;
    }

    if (Handle<Scene> scene = m_env_grid_renderer_scene.Lock()) {
        scene->GetEnvironment()->RemoveRenderComponent<EnvGrid>(m_env_grid_renderer_name);

        m_env_grid_renderer = nullptr;
        m_env_grid_renderer_name = Name::invalid;
        m_env_grid_renderer_scene.Reset();
    }
}

void EnvGridController::UpdateGridTransform(const Transform &transform)
{
    if (!m_env_grid_renderer) {
        return;
    }

    m_env_grid_renderer->SetCameraData(transform.GetTranslation());
}

void EnvGridController::OnAttachedToScene(ID<Scene> id)
{
    Threads::AssertOnThread(THREAD_GAME);

    if (m_env_grid_renderer_name.IsValid()) {
        return;
    }

    if (Handle<Scene> scene = Handle<Scene>(id)) {
        AddEnvGridRenderer(scene);
    }
}

void EnvGridController::OnDetachedFromScene(ID<Scene> id)
{
    Threads::AssertOnThread(THREAD_GAME);

    if (id == m_env_grid_renderer_scene.GetID()) {
        RemoveEnvGridRenderer();
    }
}

void EnvGridController::OnAdded()
{
    Threads::AssertOnThread(THREAD_GAME);
}

void EnvGridController::OnRemoved()
{
    Threads::AssertOnThread(THREAD_GAME);

    // OnDetachedFromScene() already handles removing the renderer
}

void EnvGridController::OnUpdate(GameCounter::TickUnit delta)
{
    Threads::AssertOnThread(THREAD_GAME);
}

void EnvGridController::OnTransformUpdate(const Transform &transform)
{
    Threads::AssertOnThread(THREAD_GAME);

    UpdateGridTransform(transform);
}

} // namespace hyperion::v2