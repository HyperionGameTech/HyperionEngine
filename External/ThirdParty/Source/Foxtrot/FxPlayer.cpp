#include "FxPlayer.hpp"

#include <Core/FxRefUtil.hpp>
#include <FxEngine.hpp>
#include <Renderer/RxGlobals.hpp>
#include <Renderer/RxRenderBackend.hpp>

void FxPlayer::Create()
{
    pCamera = FxMakeRef<FxPerspectiveCamera>();

    pCamera->SetAspectRatio(gRenderer->Swapchain.GetAspectRatio());
    pCamera->SetFov(scWalkingFov);

    Physics.Create();

    // Since the physics position is the center of the capsule, we will use standing height / 2.
    mCameraOffset = FxVec3f(0, PhPlayer::scStandingHeight / 2.0f, 0);
}

void FxPlayer::MoveBy(const FxVec3f& by)
{
    Position += by;
    RequirePhysicsUpdate();
}

void FxPlayer::Jump()
{
    if (Physics.bIsGrounded && !mbIsFlymode) {
        JumpForce = 2.0f;
    }
}

void FxPlayer::SetFlyMode(bool value)
{
    mbIsFlymode = value;
    Physics.bDisableGravity = value;
}

void FxPlayer::Move(float64 delta_time, const FxVec3f& offset)
{
    const FxVec3f forward = MovementDirection * offset.Z;
    const FxVec3f right = MovementDirection.Cross(FxVec3f::sUp) * offset.X;
    const FxVec3f up = FxVec3f::sUp * offset.Y;


    FxVec3f movement_goal = (forward + right + up);

    if (movement_goal.Length() > 1e-3) {
        movement_goal.NormalizeIP();
    }

    mUserForce.SmoothInterpolate(movement_goal * (bIsSprinting ? scMaxSprintSpeed : scMaxWalkSpeed),
                                 scMovementLerpSpeed, delta_time);

    FxVec3f force = mUserForce;

    if (!mbIsFlymode) {
        force.Y = JumpForce;
        JumpForce = 0;
    }

    Physics.ApplyMovement(force);
}

void FxPlayer::Update(float64 delta_time)
{
    UpdateDirection();

    Physics.Update(delta_time);
    SyncPhysicsToPlayer();

    pCamera->MoveTo(Position + mCameraOffset);

    if (!mUserForce.IsNearZero(0.1)) {
        if (bIsSprinting && pCamera->GetFov() < scSprintFov) {
            pCamera->SetFov(FxMath::SmoothInterpolate(pCamera->GetFov(), scSprintFov, 8.0f, delta_time));
        }
        else if (!bIsSprinting && pCamera->GetFov() > scWalkingFov) {
            pCamera->SetFov(FxMath::SmoothInterpolate(pCamera->GetFov(), scWalkingFov, 13.0f, delta_time));
        }
    }
    pCamera->Update();

    mbUpdatePhysicsTransform = false;
}

FxPlayer::~FxPlayer() {}
