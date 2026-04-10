#pragma once

#include <Core/FxRef.hpp>
#include <Math/FxMathUtil.hpp>
#include <Math/FxVec2.hpp>
#include <Physics/PhPlayer.hpp>
#include <Renderer/FxCamera.hpp>

class FxPlayer
{
    const FxVec3f scMaxWalkSpeed = FxVec3f(5.0f);
    const FxVec3f scMaxSprintSpeed = FxVec3f(8.0f);

    static constexpr float32 scMovementLerpSpeed = 10.0f;

    // TODO: Break these out into config vars
    static constexpr float32 scSprintFov = 85.0f;
    static constexpr float32 scWalkingFov = 80.0f;

public:
    FxPlayer() = default;

    void Create();

    void Update(float64 delta_time);
    void MoveBy(const FxVec3f& by);

    void Jump();

    /**
     * @brief Move the player and its physics by `offset`.
     */
    void TeleportBy(const FxVec3f& offset)
    {
        SyncPhysicsToPlayer();

        Position += offset;
        Physics.Teleport(Position);
    }

    /**
     * @brief Move the player and its physics by `offset`.
     */
    void TeleportTo(const FxVec3f& position)
    {
        Position = position;
        Physics.Teleport(Position);
    }

    void SetFlyMode(bool value);
    bool IsFlyMode() const { return Physics.bDisableGravity; };

    void Move(float64 delta_time, const FxVec3f& offset);

    void RotateHead(const FxVec2f& xy)
    {
        pCamera->Rotate(xy.X, xy.Y);

        RequireDirectionUpdate();
    }

    ~FxPlayer();

private:
    FX_FORCE_INLINE void RequireDirectionUpdate() { mbUpdateDirection = true; }
    FX_FORCE_INLINE void RequirePhysicsUpdate() { mbUpdatePhysicsTransform = true; }

    FX_FORCE_INLINE void MarkApplyingUserForce() { mbIsApplyingUserForce = true; }

    FX_FORCE_INLINE void UpdateDirection()
    {
        if (!mbUpdateDirection) {
            return;
        }

        float32 s_anglex, c_anglex;
        FxMath::SinCos(pCamera->mAngleX, &s_anglex, &c_anglex);

        MovementDirection.Set(s_anglex, 0.0f, c_anglex);
        if (mbIsFlymode) {
            MovementDirection.Y = sin(pCamera->mAngleY);
        }

        // MovementDirection = FxVec3f(c_angley, s_angley, c_angley) * MovementDirection;
        // Clear the movement direction's Y. This is because sin(mAngleY) = 0 when mAngleY is zero.
        // MovementDirection.Y = 0.0f;

        // CameraDirection.NormalizeIP();

        mbUpdateDirection = false;
    }

    FX_FORCE_INLINE void SyncPhysicsToPlayer() { Position.FromJoltVec3(Physics.pPlayerVirt->GetPosition()); }

public:
    PhPlayer Physics;
    FxRef<FxPerspectiveCamera> pCamera { nullptr };

    /**
     * @brief The direction that the player is facing. This is the direction the player moves in and disregards the
     * pitch of the camera.
     */
    FxVec3f MovementDirection = FxVec3f::sForward;
    FxVec3f Position = FxVec3f::sZero;

    float JumpForce = 0.0f;

    bool bIsSprinting : 1 = false;

private:
    FxVec3f mCameraOffset = FxVec3f::sZero;
    FxVec3f mUserForce = FxVec3f::sZero;

    bool mbIsApplyingUserForce : 1 = false;
    bool mbUpdateDirection : 1 = true;
    bool mbUpdatePhysicsTransform : 1 = true;

    bool mbIsFlymode : 1 = false;
};
