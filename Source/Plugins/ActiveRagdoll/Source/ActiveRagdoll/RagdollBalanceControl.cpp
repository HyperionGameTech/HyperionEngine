/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <ActiveRagdoll/RagdollBalance.hpp>
#include <ActiveRagdoll/Ragdoll.hpp>

#include <Jolt/Physics/Body/BodyInterface.h>

#include <limits>

namespace Hyperion {

using namespace RagdollMath;

namespace {

static constexpr float StancePress = 0.01f;
static constexpr float LiftStart = 0.003f;
static constexpr float LiftRange = 0.015f;

static constexpr float StanceHeightFraction = 0.97f;
static constexpr float MinHipHeight = 0.25f;
static constexpr float StanceFrequencyScale = 0.15f;
static constexpr float MaxComShift = 0.12f;
static constexpr float ComShiftGain = 3.0f;

static constexpr float TorsoFrequency = 4.0f;
static constexpr float TorsoMaxTorque = 300.0f;
static constexpr float GravityFeedForward = 1.0f;
static constexpr float GravityFullStrength = 0.25f;

// centre of mass control frequency (Hz) and damping ratio; its force limits come from RagdollBalanceAssist
static constexpr float ComFrequency = 2.0f;
static constexpr float ComDamping = 1.0f;
// stagger speed (m/s) at which holding the centre of mass over the feet has given way to following the stagger
static constexpr float StaggerHoldSpeed = 0.25f;

JPH::Vec3 ClampLength(JPH::Vec3Arg v, float maxLength)
{
    const float length = v.Length();

    if (length <= maxLength)
    {
        return v;
    }

    return v * (maxLength / length);
}

} // namespace

bool RagdollBalance::IsLoaded(const RagdollBalanceLeg& leg) const
{
    return leg.isStance && leg.contactAge < ContactHoldTime;
}

void RagdollBalance::DriveStanceLegs(Ragdoll& ragdoll)
{
    const JPH::Quat pelvisTarget = m_pelvisTarget;

    JPH::Vec3 center = m_supportTarget;

    if (m_shiftLeg >= 0)
    {
        center = m_legs[1 - m_shiftLeg].footCenter;
    }

    const JPH::Vec3 shift = ClampLength(m_frame.Horizontal(center - m_capturePoint) * ComShiftGain, MaxComShift);
    const JPH::Vec3 pelvisHorizontal = m_frame.Horizontal(ragdoll.m_states[0].origin) + shift;

    float pelvisHeight = std::numeric_limits<float>::max();

    for (const RagdollBalanceLeg& leg : m_legs)
    {
        if (!leg.isStance)
        {
            continue;
        }

        const JPH::Vec3 hipOffset = pelvisTarget * leg.hipOffset;
        const float reach = StanceHeightFraction * (leg.upper + leg.lower);
        const float spread = m_frame.Horizontal(pelvisHorizontal + hipOffset - leg.anklePosition).Length();

        const float reachSq = reach * reach;
        const float spreadSq = spread * spread;

        const float hipHeight = std::sqrt(MathUtil::Max(reachSq - spreadSq, reachSq * MinHipHeight));

        pelvisHeight = MathUtil::Min(pelvisHeight, leg.anklePosition.Dot(m_frame.up) + hipHeight - hipOffset.Dot(m_frame.up));
    }

    if (pelvisHeight == std::numeric_limits<float>::max())
    {
        return;
    }

    const JPH::Vec3 pelvis = pelvisHorizontal + m_frame.up * (pelvisHeight - m_landingDip);

    for (RagdollBalanceLeg& leg : m_legs)
    {
        if (!leg.isStance)
        {
            continue;
        }

        const JPH::Vec3 hip = pelvis + pelvisTarget * leg.hipOffset;

        const JPH::Vec3 footForward = GetHeadingForward(leg.footOrientation, leg.restFoot);
        const JPH::Quat foot = GetHeadingRotation(footForward) * leg.restFoot;

        const float soleLift = MathUtil::Max(leg.soleHeight - m_groundHeight, 0.0f);

        if (!IsLoaded(leg))
        {
            DriveFreeLeg(ragdoll, leg, leg.anklePosition - m_frame.up * (soleLift + SwingPress), foot);

            continue;
        }

        const JPH::Vec3 ankle = leg.anklePosition - m_frame.up * (soleLift + StancePress);

        JPH::Quat thigh;
        JPH::Quat shin;
        SolveLeg(ragdoll, leg, hip, ankle, thigh, shin);

        leg.desiredThigh = thigh;

        // the knee and ankle carry the body above them, so their stiffness comes from the supported inertia
        ragdoll.SetMotorTarget(leg.shin, thigh, shin, StanceFrequencyScale, true);
        ragdoll.SetMotorTarget(leg.foot, shin, foot, StanceFrequencyScale, true);

        ragdoll.m_motorOverrides[leg.thigh].skipGravityComp = true;
        ragdoll.m_motorOverrides[leg.shin].skipGravityComp = true;
        ragdoll.m_motorOverrides[leg.foot].skipGravityComp = true;
    }
}

void RagdollBalance::DriveSwingLeg(Ragdoll& ragdoll)
{
    const RagdollBalanceLeg& leg = m_legs[m_swing.leg];

    DriveFreeLeg(ragdoll, leg, m_swing.ankleTarget, GetHeadingRotation(m_forward) * leg.restFoot);
}

void RagdollBalance::DriveFreeLeg(Ragdoll& ragdoll, const RagdollBalanceLeg& leg, JPH::Vec3Arg ankle, JPH::QuatArg foot)
{
    JPH::Quat thigh;
    JPH::Quat shin;
    SolveLeg(ragdoll, leg, ragdoll.m_states[leg.thigh].origin, ankle, thigh, shin);

    ragdoll.SetMotorTarget(leg.thigh, m_pelvisOrientation, thigh);
    ragdoll.SetMotorTarget(leg.shin, thigh, shin);
    ragdoll.SetMotorTarget(leg.foot, shin, foot);
}

void RagdollBalance::SolveLeg(Ragdoll& ragdoll, const RagdollBalanceLeg& leg, JPH::Vec3Arg hip, JPH::Vec3Arg ankle, JPH::Quat& outThigh, JPH::Quat& outShin) const
{
    const TwoBoneIKResult ik = SolveTwoBoneIK(hip, ankle, leg.upper, leg.lower, m_forward);

    const JPH::Vec3 thighDirection = (ik.mid - hip).NormalizedOr(-m_frame.up);
    const JPH::Vec3 shinDirection = (ik.end - ik.mid).NormalizedOr(-m_frame.up);
    const JPH::Vec3 legDirection = (ik.end - hip).NormalizedOr(-m_frame.up);

    const JPH::Vec3 hinge = legDirection.Cross(m_forward).NormalizedOr(m_frame.up.Cross(m_forward)) * leg.hingeSign;

    ragdoll.GetHingeChainRotations(leg.thigh, leg.shin, thighDirection, shinDirection, hinge, outThigh, outShin);
}

void RagdollBalance::ApplyTorsoControl(Ragdoll& ragdoll, JPH::BodyInterface& bodyInterface, JPH::Vec3Arg gravity)
{
    uint32 stanceLegs[2];
    uint32 stanceCount = 0;

    for (uint32 i = 0; i < 2; i++)
    {
        if (IsLoaded(m_legs[i]))
        {
            stanceLegs[stanceCount++] = i;
        }
    }

    if (stanceCount == 0)
    {
        return;
    }

    JPH::Vec3 pivot = JPH::Vec3::sZero();
    float upperMass = ragdoll.m_states[0].subtreeMass;
    JPH::Vec3 upperMoment = ragdoll.m_states[0].subtreeMoment;

    for (uint32 i = 0; i < stanceCount; i++)
    {
        const Ragdoll::BodyState& thigh = ragdoll.m_states[m_legs[stanceLegs[i]].thigh];

        pivot += thigh.origin;
        upperMass -= thigh.subtreeMass;
        upperMoment -= thigh.subtreeMoment;
    }

    pivot /= float(stanceCount);

    if (upperMass <= 0.0f)
    {
        return;
    }

    float inertia = ragdoll.GetRawSubtreeInertia(0, pivot);

    for (uint32 i = 0; i < stanceCount; i++)
    {
        inertia -= ragdoll.GetRawSubtreeInertia(m_legs[stanceLegs[i]].thigh, pivot);
    }

    inertia = MathUtil::Max(inertia, 1e-2f);

    float weights[2] = { 1.0f, 0.0f };

    if (stanceCount == 2)
    {
        const float distanceA = m_frame.Horizontal(m_centerOfMass - m_legs[stanceLegs[0]].footCenter).Length();
        const float distanceB = m_frame.Horizontal(m_centerOfMass - m_legs[stanceLegs[1]].footCenter).Length();

        weights[0] = distanceB / MathUtil::Max(distanceA + distanceB, 1e-4f);
        weights[1] = 1.0f - weights[0];
    }

    const JPH::Quat pelvisTarget = m_pelvisTarget;
    const float frequencyScale = TorsoFrequency / ragdoll.m_bodies[m_legs[0].thigh].muscle.stiffness;

    for (uint32 i = 0; i < stanceCount; i++)
    {
        const RagdollBalanceLeg& leg = m_legs[stanceLegs[i]];
        const JPH::Quat thigh = ragdoll.m_states[leg.thigh].orientation;
        const float lift = MathUtil::Clamp((leg.soleHeight - m_groundHeight - LiftStart) / LiftRange, 0.0f, 1.0f);
        const JPH::Quat hipThigh = thigh.SLERP(leg.desiredThigh, lift).Normalized();

        Ragdoll::MotorOverride& hipMotor = ragdoll.SetMotorTarget(leg.thigh, pelvisTarget, hipThigh, frequencyScale);
        hipMotor.inertia = MathUtil::Max(inertia * weights[i], 1e-2f);
    }

    const JPH::Vec3 upperCenter = upperMoment / upperMass;

    JPH::Vec3 torque = -(upperCenter - pivot).Cross(gravity * upperMass) * GravityFeedForward * MathUtil::Min(ragdoll.m_bodies[0].strength / GravityFullStrength, 1.0f);
    torque = ClampLength(torque, TorsoMaxTorque);

    ragdoll.AddStepTorque(bodyInterface, ragdoll.GetBodyID(0), torque);

    for (uint32 i = 0; i < stanceCount; i++)
    {
        ragdoll.AddStepTorque(bodyInterface, ragdoll.GetBodyID(m_legs[stanceLegs[i]].thigh), -torque * weights[i]);
    }
}

void RagdollBalance::ApplyCenterOfMassControl(Ragdoll& ragdoll, JPH::BodyInterface& bodyInterface)
{
    // the body keeps moving back through the stumble, so the hold and braking wait until it has recovered
    if (m_stumbleTime > 0.0f)
    {
        return;
    }

    uint32 loaded[2];
    uint32 loadedCount = 0;

    for (uint32 i = 0; i < 2; i++)
    {
        if (IsLoaded(m_legs[i]))
        {
            loaded[loadedCount++] = i;
        }
    }

    if (loadedCount == 0)
    {
        return;
    }

    JPH::Vec3 target = m_supportTarget;

    if (loadedCount == 1)
    {
        target = m_legs[loaded[0]].footCenter;
    }
    else if (m_shiftLeg >= 0)
    {
        target = m_legs[1 - m_shiftLeg].footCenter;
    }

    const float mass = ragdoll.GetTotalMass();
    const float omega = 2.0f * MathUtil::pi<float> * ComFrequency;

    const JPH::Vec3 velocity = m_frame.Horizontal(m_velocity);
    const float speed = velocity.Length();
    const float dampingScale = -2.0f * ComDamping * omega * mass;

    JPH::Vec3 force = m_frame.Horizontal(target - m_centerOfMass) * (omega * omega * mass) + velocity * dampingScale;

    // brakes anything above the stagger speed and never pushes, so a shove carries the body but can't be added to
    JPH::Vec3 braking = JPH::Vec3::sZero();
    const float excessSpeed = speed - m_staggerVelocity.Length();

    if (excessSpeed > 0.0f && speed > 1e-4f)
    {
        braking = velocity * (excessSpeed / speed * dampingScale);
    }

    const float strengthScale = MathUtil::Min(ragdoll.m_bodies[0].strength / GravityFullStrength, 1.0f);
    const float weight = mass * m_frame.gravity;

    float hold = 1.0f;

    // only holds the body while the capture point is over the feet; past that it has to step, so hard hits still fell it
    if (m_assist.releaseDistance > 0.0f)
    {
        hold = MathUtil::Clamp(1.0f - m_captureOutside / m_assist.releaseDistance, 0.0f, 1.0f);
    }

    hold *= 1.0f - MathUtil::Min(m_staggerVelocity.Length() / StaggerHoldSpeed, 1.0f);

    force = ClampLength(force, m_assist.maxForceFraction * weight * strengthScale * hold);

    // released, it only slows the body, so each recovery step takes some speed out
    force += ClampLength(braking, m_assist.recoveryForceFraction * weight * strengthScale) * (1.0f - hold);

    if (force.LengthSq() <= 0.0f)
    {
        return;
    }

    // a pair, pelvis against the loaded feet, so the ground still has to supply it through friction
    ragdoll.AddStepForce(bodyInterface, ragdoll.GetBodyID(0), force);

    for (uint32 i = 0; i < loadedCount; i++)
    {
        ragdoll.AddStepForce(bodyInterface, ragdoll.GetBodyID(m_legs[loaded[i]].foot), -force / float(loadedCount));
    }
}

} // namespace Hyperion
