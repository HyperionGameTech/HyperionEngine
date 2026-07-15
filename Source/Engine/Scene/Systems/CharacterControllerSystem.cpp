/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/Systems/CharacterControllerSystem.hpp>

#include <Scene/EntityManager.hpp>
#include <Scene/Scene.hpp>
#include <Scene/World.hpp>

#include <Physics/PhysicsWorld.hpp>

#include <Input/Keyboard.hpp>
#include <Input/InputManager.hpp>

#include <System/AppContext.hpp>

#include <Framework/Game.hpp>

#include <CharacterControllerSystem.generated.inl>

namespace Hyperion {

#pragma region CharacterControllerInputHandler

Vec2f CharacterControllerInputHandler::GetMovementInput() const
{
    float forward = 0.0f;
    float strafe = 0.0f;

    if (g_appContext != nullptr)
    {
        if (ApplicationWindow* mainWindow = g_appContext->GetMainWindow())
        {
            if (InputManager* inputManager = mainWindow->GetInputManager())
            {
                if (inputManager->IsKeyDown(KeyCode::KEY_W))
                    forward += 1.0f;
                if (inputManager->IsKeyDown(KeyCode::KEY_S))
                    forward -= 1.0f;
                if (inputManager->IsKeyDown(KeyCode::KEY_A))
                    strafe -= 1.0f;
                if (inputManager->IsKeyDown(KeyCode::KEY_D))
                    strafe += 1.0f;
            }
        }
    }

    return Vec2f(strafe, forward);
}

bool CharacterControllerInputHandler::IsJumpPressed() const
{
    if (g_appContext != nullptr)
    {
        if (ApplicationWindow* mainWindow = g_appContext->GetMainWindow())
        {
            if (InputManager* inputManager = mainWindow->GetInputManager())
            {
                return inputManager->IsKeyDown(KeyCode::KEY_SPACE);
            }
        }
    }

    return false;
}

bool CharacterControllerInputHandler::OnKeyDown(const KeyboardEvent& evt)
{
    switch (evt.keyCode)
    {
    case KeyCode::KEY_W:
        m_forward = 1.0f;
        return true;
    case KeyCode::KEY_S:
        m_forward = -1.0f;
        return true;
    case KeyCode::KEY_A:
        m_strafe = -1.0f;
        return true;
    case KeyCode::KEY_D:
        m_strafe = 1.0f;
        return true;
    case KeyCode::KEY_SPACE:
        m_jump = true;
        return true;
    default:
        break;
    }

    return false;
}

bool CharacterControllerInputHandler::OnKeyUp(const KeyboardEvent& evt)
{
    switch (evt.keyCode)
    {
    case KeyCode::KEY_W:
        if (m_forward > 0.0f)
            m_forward = 0.0f;
        return true;
    case KeyCode::KEY_S:
        if (m_forward < 0.0f)
            m_forward = 0.0f;
        return true;
    case KeyCode::KEY_A:
        if (m_strafe < 0.0f)
            m_strafe = 0.0f;
        return true;
    case KeyCode::KEY_D:
        if (m_strafe > 0.0f)
            m_strafe = 0.0f;
        return true;
    case KeyCode::KEY_SPACE:
        m_jump = false;
        return true;
    default:
        break;
    }

    return false;
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

        GetWorld()->GetGame()->RegisterInputHandler(component.inputHandler);
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

    if (!component.physicsHandle)
    {
        HYP_LOG(Scene, Error, "Failed to add CharacterController to physics world (physicsHandle was null) - Entity = {}", entity->GetName());
    }
}

void CharacterControllerSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);

    if (!ShouldProcessScene(entity->GetScene()))
        return;

    CharacterControllerComponent& component = entity->GetComponent<CharacterControllerComponent>();

    if (component.inputHandler)
    {
        GetWorld()->GetGame()->UnregisterInputHandler(component.inputHandler);
    }

    if (component.physicsHandle)
    {
        entity->GetWorld()->GetPhysicsWorld()->RemoveCharacterController(component.physicsHandle);
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

        for (auto [entity, component] : scene->GetEntityManager()->GetEntitySet<CharacterControllerComponent>().GetScopedView(GetComponentInfos()))
        {
            if (!component.physicsHandle)
            {
                HYP_LOG_ONCE(Scene, Warning, "physicsHandle is null for Entity {}'s character controller.", entity->GetName());
                continue;
            }

            Vec3f walkDirection;

            if (component.inputHandler)
            {
                CharacterControllerInputHandler* inputHandler = StaticCast<CharacterControllerInputHandler>(component.inputHandler.Get());

                Vec2f movementInput = inputHandler->GetMovementInput();

                if (movementInput.LengthSquared() > 0.0001f)
                {
                    Vec3f forward = Vec3f(component.viewDirection.x, 0.0f, component.viewDirection.z).Normalize();
                    Vec3f right = Vec3f(0.0f, 1.0f, 0.0f).Cross(forward).Normalize();

                    walkDirection = (forward * movementInput.y + right * movementInput.x) * component.moveSpeed;
                }

                if (inputHandler->IsJumpPressed())
                {
                    entity->GetWorld()->GetPhysicsWorld()->ApplyCharacterJump(component.physicsHandle);
                }
            }

            entity->GetWorld()->GetPhysicsWorld()->SetCharacterWalkDirection(component.physicsHandle, walkDirection);
            entity->GetWorld()->GetPhysicsWorld()->GetCharacterState(component.physicsHandle, component.translation, component.isOnGround);

            entity->SetWorldTranslation(component.translation, TransformChangeType::Simulation);
        }
    }
}

#pragma endregion CharacterControllerSystem

} // namespace Hyperion
