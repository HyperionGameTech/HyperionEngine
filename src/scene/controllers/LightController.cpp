#include <scene/controllers/LightController.hpp>

#include <scene/Scene.hpp>
#include <rendering/Light.hpp>
#include <Threads.hpp>

namespace hyperion::v2 {

LightController::LightController(const Handle<Light> &light)
    : Controller(true),
      m_light(light)
{
}

void LightController::OnAttachedToScene(ID<Scene> id)
{
    Threads::AssertOnThread(THREAD_GAME);

    if (!m_light) {
        DebugLog(
            LogType::Warn,
            "LightController has no Light attached\n"
        );

        return;
    }

    OnTransformUpdate(GetOwner()->GetTransform());

    if (Handle<Scene> scene = Handle<Scene>(id)) {
        if (scene->IsWorldScene()) {
            scene->AddLight(m_light);
        }
    }
}

void LightController::OnDetachedFromScene(ID<Scene> id)
{
    Threads::AssertOnThread(THREAD_GAME);

    if (!m_light) {
        DebugLog(
            LogType::Warn,
            "LightController has no Light attached\n"
        );

        return;
    }

    if (Handle<Scene> scene = Handle<Scene>(id)) {
        if (scene->IsWorldScene()) {
            scene->RemoveLight(m_light->GetID());
        }
    }
}

void LightController::OnAdded()
{
}

void LightController::OnRemoved()
{
}

void LightController::OnUpdate(GameCounter::TickUnit delta)
{
    // light already gets updated by scene (This should change,
    // because a light could possibly be updated twice per tick
    // if attached to more than one world scene)

    if (!m_light) {
        return;
    }
}

void LightController::OnTransformUpdate(const Transform &transform)
{
    Threads::AssertOnThread(THREAD_GAME);

    if (!m_light) {
        return;
    }

    if (m_light->GetType() == LightType::DIRECTIONAL) {
        m_light->SetPosition((transform.GetTranslation()).Normalized());
    } else {
        m_light->SetPosition(transform.GetTranslation());
    }
}

} // namespace hyperion::v2