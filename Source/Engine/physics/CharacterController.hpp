/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <Core/reflection/Handle.hpp>
#include <Core/reflection/ObjectMacros.hpp>

#include <Core/memory/UniquePtr.hpp>

#include <Core/math/Vector3.hpp>

#include <input/InputHandler.hpp>
#include <physics/RigidBody.hpp>

namespace Hyperion {

class CharacterController;

HYP_CLASS()
class HYP_API CharacterControllerInputHandler : public InputHandlerBase
{
    HYP_OBJECT_BODY(CharacterControllerInputHandler);

public:
    CharacterControllerInputHandler() = default;
    virtual ~CharacterControllerInputHandler() override = default;

    HYP_METHOD()
    Vec2f GetMovementInput() const;

    HYP_METHOD()
    bool IsJumpPressed() const;

protected:
    virtual bool OnKeyDown_Impl(const KeyboardEvent& evt) override;
    virtual bool OnKeyUp_Impl(const KeyboardEvent& evt) override;
    
    virtual bool OnMouseMove_Impl(const MouseEvent& evt) override { return false; }
    virtual bool OnMouseDrag_Impl(const MouseEvent& evt) override { return false; }
    virtual bool OnMouseLeave_Impl(const MouseEvent& evt) override { return false; }
    virtual bool OnClick_Impl(const MouseEvent& evt) override { return false; }
    virtual bool OnGainFocus_Impl(const MouseEvent& evt) override { return false; }
    virtual bool OnLoseFocus_Impl(const MouseEvent& evt) override { return false; }

private:
    float m_forward = 0.0f;
    float m_strafe = 0.0f;
    
    bool m_jump = false;
};

HYP_CLASS()
class HYP_API CharacterController : public ObjectBase
{
    HYP_OBJECT_BODY(CharacterController);

public:
    CharacterController();
    explicit CharacterController(const Handle<PhysicsShape>& shape);

    CharacterController(const CharacterController& other) = delete;
    CharacterController& operator=(const CharacterController& other) = delete;

    ~CharacterController();

    HYP_METHOD(Property = "Shape", Serialize)
    const Handle<PhysicsShape>& GetShape() const
    {
        return m_shape;
    }

    HYP_METHOD(Property = "Shape", Serialize)
    void SetShape(const Handle<PhysicsShape>& shape);

    HYP_METHOD(Property = "Translation", Transient)
    const Vec3f& GetTranslation() const
    {
        return m_translation;
    }

    /*! \internal */
    void SetTranslation(const Vec3f& translation)
    {
        m_translation = translation;
    }

    HYP_METHOD(Property = "StepHeight", Serialize)
    float GetStepHeight() const
    {
        return m_stepHeight;
    }

    HYP_METHOD(Property = "StepHeight", Serialize)
    void SetStepHeight(float stepHeight)
    {
        m_stepHeight = stepHeight;
    }

    HYP_METHOD(Property = "MaxSlopeAngle", Serialize)
    float GetMaxSlopeAngle() const
    {
        return m_maxSlopeAngle;
    }

    HYP_METHOD(Property = "MaxSlopeAngle", Serialize)
    void SetMaxSlopeAngle(float maxSlopeAngle)
    {
        m_maxSlopeAngle = maxSlopeAngle;
    }

    HYP_METHOD(Property = "JumpSpeed", Serialize)
    float GetJumpSpeed() const
    {
        return m_jumpSpeed;
    }

    HYP_METHOD(Property = "JumpSpeed", Serialize)
    void SetJumpSpeed(float jumpSpeed)
    {
        m_jumpSpeed = jumpSpeed;
    }

    HYP_METHOD(Property = "FallSpeed", Serialize)
    float GetFallSpeed() const
    {
        return m_fallSpeed;
    }

    HYP_METHOD(Property = "FallSpeed", Serialize)
    void SetFallSpeed(float fallSpeed)
    {
        m_fallSpeed = fallSpeed;
    }

    HYP_METHOD()
    bool IsOnGround() const
    {
        return m_isOnGround;
    }

    /*! \internal */
    void SetIsOnGround(bool isOnGround)
    {
        m_isOnGround = isOnGround;
    }

    HYP_METHOD()
    void SetWalkDirection(const Vec3f& velocity);

    HYP_METHOD()
    void Jump();

    HYP_FORCE_INLINE void* GetHandle() const
    {
        return m_handle.Get();
    }

    HYP_FORCE_INLINE void SetHandle(RC<void>&& handle)
    {
        m_handle = std::move(handle);
    }

private:
    void Init() override;

    Handle<PhysicsShape> m_shape;
    Vec3f m_translation = Vec3f::Zero();
    Vec3f m_walkDirection = Vec3f::Zero();
    float m_stepHeight = 0.35f;
    float m_maxSlopeAngle = 45.0f;
    float m_jumpSpeed = 10.0f;
    float m_fallSpeed = 55.0f;
    bool m_isOnGround = false;

    RC<void> m_handle;
};

} // namespace Hyperion
