/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <ActiveRagdoll/RagdollMath.hpp>
#include <ActiveRagdoll/RagdollRig.hpp>

#include <Core/Reflection/ObjectMacros.hpp>

namespace JPH {
class BodyInterface;
} // namespace JPH

namespace Hyperion {

class Ragdoll;
class DebugDrawCommandList;

HYP_ENUM()
enum class RagdollBalanceState : uint8
{
    Off = 0,
    Standing,
    Stepping,
    Fallen
};

HYP_ENUM()
enum class RagdollFallReason : uint8
{
    None = 0,
    Tilt,
    Height,
    Contact,
    StepBudget
};

// limits of the centre of mass control: a horizontal force pair between the pelvis and the loaded feet doing the
// hip and ankle strategy's job, since the leg motors alone barely move the pelvis
HYP_STRUCT()
struct RagdollBalanceAssist
{
    HYP_STRUCT_BODY(RagdollBalanceAssist);

    // largest force as a fraction of body weight
    HYP_FIELD(Property = "MaxForceFraction", Serialize = true, Editor = true)
    float maxForceFraction = 0.225f;

    // the force fades out as the capture point moves this far outside the support (m); 0 never releases
    HYP_FIELD(Property = "ReleaseDistance", Serialize = true, Editor = true)
    float releaseDistance = 0.1f;

    // once released, a braking-only force up to this fraction of body weight still slows the body while it steps
    HYP_FIELD(Property = "RecoveryForceFraction", Serialize = true, Editor = true)
    float recoveryForceFraction = 0.75f;

    // how long a shove's stagger drift lasts (s)
    HYP_FIELD(Property = "StaggerDecayTime", Serialize = true, Editor = true)
    float staggerDecayTime = 0.5f;

    // how long each shove leaves the body unassisted (s), so it stumbles before it recovers
    HYP_FIELD(Property = "StumbleTime", Serialize = true, Editor = true)
    float stumbleTime = 0.3f;

    HYP_FORCE_INLINE bool operator==(const RagdollBalanceAssist& other) const
    {
        return maxForceFraction == other.maxForceFraction
            && releaseDistance == other.releaseDistance
            && recoveryForceFraction == other.recoveryForceFraction
            && staggerDecayTime == other.staggerDecayTime
            && stumbleTime == other.stumbleTime;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(maxForceFraction);
        hc.Add(releaseDistance);
        hc.Add(recoveryForceFraction);
        hc.Add(staggerDecayTime);
        hc.Add(stumbleTime);

        return hc;
    }
};

struct RagdollBalanceStatus
{
    RagdollBalanceState state = RagdollBalanceState::Off;
    RagdollFallReason fallReason = RagdollFallReason::None;
    uint32 steps = 0;
    uint32 recoverySteps = 0;
    bool isRecovering = false;
    bool isCalm = false;
    // horizontal offset from the support to the capture point, zero while it's over the feet
    JPH::Vec3 captureError = JPH::Vec3::sZero();
};

// a rig leg plus what balance senses and plans for it each step
struct RagdollBalanceLeg : RagdollRigLeg
{
    JPH::Vec3 anklePosition = JPH::Vec3::sZero();
    JPH::Quat footOrientation = JPH::Quat::sIdentity();
    JPH::Vec3 footCenter = JPH::Vec3::sZero();
    JPH::Vec3 contactCorners[RagdollMath::SoleCorners];
    uint32 contactCornerCount = 0;
    float soleHeight = 0.0f;
    float contactAge = 0.0f;
    bool isTouching = false;
    bool hasContact = false;

    bool isStance = true;
    JPH::Quat desiredThigh = JPH::Quat::sIdentity();
};

class RagdollBalance
{
public:
    static constexpr float ContactHoldTime = 0.08f;
    static constexpr float SwingPress = 0.03f;

    bool Init(Ragdoll& ragdoll);
    void Reset();
    void Clear(Ragdoll& ragdoll);

    void Step(Ragdoll& ragdoll, JPH::BodyInterface& bodyInterface, float deltaTime, JPH::Vec3Arg gravity, bool contactsFresh);

    void DebugDraw(DebugDrawCommandList& list) const;

    HYP_FORCE_INLINE bool IsValid() const
    {
        return m_isValid;
    }

    HYP_FORCE_INLINE void SetAssist(const RagdollBalanceAssist& assist)
    {
        m_assist = assist;
    }

    // the body may drift at this after a shove instead of being held still, so it staggers a few steps; it decays on its
    // own while balancing and clears when balance stops
    HYP_FORCE_INLINE void SetStaggerVelocity(JPH::Vec3Arg velocity)
    {
        m_staggerVelocity = velocity;

        if (velocity.LengthSq() > 0.0f)
        {
            m_stumbleTime = m_assist.stumbleTime;
        }
    }

    HYP_FORCE_INLINE JPH::Vec3 GetStaggerVelocity() const
    {
        return m_staggerVelocity;
    }

