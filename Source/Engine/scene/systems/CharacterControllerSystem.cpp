/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <scene/systems/CharacterControllerSystem.hpp>

#include <scene/EntityManager.hpp>
#include <scene/Scene.hpp>
#include <scene/World.hpp>

#include <physics/PhysicsWorld.hpp>
#include <physics/CharacterController.hpp>

#include <engine/Game.hpp>

#include <CharacterControllerSystem.generated.inl>

namespace Hyperion {

bool CharacterControllerSystem::ShouldProcessScene(Scene* scene) const
{
    static constexpr EnumFlags<SceneFlags> ExpectedFlags = SceneFlags::FOREGROUND;

    return (scene->GetSceneFlags() & (SceneFlags::UI | SceneFlags::DETACHED | ExpectedFlags)) == ExpectedFlags;
}

void CharacterControllerSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    CharacterControllerComponent& component = entity->GetEntityManager()->GetComponent<CharacterControllerComponent>(entity);

    if (!component.inputHandler)
    {
        component.inputHandler = MakeHandle<CharacterControllerInputHandler>();
        InitObject(component.inputHandler);
    }

    if (component.characterController)
    {
        InitObject(component.characterController);

        TransformComponent& transformComponent = entity->GetEntityManager()->GetComponent<TransformComponent>(entity);

        component.characterController->SetTranslation(transformComponent.translation);

        entity->GetWorld()->GetPhysicsWorld()->AddCharacterController(component.characterController);
    }
}

void CharacterControllerSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);

    CharacterControllerComponent& component = entity->GetEntityManager()->GetComponent<CharacterControllerComponent>(entity);

    if (component.characterController)
    {
        entity->GetWorld()->GetPhysicsWorld()->RemoveCharacterController(component.characterController);
    }
}

void CharacterControllerSystem::Process(float delta, Span<Handle<Scene>> scenes)
{
    HYP_SCOPE;

    if (!GetWorld()->GetGameState().IsSimulating())
    {
        return;
    }

    for (Scene* scene : scenes)
    {
        if (!ShouldProcessScene(scene))
        {
            continue;
        }

        for (auto [entity, component, transformComponent] : scene->GetEntityManager()->GetEntitySet<CharacterControllerComponent, TransformComponent>().GetScopedView(GetComponentInfos()))
        {
            Handle<CharacterController>& characterController = component.characterController;

            if (!characterController)
            {
                continue;
            }

            Vec3f walkDirection = Vec3f::Zero();

            if (component.inputHandler)
            {
                CharacterControllerInputHandler* inputHandler = ObjCast<CharacterControllerInputHandler>(component.inputHandler.Get());

                if (inputHandler)
                {
                    Vec2f movementInput = inputHandler->GetMovementInput(); // (strafe, forward)

                    if (movementInput.LengthSquared() > 0.0001f)
                    {
                        // Build a right vector from the view direction (flat on XZ plane)
                        Vec3f forward = Vec3f(component.viewDirection.x, 0.0f, component.viewDirection.z).Normalize();
                        Vec3f right   = forward.Cross(Vec3f(0.0f, 1.0f, 0.0f)).Normalize();

                        walkDirection = (forward * movementInput.y + right * movementInput.x) * component.moveSpeed;
                    }

                    if (inputHandler->IsJumpPressed())
                    {
                        characterController->Jump();
                    }
                }
            }

            characterController->SetWalkDirection(walkDirection);

            transformComponent.translation = characterController->GetTranslation();
        }
    }
}

} // namespace Hyperion
