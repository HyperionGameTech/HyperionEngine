/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <physics/CharacterController.hpp>
#include <physics/PhysicsWorld.hpp>

#include <scene/World.hpp>
#include <scene/Scene.hpp>

#include <engine/EngineDriver.hpp>
#include <engine/EngineGlobals.hpp>

#include <input/Keyboard.hpp>

#include <CharacterController.generated.inl>

namespace Hyperion {

static inline PhysicsWorld* GetPhysicsWorld()
{
    World* currentWorld = g_engineDriver->GetCurrentWorld();

    if (!currentWorld)
    {
        return nullptr;
    }

    return static_cast<PhysicsWorld*>(currentWorld->GetPhysicsWorld().Get());
}

#pragma region CharacterControllerInputHandler

Vec2f CharacterControllerInputHandler::GetMovementInput() const
{
    return Vec2f(m_strafe, m_forward);
}

bool CharacterControllerInputHandler::IsJumpPressed() const
{
    return m_jump;
}

bool CharacterControllerInputHandler::OnKeyDown_Impl(const KeyboardEvent& evt)
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

bool CharacterControllerInputHandler::OnKeyUp_Impl(const KeyboardEvent& evt)
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
        return false;
    }
}

#pragma endregion CharacterControllerInputHandler

#pragma region CharacterController

CharacterController::CharacterController()
    : CharacterController(Handle<PhysicsShape>::Null())
{
}

CharacterController::CharacterController(const Handle<PhysicsShape>& shape)
    : ObjectBase(),
      m_shape(shape)
{
}

CharacterController::~CharacterController() = default;

void CharacterController::Init()
{
    SetReady(true);
}

void CharacterController::SetShape(const Handle<PhysicsShape>& shape)
{
    m_shape = shape;
}

void CharacterController::SetWalkDirection(const Vec3f& velocity)
{
    m_walkDirection = velocity;

    PhysicsWorld* world = GetPhysicsWorld();

    if (world)
    {
        world->GetAdapter().SetCharacterWalkDirection(this, velocity);
    }
}

void CharacterController::Jump()
{
    PhysicsWorld* world = GetPhysicsWorld();

    if (world)
    {
        world->GetAdapter().ApplyCharacterJump(this);
    }
}

#pragma endregion CharacterController

} // namespace Hyperion