    HYP_FORCE_INLINE const RagdollBalanceStatus& GetStatus() const
    {
        return m_status;
    }

private:
    struct Swing
    {
        uint32 leg = 0;
        float time = 0.0f;
        JPH::Vec3 startAnkle = JPH::Vec3::sZero();
        JPH::Vec3 landing = JPH::Vec3::sZero();
        JPH::Vec3 ankleTarget = JPH::Vec3::sZero();
        bool isLifted = false;
        bool isFixed = false;
        bool isSwapped = false;
    };

    bool InitRestPose(const Ragdoll& ragdoll);

    void GatherState(Ragdoll& ragdoll, float deltaTime, JPH::Vec3Arg gravity, bool contactsFresh);
    void GatherFeet(Ragdoll& ragdoll, float deltaTime, bool contactsFresh);
    bool IsAnyBodyOnGround(Ragdoll& ragdoll) const;
    void BuildSupport();

    bool DetectFall();
    void Fall(RagdollFallReason reason);
    void UpdateStanding(float deltaTime);
    void UpdateStepping(float deltaTime);
    void UpdateClosingStep(float deltaTime);
    bool NeedsClosingStep() const;
    bool BeginStep();
    void StartSwing(uint32 swingLeg, JPH::Vec3Arg landing, bool isFixed);

    float GetWeightShare(uint32 legIndex) const;
    float GetCaptureDistance(const RagdollBalanceLeg& leg) const;
    float GetLandingCaptureError(uint32 stanceLeg, JPH::Vec3Arg landing) const;
    JPH::Vec3 PlanLanding(uint32 swingLeg, uint32 stanceLeg, float timeLeft) const;

    void DriveStanceLegs(Ragdoll& ragdoll);
    void DriveSwingLeg(Ragdoll& ragdoll);
    void DriveFreeLeg(Ragdoll& ragdoll, const RagdollBalanceLeg& leg, JPH::Vec3Arg ankle, JPH::QuatArg foot);
    void SolveLeg(Ragdoll& ragdoll, const RagdollBalanceLeg& leg, JPH::Vec3Arg hip, JPH::Vec3Arg ankle, JPH::Quat& outThigh, JPH::Quat& outShin) const;
    void ApplyTorsoControl(Ragdoll& ragdoll, JPH::BodyInterface& bodyInterface, JPH::Vec3Arg gravity);
    void ApplyCenterOfMassControl(Ragdoll& ragdoll, JPH::BodyInterface& bodyInterface);
    bool IsLoaded(const RagdollBalanceLeg& leg) const;

    JPH::Vec3 GetHeadingForward(JPH::QuatArg orientation, JPH::QuatArg rest) const;
    JPH::Quat GetHeadingRotation(JPH::Vec3Arg forward) const;
    JPH::Vec3 ToGround(JPH::Vec3Arg point) const;

    bool m_isValid = false;
    RagdollBalanceAssist m_assist;
    RagdollBalanceLeg m_legs[RagdollRig::LimbCount];

    const RagdollRig* m_rig = nullptr;

    JPH::Quat m_restPelvis = JPH::Quat::sIdentity();

    float m_restComHeight = 1.0f;
    float m_restWidth = 0.3f;

    RagdollMath::GravityFrame m_frame;
    JPH::Vec3 m_forward = JPH::Vec3::sAxisZ();
    float m_omega = 3.0f;
    float m_groundHeight = 0.0f;
    bool m_hasGround = false;
    bool m_isBodyOnGround = false;

    JPH::Vec3 m_centerOfMass = JPH::Vec3::sZero();
    JPH::Vec3 m_velocity = JPH::Vec3::sZero();
    JPH::Quat m_pelvisOrientation = JPH::Quat::sIdentity();
    JPH::Quat m_pelvisTarget = JPH::Quat::sIdentity();
    float m_trunkTilt = 0.0f;
    JPH::Vec3 m_leanDirection = JPH::Vec3::sAxisZ();
    JPH::Vec3 m_capturePoint = JPH::Vec3::sZero();
    JPH::Vec3 m_staggerVelocity = JPH::Vec3::sZero();
    float m_stumbleTime = 0.0f;

    uint32 m_supportCount = 0;
    JPH::Vec3 m_supportCenter = JPH::Vec3::sZero();
    JPH::Vec3 m_supportTarget = JPH::Vec3::sZero();
    float m_captureOutside = 0.0f;

    Swing m_swing;
    float m_doubleSupportTime = 0.0f;
    float m_loadWaitTime = 0.0f;
    float m_capturedTime = 0.0f;
    // how far the stance legs bend below their usual height after a landing, easing back out
    float m_landingDip = 0.0f;
    float m_calmTime = 0.0f;
    float m_settleTime = 0.0f;
    float m_shiftTime = 0.0f;
    int32 m_shiftLeg = -1;
    int32 m_lastSwingLeg = -1;
    RagdollBalanceStatus m_status;
};

} // namespace Hyperion
