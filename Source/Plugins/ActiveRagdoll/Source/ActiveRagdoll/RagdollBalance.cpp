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

#include <RagdollBalance.generated.inl>

namespace Hyperion {

using namespace RagdollMath;

namespace {

static constexpr float NearGround = 0.02f;
static constexpr float MinPendulumHeight = 0.3f;
static constexpr float SupportShrink = 0.5f;
static constexpr float Centering = 0.2f;

static constexpr float StepTrigger = 0.02f;
static constexpr float MinDoubleSupport = 0.05f;
static constexpr float SwingTime = 0.22f;
static constexpr float SwingFreezeFraction = 0.75f;
static constexpr float SwingTravelFraction = 0.85f;
static constexpr float SwingLift = 0.12f;
static constexpr float SwingOvertime = 0.15f;
static constexpr float SwingMinTouchdown = 0.6f;
static constexpr float TouchdownDistance = 0.05f;
static constexpr float LandingMargin = 0.08f;
static constexpr float LandingSideMargin = 0.03f;
static constexpr float MinStepWidth = 0.12f;
static constexpr float MaxStepWidth = 0.5f;
static constexpr float MaxStepLength = 0.45f;
static constexpr float StanceDistanceCost = 2.0f;
static constexpr float CaptureErrorCost = 10.0f;
static constexpr float CaptureMargin = 0.05f;
static constexpr float FreeFootPreference = 0.5f;
static constexpr float MinUsefulStep = 0.05f;
static constexpr float UselessStepCost = 100.0f;
static constexpr float RepeatStepCost = 0.5f;
static constexpr uint32 MaxRecoverySteps = 8;
static constexpr float MaxSwingWeight = 0.6f;
static constexpr float MaxLoadWait = 0.2f;
static constexpr float SwapLift = 0.03f;
static constexpr float MaxStaggerDrift = 0.15f;
static constexpr float RecoveredTime = 0.25f;
static constexpr float LandingDipPerSpeed = 0.1f;
static constexpr float MaxLandingDip = 0.03f;
static constexpr float LandingDipTime = 0.25f;

static constexpr float FallTilt = MathUtil::DegToRad(50.0f);
// a trunk lean past this steps even while the capture point is over the feet, at most once per LeanStepTime
static constexpr float LeanStepTilt = MathUtil::DegToRad(10.0f);
static constexpr float LeanStepTime = 0.15f;
static constexpr float FallHeightFraction = 0.6f;
static constexpr float CalmSpeed = 0.08f;
static constexpr float CalmTime = 0.25f;

static constexpr float SettleSpeed = 0.25f;
static constexpr float SettleTime = 0.3f;
static constexpr float SettleInside = 0.03f;
static constexpr float StanceWidthTolerance = 0.12f;
static constexpr float StanceSplitTolerance = 0.2f;
static constexpr float ShiftInside = 0.02f;
static constexpr float ShiftSpeed = 0.3f;
static constexpr float ShiftTimeout = 1.0f;

static constexpr float Pi = MathUtil::pi<float>;

} // namespace

bool RagdollBalance::Init(Ragdoll& ragdoll)
{
    m_isValid = false;
    m_rig = &ragdoll.GetRig();

    if (!m_rig->canBalance)
    {
        return false;
    }

    m_frame = ragdoll.GetGravityFrame();

    for (uint32 i = 0; i < RagdollRig::LimbCount; i++)
    {
        m_legs[i] = RagdollBalanceLeg {};
        static_cast<RagdollRigLeg&>(m_legs[i]) = m_rig->legs[i];
    }

    m_forward = m_rig->restForward;

    if (!InitRestPose(ragdoll))
    {
        return false;
    }

    m_isValid = true;
    Reset();

    return true;
}

bool RagdollBalance::InitRestPose(const Ragdoll& ragdoll)
{
    const Array<Ragdoll::BodyInfo, PhysicsAllocator>& bodies = ragdoll.m_bodies;
    const Ragdoll::BodyInfo& pelvis = bodies[0];

    m_restPelvis = pelvis.restOrientation;

    const JPH::Vec3 side = m_frame.up.Cross(m_rig->restForward);
    float restGround = std::numeric_limits<float>::max();

    for (const RagdollBalanceLeg& leg : m_legs)
    {
        const Ragdoll::BodyInfo& foot = bodies[leg.foot];

        for (const JPH::Vec3& corner : leg.soleCorners)
        {
            restGround = MathUtil::Min(restGround, (foot.restPosition + foot.restOrientation * corner).Dot(m_frame.up));
        }
    }

    const JPH::Vec3 restFootA = bodies[m_legs[0].foot].restPosition + bodies[m_legs[0].foot].restOrientation * m_legs[0].soleCenter;
    const JPH::Vec3 restFootB = bodies[m_legs[1].foot].restPosition + bodies[m_legs[1].foot].restOrientation * m_legs[1].soleCenter;

    m_restWidth = std::abs((restFootA - restFootB).Dot(side));

    const float totalMass = ragdoll.GetTotalMass();

    if (totalMass <= 0.0f)
    {
        return false;
    }

    JPH::Vec3 restCom = JPH::Vec3::sZero();

    for (const Ragdoll::BodyInfo& body : bodies)
    {
        restCom += (body.restPosition + body.restOrientation * body.localCenter) * body.mass;
    }

    m_restComHeight = MathUtil::Max((restCom / totalMass).Dot(m_frame.up) - restGround, MinPendulumHeight);

    return true;
}

void RagdollBalance::Reset()
{
    for (RagdollBalanceLeg& leg : m_legs)
    {
        leg.isStance = true;
        leg.hasContact = false;
        leg.isTouching = false;
        leg.contactAge = ContactHoldTime;
        leg.contactCornerCount = 0;
    }

    m_swing = Swing {};
    m_doubleSupportTime = 0.0f;
    m_loadWaitTime = 0.0f;
    m_capturedTime = 0.0f;
    m_landingDip = 0.0f;
    m_stumbleTime = 0.0f;
    m_calmTime = 0.0f;
    m_settleTime = 0.0f;
    m_shiftTime = 0.0f;
    m_shiftLeg = -1;
    m_lastSwingLeg = -1;
    m_isBodyOnGround = false;
    m_supportCount = 0;
    m_hasGround = false;
    m_status = RagdollBalanceStatus {};

    if (m_isValid)
    {
        m_status.state = RagdollBalanceState::Standing;
    }
}

void RagdollBalance::Clear(Ragdoll& ragdoll)
{
    ragdoll.ClearMotorOverrides();
    m_staggerVelocity = JPH::Vec3::sZero();
    m_stumbleTime = 0.0f;

    if (m_status.state != RagdollBalanceState::Fallen)
    {
        m_status.state = RagdollBalanceState::Off;
    }
}

void RagdollBalance::Step(Ragdoll& ragdoll, JPH::BodyInterface& bodyInterface, float deltaTime, JPH::Vec3Arg gravity, bool contactsFresh)
{
    ragdoll.ClearMotorOverrides();

    if (!m_isValid || m_status.state == RagdollBalanceState::Off || m_status.state == RagdollBalanceState::Fallen)
    {
        return;
    }

    for (const Ragdoll::BodyInfo& body : ragdoll.m_bodies)
    {
        if (body.mode != RagdollBodyMode::Powered)
        {
            return;
        }
    }

    GatherState(ragdoll, deltaTime, gravity, contactsFresh);
    BuildSupport();

    const float staggerDecay = MathUtil::Max(m_assist.staggerDecayTime, 1e-3f);
    m_staggerVelocity = m_frame.Horizontal(m_staggerVelocity) * std::exp(-deltaTime / staggerDecay);
    m_landingDip *= std::exp(-deltaTime / LandingDipTime);
    m_stumbleTime = MathUtil::Max(m_stumbleTime - deltaTime, 0.0f);

    if (!DetectFall())
    {
        if (m_status.state == RagdollBalanceState::Standing)
        {
            UpdateStanding(deltaTime);
        }
        else if (m_status.state == RagdollBalanceState::Stepping)
        {
            UpdateStepping(deltaTime);
        }
    }

    m_status.isCalm = m_status.state == RagdollBalanceState::Standing && m_calmTime > CalmTime && m_shiftLeg < 0;

    if (m_status.state == RagdollBalanceState::Fallen)
    {
        return;
    }

    DriveStanceLegs(ragdoll);

    if (m_status.state == RagdollBalanceState::Stepping)
    {
        DriveSwingLeg(ragdoll);
    }

    ApplyTorsoControl(ragdoll, bodyInterface, gravity);
    ApplyCenterOfMassControl(ragdoll, bodyInterface);
}

void RagdollBalance::GatherState(Ragdoll& ragdoll, float deltaTime, JPH::Vec3Arg gravity, bool contactsFresh)
{
    m_frame = GravityFrame::FromGravity(gravity);

    ragdoll.GetMassAverage(ragdoll.m_states.ToSpan(), m_centerOfMass, m_velocity);

    m_pelvisOrientation = ragdoll.m_states[0].orientation;

    // plan around the way the body actually faces and keep only the animated pelvis tilt: a stumble can turn the body
    // away from the held animation, and holding the animated heading spends the hips' torque fighting the turn
    const JPH::Quat& animated = ragdoll.m_targets[0].orientation;
    m_forward = GetHeadingForward(m_pelvisOrientation, m_restPelvis);
    m_pelvisTarget = (GetHeadingRotation(m_forward) * GetHeadingRotation(GetHeadingForward(animated, m_restPelvis)).Conjugated() * animated).Normalized();

    GatherFeet(ragdoll, deltaTime, contactsFresh);

    if (contactsFresh)
    {
        m_isBodyOnGround = IsAnyBodyOnGround(ragdoll);
    }

    const float height = MathUtil::Max(m_centerOfMass.Dot(m_frame.up) - m_groundHeight, MinPendulumHeight);
    m_omega = std::sqrt(m_frame.gravity / height);

    // a stagger adds the distance it still has to drift, so a shove the legs soaked up still gets stepped into; capped so
    // the steps aim at a point they can reach
    JPH::Vec3 staggerDrift = m_frame.Horizontal(m_staggerVelocity) * m_assist.staggerDecayTime;
    const float driftLength = staggerDrift.Length();

    if (driftLength > MaxStaggerDrift)
    {
        staggerDrift *= MaxStaggerDrift / driftLength;
    }

    m_capturePoint = ToGround(m_centerOfMass + m_frame.Horizontal(m_velocity) / m_omega + staggerDrift);

    const JPH::Quat chest = ragdoll.m_states[m_rig->chest].orientation;
    const JPH::Vec3 chestUp = (chest * ragdoll.m_bodies[m_rig->chest].restOrientation.Conjugated()) * m_frame.up;

    m_trunkTilt = std::acos(MathUtil::Clamp(chestUp.Dot(m_frame.up), -1.0f, 1.0f));

    m_leanDirection = m_forward;

    if (m_trunkTilt > LeanStepTilt)
    {
        m_leanDirection = m_frame.Horizontal(chestUp).NormalizedOr(m_forward);
    }
}

void RagdollBalance::GatherFeet(Ragdoll& ragdoll, float deltaTime, bool contactsFresh)
{
    float contactGround = std::numeric_limits<float>::max();

    for (RagdollBalanceLeg& leg : m_legs)
    {
        const Ragdoll::BodyState& foot = ragdoll.m_states[leg.foot];
        const JPH::Quat orientation = foot.orientation;

        leg.anklePosition = foot.origin;
        leg.footOrientation = orientation;
        leg.footCenter = leg.anklePosition + orientation * leg.soleCenter;

        leg.contactAge += deltaTime;

        if (contactsFresh)
        {
            leg.isTouching = ragdoll.IsBodyOnGround(leg.foot, m_frame.up, SupportNormalMinUp);

            if (leg.isTouching)
            {
                leg.contactAge = 0.0f;
            }
        }

        leg.soleHeight = std::numeric_limits<float>::max();

        for (uint32 corner = 0; corner < SoleCorners; corner++)
        {
            leg.contactCorners[corner] = leg.anklePosition + orientation * leg.soleCorners[corner];
            leg.soleHeight = MathUtil::Min(leg.soleHeight, leg.contactCorners[corner].Dot(m_frame.up));
        }

        if (leg.contactAge < ContactHoldTime)
        {
            contactGround = MathUtil::Min(contactGround, leg.soleHeight);
        }
    }

    if (contactGround < std::numeric_limits<float>::max())
    {
        m_groundHeight = contactGround;
        m_hasGround = true;
    }
    else if (!m_hasGround)
    {
        m_groundHeight = MathUtil::Min(m_legs[0].footCenter.Dot(m_frame.up), m_legs[1].footCenter.Dot(m_frame.up));
    }

    for (RagdollBalanceLeg& leg : m_legs)
    {
        leg.hasContact = leg.contactAge < ContactHoldTime || (m_hasGround && leg.soleHeight - m_groundHeight < NearGround);
        leg.contactCornerCount = 0;

        if (leg.hasContact)
        {
            leg.contactCornerCount = SoleCorners;
        }
    }
}

bool RagdollBalance::IsAnyBodyOnGround(Ragdoll& ragdoll) const
{
    for (uint32 i = 0; i < ragdoll.GetBodyCount(); i++)
    {
        if (ragdoll.m_bodies[i].part == RagdollBodyPart::Leg && i != m_legs[0].thigh && i != m_legs[1].thigh)
        {
            continue;
        }

        if (ragdoll.IsBodyOnGround(i, m_frame.up, SupportNormalMinUp))
        {
            return true;
        }
    }

    return false;
}

void RagdollBalance::BuildSupport()
{
    static constexpr uint32 MaxPoints = 2 * SoleCorners;

    PlanePoint points[MaxPoints];
    uint32 count = 0;

    JPH::Vec3 footCenterSum = JPH::Vec3::sZero();
    uint32 stanceCount = 0;

    for (const RagdollBalanceLeg& leg : m_legs)
    {
        if (!leg.isStance)
        {
            continue;
        }

        footCenterSum += leg.footCenter;
        stanceCount++;

        for (uint32 corner = 0; corner < leg.contactCornerCount; corner++)
        {
            points[count++] = m_frame.ToPlane(leg.contactCorners[corner]);
        }
    }

    PlanePoint hull[MaxPoints + 1];
    m_supportCount = BuildHull(points, count, hull);

    if (m_supportCount == 0)
    {
        m_supportCenter = ToGround(footCenterSum / float(MathUtil::Max(stanceCount, 1u)));
        m_supportTarget = m_supportCenter;
        m_captureOutside = 0.0f;
        m_status.captureError = JPH::Vec3::sZero();

        return;
    }

    const PlanePoint center = GetHullCentroid(hull, m_supportCount);

    m_supportCenter = m_frame.FromPlane(center, m_groundHeight);

    const PlanePoint closest = ClosestOnHull(hull, m_supportCount, m_frame.ToPlane(m_capturePoint), m_captureOutside);
    m_status.captureError = m_capturePoint - m_frame.FromPlane(closest, m_groundHeight);

    PlanePoint shrunk[MaxPoints + 1];

    for (uint32 i = 0; i < m_supportCount; i++)
    {
        shrunk[i] = center + (hull[i] - center) * SupportShrink;
    }

    float unusedDistance = 0.0f;
    const PlanePoint held = ClosestOnHull(shrunk, m_supportCount, m_frame.ToPlane(m_centerOfMass), unusedDistance);
    const PlanePoint target = held + (center - held) * Centering;

    m_supportTarget = m_frame.FromPlane(target, m_groundHeight);
}

bool RagdollBalance::DetectFall()
{
    RagdollFallReason reason = RagdollFallReason::None;

    if (m_trunkTilt > FallTilt)
    {
        reason = RagdollFallReason::Tilt;
    }
    else if (m_centerOfMass.Dot(m_frame.up) - m_groundHeight < m_restComHeight * FallHeightFraction)
    {
        reason = RagdollFallReason::Height;
    }
    else if (m_isBodyOnGround)
    {
        reason = RagdollFallReason::Contact;
    }

    if (reason == RagdollFallReason::None)
    {
        return false;
    }

    Fall(reason);

    return true;
}

void RagdollBalance::Fall(RagdollFallReason reason)
{
    m_status.state = RagdollBalanceState::Fallen;
    m_status.fallReason = reason;
}

void RagdollBalance::UpdateStanding(float deltaTime)
{
    m_doubleSupportTime += deltaTime;

    if (m_captureOutside < 0.0f && m_frame.Horizontal(m_velocity).Length() < CalmSpeed)
    {
        m_calmTime += deltaTime;
    }
    else
    {
        m_calmTime = 0.0f;
    }

    if (m_captureOutside < 0.0f)
    {
        m_capturedTime += deltaTime;
    }
    else
    {
        m_capturedTime = 0.0f;
    }

    // once a step has caught the body, shuffles after it start a fresh step budget
    if (m_capturedTime > RecoveredTime)
    {
        m_status.recoverySteps = 0;
    }

    if (m_calmTime > CalmTime)
    {
        m_status.isRecovering = false;
        m_status.recoverySteps = 0;
        m_lastSwingLeg = -1;
    }

    const bool captureOutside = m_captureOutside > StepTrigger && m_doubleSupportTime > MinDoubleSupport;
    const bool isLeaning = m_trunkTilt > LeanStepTilt && m_doubleSupportTime > LeanStepTime;

    if (m_stumbleTime <= 0.0f && m_supportCount > 0 && (captureOutside || isLeaning))
    {
        m_shiftLeg = -1;

        if (!BeginStep())
        {
            m_loadWaitTime += deltaTime;
        }

        return;
    }

    m_loadWaitTime = 0.0f;

    UpdateClosingStep(deltaTime);
}

void RagdollBalance::UpdateStepping(float deltaTime)
{
    RagdollBalanceLeg& leg = m_legs[m_swing.leg];

    m_swing.time += deltaTime;

    const uint32 stanceLeg = 1 - m_swing.leg;
    const RagdollBalanceLeg& stance = m_legs[stanceLeg];

    // the swing foot is still carrying the body and the stance foot is coming off the ground, so step with the free foot
    // rather than pivot over the planted one
    if (!m_swing.isLifted && !m_swing.isSwapped && leg.isTouching && !stance.isTouching && stance.soleHeight - m_groundHeight > SwapLift)
    {
        const uint32 plantedLeg = m_swing.leg;

        leg.isStance = true;
        StartSwing(stanceLeg, PlanLanding(stanceLeg, plantedLeg, SwingTime), false);
        m_swing.isSwapped = true;

        return;
    }

    const float phase = m_swing.time / SwingTime;

    if (!m_swing.isFixed && phase < SwingFreezeFraction)
    {
        m_swing.landing = PlanLanding(m_swing.leg, 1 - m_swing.leg, SwingTime - m_swing.time);
    }

    const JPH::Quat landingFoot = GetHeadingRotation(m_forward) * leg.restFoot;
    const JPH::Vec3 landingAnkle = m_swing.landing - landingFoot * leg.soleCenter;

    const float travel = SmoothStep(phase / SwingTravelFraction);
    const JPH::Vec3 start = m_frame.Horizontal(m_swing.startAnkle);
    const JPH::Vec3 end = m_frame.Horizontal(landingAnkle);

    float height = m_swing.startAnkle.Dot(m_frame.up) + (landingAnkle.Dot(m_frame.up) - m_swing.startAnkle.Dot(m_frame.up)) * travel;

    if (phase < 1.0f)
    {
        height += SwingLift * std::sin(Pi * phase);
    }
    else
    {
        height -= SwingPress;
    }

    m_swing.ankleTarget = start + (end - start) * travel + m_frame.up * height;

    if (!leg.isTouching)
    {
        m_swing.isLifted = true;
    }

    const bool isNearLanding = m_frame.Horizontal(leg.anklePosition - landingAnkle).Length() < TouchdownDistance;
    const bool hasTouchedDown = leg.isTouching && ((m_swing.isLifted && (phase > SwingMinTouchdown || isNearLanding)) || phase >= 1.0f);

    if (!hasTouchedDown && m_swing.time <= SwingTime + SwingOvertime)
    {
        return;
    }

    leg.isStance = true;

    m_status.state = RagdollBalanceState::Standing;
    m_status.steps++;
    m_doubleSupportTime = 0.0f;

    // the knees give with the landing, more the faster the body was going, instead of catching it on straight legs
    const float landingSpeed = m_frame.Horizontal(m_velocity).Length();
    m_landingDip = MathUtil::Max(m_landingDip, MathUtil::Min(landingSpeed * LandingDipPerSpeed, MaxLandingDip));

    if (!m_swing.isFixed)
    {
        m_status.recoverySteps++;
    }
}

void RagdollBalance::UpdateClosingStep(float deltaTime)
{
    if (m_captureOutside < -SettleInside && m_frame.Horizontal(m_velocity).Length() < SettleSpeed)
    {
        m_settleTime += deltaTime;
    }
    else
    {
        m_settleTime = 0.0f;
    }

    if (m_shiftLeg < 0)
    {
        if (m_settleTime > SettleTime && NeedsClosingStep())
        {
            const float distanceA = m_frame.Horizontal(m_centerOfMass - m_legs[0].footCenter).Length();
            const float distanceB = m_frame.Horizontal(m_centerOfMass - m_legs[1].footCenter).Length();

            m_shiftLeg = 1;

            if (distanceA > distanceB)
            {
                m_shiftLeg = 0;
            }

            m_shiftTime = 0.0f;
        }

        return;
    }

    m_shiftTime += deltaTime;

    const uint32 swingLeg = uint32(m_shiftLeg);
    const RagdollBalanceLeg& stance = m_legs[1 - swingLeg];

    if (GetCaptureDistance(stance) < -ShiftInside && m_frame.Horizontal(m_velocity).Length() < ShiftSpeed)
    {
        const JPH::Vec3 side = m_frame.up.Cross(m_forward) * m_legs[swingLeg].sideSign;
        StartSwing(swingLeg, ToGround(stance.footCenter + side * m_restWidth), true);

        return;
    }

    if (m_shiftTime > ShiftTimeout)
    {
        m_shiftLeg = -1;
        m_settleTime = 0.0f;
    }
}

bool RagdollBalance::NeedsClosingStep() const
{
    if (m_status.steps == 0 || !m_legs[0].hasContact || !m_legs[1].hasContact)
    {
        return false;
    }

    const JPH::Vec3 side = m_frame.up.Cross(m_forward);
    const JPH::Vec3 spread = m_frame.Horizontal(m_legs[0].footCenter - m_legs[1].footCenter);

    return std::abs(std::abs(spread.Dot(side)) - m_restWidth) > StanceWidthTolerance
        || std::abs(spread.Dot(m_forward)) > StanceSplitTolerance;
}

bool RagdollBalance::BeginStep()
{
    if (m_status.recoverySteps >= MaxRecoverySteps)
    {
        Fall(RagdollFallReason::StepBudget);

        return false;
    }

    int32 bestLeg = -1;
    float bestCost = std::numeric_limits<float>::max();
    JPH::Vec3 bestLanding = JPH::Vec3::sZero();

    for (uint32 swing = 0; swing < 2; swing++)
    {
        const uint32 stance = 1 - swing;

        if (!m_legs[stance].hasContact)
        {
            continue;
        }

        // a foot carrying most of the weight can't lift, so give the body a moment to move off it first
        if (GetWeightShare(swing) > MaxSwingWeight && m_loadWaitTime < MaxLoadWait)
        {
            continue;
        }

        const JPH::Vec3 landing = PlanLanding(swing, stance, SwingTime);
        const float travel = m_frame.Horizontal(landing - m_legs[swing].footCenter).Length();

        float cost = travel
            + MathUtil::Max(GetCaptureDistance(m_legs[stance]), 0.0f) * StanceDistanceCost
            + GetLandingCaptureError(stance, landing) * CaptureErrorCost;

        // recovery steps alternate, so a sideways stagger shuffles: the trailing foot closes in and the leading one can
        // step out again, instead of the leading foot stepping in place at the limit of its reach
        if (int32(swing) == m_lastSwingLeg)
        {
            cost += RepeatStepCost;
        }

        if (m_legs[swing].hasContact && travel < MinUsefulStep)
        {
            cost += UselessStepCost;
        }

        if (!m_legs[swing].hasContact)
        {
            cost *= FreeFootPreference;
        }

        if (cost < bestCost)
        {
            bestCost = cost;
            bestLeg = int32(swing);
            bestLanding = landing;
        }
    }

    if (bestLeg < 0)
    {
        return false;
    }

    StartSwing(uint32(bestLeg), bestLanding, false);
    m_status.isRecovering = true;
    m_loadWaitTime = 0.0f;

    return true;
}

void RagdollBalance::StartSwing(uint32 swingLeg, JPH::Vec3Arg landing, bool isFixed)
{
    RagdollBalanceLeg& leg = m_legs[swingLeg];
    leg.isStance = false;

    m_swing = Swing {};
    m_swing.leg = swingLeg;
    m_lastSwingLeg = int32(swingLeg);
    m_swing.startAnkle = leg.anklePosition;
    m_swing.landing = landing;
    m_swing.ankleTarget = leg.anklePosition;
    m_swing.isFixed = isFixed;

    m_status.state = RagdollBalanceState::Stepping;
    m_calmTime = 0.0f;
    m_settleTime = 0.0f;
    m_shiftLeg = -1;
}

float RagdollBalance::GetWeightShare(uint32 legIndex) const
{
    const RagdollBalanceLeg& leg = m_legs[legIndex];
    const RagdollBalanceLeg& other = m_legs[1 - legIndex];

    const bool isOff = !leg.isTouching && leg.soleHeight - m_groundHeight > NearGround;
    const bool isOtherOff = !other.isTouching && other.soleHeight - m_groundHeight > NearGround;

    if (isOff)
    {
        return 0.0f;
    }

    if (isOtherOff)
    {
        return 1.0f;
    }

    // where the centre of mass sits along the line between the feet: 1 is right over this foot
    const JPH::Vec3 own = m_frame.Horizontal(leg.footCenter);
    const JPH::Vec3 span = own - m_frame.Horizontal(other.footCenter);
    const float lengthSq = span.LengthSq();

    if (lengthSq < 1e-6f)
    {
        return 0.5f;
    }

    const float fromOther = (m_frame.Horizontal(m_centerOfMass) - m_frame.Horizontal(other.footCenter)).Dot(span);

    return MathUtil::Clamp(fromOther / lengthSq, 0.0f, 1.0f);
}

float RagdollBalance::GetCaptureDistance(const RagdollBalanceLeg& leg) const
{
    PlanePoint points[SoleCorners];
    PlanePoint hull[SoleCorners + 1];

    for (uint32 corner = 0; corner < SoleCorners; corner++)
    {
        points[corner] = m_frame.ToPlane(leg.contactCorners[corner]);
    }

    const uint32 count = BuildHull(points, SoleCorners, hull);

    float distance = 0.0f;
    ClosestOnHull(hull, count, m_frame.ToPlane(m_capturePoint), distance);

    return distance;
}

float RagdollBalance::GetLandingCaptureError(uint32 stanceLeg, JPH::Vec3Arg landing) const
{
    const RagdollBalanceLeg& stance = m_legs[stanceLeg];
    const JPH::Vec3 side = m_frame.up.Cross(m_forward);

    PlanePoint points[SoleCorners * 2];
    PlanePoint hull[SoleCorners * 2 + 1];
    uint32 count = 0;

    for (uint32 corner = 0; corner < SoleCorners; corner++)
    {
        points[count++] = m_frame.ToPlane(stance.contactCorners[corner]);
    }

    for (uint32 corner = 0; corner < SoleCorners; corner++)
    {
        const JPH::Vec3 point = landing + side * (SoleCornerSigns[corner][0] * m_rig->footHalfWidth) + m_forward * (SoleCornerSigns[corner][1] * m_rig->footHalfLength);
        points[count++] = m_frame.ToPlane(point);
    }

    count = BuildHull(points, count, hull);

    const float growth = std::exp(m_omega * SwingTime);
    const JPH::Vec3 pressure = stance.footCenter;
    const JPH::Vec3 predicted = pressure + m_frame.Horizontal(m_capturePoint - pressure) * growth;

    float distance = 0.0f;
    ClosestOnHull(hull, count, m_frame.ToPlane(predicted), distance);

    return MathUtil::Max(distance + CaptureMargin, 0.0f);
}

JPH::Vec3 RagdollBalance::PlanLanding(uint32 swingLeg, uint32 stanceLeg, float timeLeft) const
{
    const RagdollBalanceLeg& stance = m_legs[stanceLeg];
    const RagdollBalanceLeg& swing = m_legs[swingLeg];

    PlanePoint stancePoints[SoleCorners];
    PlanePoint stanceHull[SoleCorners + 1];
    uint32 stanceCount = 0;

    for (uint32 corner = 0; corner < stance.contactCornerCount; corner++)
    {
        stancePoints[stanceCount++] = m_frame.ToPlane(stance.contactCorners[corner]);
    }

    if (stanceCount == 0)
    {
        stancePoints[stanceCount++] = m_frame.ToPlane(stance.footCenter);
    }

    stanceCount = BuildHull(stancePoints, stanceCount, stanceHull);

    const PlanePoint capture = m_frame.ToPlane(m_capturePoint);

    float unusedDistance = 0.0f;
    const PlanePoint pressure = ClosestOnHull(stanceHull, stanceCount, capture, unusedDistance);

    const float growth = std::exp(m_omega * MathUtil::Max(timeLeft, 0.0f));
    const PlanePoint predicted = pressure + (capture - pressure) * growth;

    JPH::Vec3 direction = m_frame.axisU * (predicted.x - pressure.x) + m_frame.axisV * (predicted.y - pressure.y);
    direction = direction.NormalizedOr(m_frame.Horizontal(m_velocity).NormalizedOr(m_leanDirection));

    const JPH::Vec3 side = m_frame.up.Cross(m_forward) * swing.sideSign;
    JPH::Vec3 landing = m_frame.axisU * predicted.x + m_frame.axisV * predicted.y + direction * LandingMargin + side * LandingSideMargin;

    const JPH::Vec3 stanceCenter = m_frame.Horizontal(stance.footCenter);
    const float lateral = (landing - stanceCenter).Dot(side);

    if (lateral < MinStepWidth)
    {
        landing += side * (MinStepWidth - lateral);
    }
    else if (lateral > MaxStepWidth)
    {
        landing -= side * (lateral - MaxStepWidth);
    }

    JPH::Vec3 step = landing - stanceCenter;
    const float stepLength = step.Length();

    if (stepLength > MaxStepLength)
    {
        step *= MaxStepLength / stepLength;
    }

    return ToGround(stanceCenter + step);
}

JPH::Vec3 RagdollBalance::GetHeadingForward(JPH::QuatArg orientation, JPH::QuatArg rest) const
{
    return m_frame.Horizontal((orientation * rest.Conjugated()) * m_rig->restForward).NormalizedOr(m_rig->restForward);
}

JPH::Quat RagdollBalance::GetHeadingRotation(JPH::Vec3Arg forward) const
{
    const JPH::Vec3 heading = m_frame.Horizontal(forward).NormalizedOr(m_rig->restForward);
    const float angle = std::atan2(m_rig->restForward.Cross(heading).Dot(m_frame.up), m_rig->restForward.Dot(heading));

    return JPH::Quat::sRotation(m_frame.up, angle);
}

JPH::Vec3 RagdollBalance::ToGround(JPH::Vec3Arg point) const
{
    return m_frame.Horizontal(point) + m_frame.up * m_groundHeight;
}

} // namespace Hyperion
