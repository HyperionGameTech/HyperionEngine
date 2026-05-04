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

#include <input/Keyboard.hpp>

#include <engine/Game.hpp>

#include <CharacterControllerSystem.generated.inl>

namespace Hyperion {

#pragma region CharacterControllerInputHandler

Vec2f CharacterControllerInputHandler::GetMovementInput() const
{
    return Vec2f(m_strafe, m_forward);
}

bool CharacterControllerInputHandler::IsJumpPressed() const
{
    return m_jump;
}

bool CharacterControllerInputHandler::OnKeyDown(const KeyboardEvent& evt)
{
    switch (evt.keyCode)
    {
    case KeyCode::KEY_W:     m_forward =  1.0f; return true;
    case KeyCode::KEY_S:     m_forward = -1.0f; return true;
    case KeyCode::KEY_A:     m_strafe  = -1.0f; return true;
    case KeyCode::KEY_D:     m_strafe  =  1.0f; return true;
    case KeyCode::KEY_SPACE: m_jump    = true;  return true;
    default: break;
    }

    return false;
}

bool CharacterControllerInputHandler::OnKeyUp(const KeyboardEvent& evt)
{
    switch (evt.keyCode)
    {
    case KeyCode::KEY_W:
        if (m_forward > 0.0f) m_forward = 0.0f;
        return true;
    case KeyCode::KEY_S:
        if (m_forward < 0.0f) m_forward = 0.0f;
        return true;
    case KeyCode::KEY_A:
        if (m_strafe < 0.0f) m_strafe = 0.0f;
        return true;
    case KeyCode::KEY_D:
        if (m_strafe > 0.0f) m_strafe = 0.0f;
        return true;
    case KeyCode::KEY_SPACE:
        m_jump = false;
        return true;
    default:
        return false;
    }
}

#pragma endregion CharacterControllerInputHandler

#pragma region CharacterControllerSystem

bool CharacterControllerSystem::ShouldProcessScene(Scene* scene) const
{
    static constexpr EnumFlags<SceneFlags> ExpectedFlags = SceneFlags::FOREGROUND;

    return (scene->GetSceneFlags() & (SceneFlags::UI | SceneFlags::DETACHED | ExpectedFlags)) == ExpectedFlags;
}

void CharacterControllerSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    if (!ShouldProcessScene(entity->GetScene()))
        return;

    CharacterControllerComponent& component = entity->GetComponent<CharacterControllerComponent>();

    if (!component.inputHandler)
    {
        component.inputHandler = MakeHandle<CharacterControllerInputHandler>();
        InitObject(component.inputHandler);
    }

    TransformComponent& transformComponent = entity->GetComponent<TransformComponent>();
    component.translation = transformComponent.translation;

    CharacterControllerConfig config;
    config.shape = component.shape;
    config.startTranslation = component.translation;
    config.stepHeight = component.stepHeight;
    config.maxSlopeAngle = component.maxSlopeAngle;
    config.jumpSpeed = component.jumpSpeed;
    config.fallSpeed = component.fallSpeed;

    entity->GetWorld()->GetPhysicsWorld()->AddCharacterController(config, component.physicsHandle);
}

void CharacterControllerSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);

    if (!ShouldProcessScene(entity->GetScene()))
        return;

    CharacterControllerComponent& component = entity->GetComponent<CharacterControllerComponent>();

    if (component.physicsHandle)
    {
        entity->GetWorld()->GetPhysicsWorld()->RemoveCharacterController(component.physicsHandle);
    }
}

void CharacterControllerSystem::Process(float delta, Span<Handle<Scene>> scenes)
{
    HYP_SCOPE;

    for (Scene* scene : scenes)
    {
        if (!ShouldProcessScene(scene))
        {
            continue;
        }

        for (auto [entity, component, transformComponent] : scene->GetEntityManager()->GetEntitySet<CharacterControllerComponent, TransformComponent>().GetScopedView(GetComponentInfos()))
        {
            if (!component.physicsHandle)
            {
                continue;
            }

            Vec3f walkDirection = Vec3f::Zero();

            if (component.inputHandler)
            {
                CharacterControllerInputHandler* inputHandler = DynamicCast<CharacterControllerInputHandler>(component.inputHandler.Get());

                if (inputHandler)
                {
                    Vec2f movementInput = inputHandler->GetMovementInput();

                    if (movementInput.LengthSquared() > 0.0001f)
                    {
                        Vec3f forward = Vec3f(component.viewDirection.x, 0.0f, component.viewDirection.z).Normalize();
                        Vec3f right   = forward.Cross(Vec3f(0.0f, 1.0f, 0.0f)).Normalize();

                        walkDirection = (forward * movementInput.y + right * movementInput.x) * component.moveSpeed;
                    }

                    if (inputHandler->IsJumpPressed())
                    {
                        entity->GetWorld()->GetPhysicsWorld()->ApplyCharacterJump(component.physicsHandle);
                    }
                }
            }

            entity->GetWorld()->GetPhysicsWorld()->SetCharacterWalkDirection(component.physicsHandle, walkDirection);
            entity->GetWorld()->GetPhysicsWorld()->GetCharacterState(component.physicsHandle, component.translation, component.isOnGround);

            transformComponent.translation = component.translation;
        }
    }
}

#pragma endregion CharacterControllerSystem

} // namespace Hyperion
