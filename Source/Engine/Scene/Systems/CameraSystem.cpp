/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/Systems/CameraSystem.hpp>

#include <Scene/EntityManager.hpp>
#include <Scene/Scene.hpp>
#include <Scene/World.hpp>

#include <Scene/Camera/Camera.hpp>

#include <Framework/Game.hpp>

#include <CameraSystem.generated.inl>

namespace Hyperion {

bool CameraSystem::ShouldProcessScene(Scene* scene) const
{
    static constexpr EnumFlags<SceneFlags> ExpectedFlags = SceneFlags::FOREGROUND;

    return (scene->GetSceneFlags() & (SceneFlags::UI | SceneFlags::DETACHED | ExpectedFlags)) == ExpectedFlags;
}

void CameraSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    if (!ShouldProcessScene(entity->GetScene()))
        return;

    Camera* camera = static_cast<Camera*>(entity);

    const Handle<CameraController>& controller = camera->GetCameraController();

    if (controller && controller->GetInputHandler())
    {
        GetWorld()->GetGame()->RegisterInputHandler(controller->GetInputHandler());
    }
}

void CameraSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);

    if (!ShouldProcessScene(entity->GetScene()))
        return;

    Camera* camera = static_cast<Camera*>(entity);

    const Handle<CameraController>& controller = camera->GetCameraController();

    if (controller && controller->GetInputHandler())
    {
        GetWorld()->GetGame()->UnregisterInputHandler(controller->GetInputHandler());
    }
}

void CameraSystem::Process(float delta, Span<Handle<Scene>> scenes)
{
    // No per-frame processing needed; this system only manages input handler registration
}

} // namespace Hyperion
