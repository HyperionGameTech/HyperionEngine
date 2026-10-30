/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <ActiveRagdoll/RagdollReactions.hpp>
#include <ActiveRagdoll/Ragdoll.hpp>

namespace Hyperion {

using namespace RagdollMath;

namespace {

static constexpr float Pi = MathUtil::pi<float>;
static constexpr float TwoPi = 2.0f * Pi;

static constexpr float FlinchAttack = 0.002f;
static constexpr float LookAttack = 0.1f;
static constexpr float MinLookDistance = 0.2f;
static constexpr float GrabRamp = 0.3f;
static constexpr float BehindMargin = -0.1f;
static constexpr float BraceHandLift = 0.06f;
static constexpr float BraceElbowBend = 0.1f;
static constexpr float HeadHandSide = 0.11f;
static constexpr float HeadHandForward = 0.05f;
static constexpr float HeadHandDrop = 0.05f;
static constexpr float FallTimeout = 4.0f;
static constexpr float GroundedStopTime = 0.1f;

float Envelope(float time, float attack, float total)
{
    if (time < 0.0f || time >= total)
    {
        return 0.0f;
    }

    if (time < attack)
    {
        return time / attack;
    }

    return 1.0f - SmoothStep((time - attack) / MathUtil::Max(total - attack, 1e-4f));
}

bool IsInFront(const RagdollBehaviourContext& ctx, JPH::Vec3Arg point)
{
    const JPH::Vec3 chest = ctx.bodies[ctx.rig->chest].center;

    return ctx.Horizontal(point - chest).Dot(ctx.Horizontal(ctx.forward)) > BehindMargin;
}

} // namespace

#pragma region Fallen

RagdollFallenBehaviour::RagdollFallenBehaviour()
    : TRagdollBehaviour(NAME("Fallen"))
{
}

void RagdollFallenBehaviour::Init(const RagdollRig& rig, uint32 bodyCount)
{
    m_follow.Resize(bodyCount);

    for (JPH::Quat& follow : m_follow)
    {
        follow = JPH::Quat::sIdentity();
    }
}

void RagdollFallenBehaviour::CaptureFollowPose(const RagdollBehaviourContext& ctx, float blend)
{
    for (uint32 i = 1; i < ctx.GetBodyCount(); i++)
    {
        const int32 parent = ctx.ragdoll->GetParentBody(i);

        if (parent < 0)
        {
            continue;
        }

        const JPH::Quat actual = RelativeRotation(ctx.bodies[parent].orientation, ctx.bodies[i].orientation);

        if (blend >= 1.0f)
        {
            m_follow[i] = actual;
        }
        else
        {
            m_follow[i] = m_follow[i].SLERP(actual, blend).Normalized();
        }
    }
}

void RagdollFallenBehaviour::OnStart(const RagdollBehaviourContext& ctx)
{
    CaptureFollowPose(ctx, 1.0f);
}

void RagdollFallenBehaviour::Update(const RagdollBehaviourContext& ctx)
{
    if (!ctx.isFreePose)
    {
        if (IsActive())
        {
            Stop();
        }

        return;
    }

    if (!IsActive())
    {
        Start(ctx);

        return;
    }

    float blend = 1.0f;

    if (settings.followTime > 0.0f)
    {
        blend = 1.0f - std::exp(-ctx.deltaTime / settings.followTime);
    }

    CaptureFollowPose(ctx, blend);
}

void RagdollFallenBehaviour::Contribute(const RagdollBehaviourContext& ctx, RagdollBehaviourOutput& out) const
{
    out.isAdditive = false;

    for (uint32 i = 1; i < ctx.GetBodyCount(); i++)
    {
        out.pose[i] = m_follow[i];
        out.mask[i] = 1.0f;
    }

    out.ScaleAllStrengths(settings.strength);

    if (ctx.isAirborne)
    {
        out.gravityScale = 0.0f;
    }
    else
    {
        out.gravityScale = settings.gravityComp;
    }
}

#pragma endregion Fallen

#pragma region Stagger

RagdollStaggerBehaviour::RagdollStaggerBehaviour()
    : TRagdollBehaviour(NAME("Stagger"))
{
}

void RagdollStaggerBehaviour::OnEvent(const RagdollBehaviourContext& ctx, const RagdollEvent& event)
{
    const bool isShove = event.type == RagdollEventType::Hit || event.type == RagdollEventType::Push;

    if (!isShove || ctx.isFreePose || !ctx.isAlive || event.magnitude < settings.minImpulse)
    {
        return;
    }

    Start(ctx);
}

void RagdollStaggerBehaviour::Update(const RagdollBehaviourContext& ctx)
{
    if (!IsActive())
    {
        return;
    }

    if (ctx.isFreePose)
    {
        Stop();

        return;
    }

    if (ctx.isBalancing && ctx.balance->state == RagdollBalanceState::Fallen)
    {
        Stop();

        return;
    }

    if (m_time < settings.minTime)
    {
        return;
    }

    if (!ctx.isBalancing || ctx.balance->isCalm)
    {
        Stop();
    }
}

void RagdollStaggerBehaviour::Contribute(const RagdollBehaviourContext& ctx, RagdollBehaviourOutput& out) const
{
    out.balance = RagdollBehaviourBalance::On;

    if (settings.spreadSpeed <= 0.0f)
    {
        return;
    }

    const float speed = ctx.Horizontal(ctx.velocity).Length();
    const float spread = MathUtil::DegToRad(settings.armSpread) * MathUtil::Clamp(speed / settings.spreadSpeed, 0.0f, 1.0f);

    for (const RagdollRigArm& arm : ctx.rig->arms)
    {
        out.AddWorldRotation(ctx, arm.upper, ctx.forward, arm.sideSign * spread);
    }
}

#pragma endregion Stagger

#pragma region ArmBalance

RagdollArmBalanceBehaviour::RagdollArmBalanceBehaviour()
    : TRagdollBehaviour(NAME("ArmBalance"))
{
}

void RagdollArmBalanceBehaviour::Update(const RagdollBehaviourContext& ctx)
{
    const RagdollBalanceState state = ctx.balance->state;
    const bool isRecovering = ctx.isAlive && ctx.isBalancing && (state == RagdollBalanceState::Standing || state == RagdollBalanceState::Stepping);

    if (isRecovering && !IsActive())
    {
        Start(ctx);
    }
    else if (!isRecovering && IsActive())
    {
        Stop();
    }

    float targetFore = 0.0f;
    float targetSide = 0.0f;

    if (isRecovering)
    {
        const JPH::Vec3 error = ctx.Horizontal(ctx.balance->captureError);
        const float length = error.Length();
        JPH::Vec3 outside = JPH::Vec3::sZero();

        if (length > settings.deadZone)
        {
            outside = error * ((length - settings.deadZone) / length);
        }

        targetFore = MathUtil::Clamp(outside.Dot(ctx.forward) / settings.errorScale, -1.0f, 1.0f);
        targetSide = MathUtil::Clamp(outside.Dot(ctx.right) / settings.errorScale, -1.0f, 1.0f);
    }

    float blend = 1.0f;

    if (settings.smoothTime > 0.0f)
    {
        blend = 1.0f - std::exp(-ctx.deltaTime / settings.smoothTime);
    }

    m_fore += (targetFore - m_fore) * blend;
    m_side += (targetSide - m_side) * blend;
}

void RagdollArmBalanceBehaviour::Contribute(const RagdollBehaviourContext& ctx, RagdollBehaviourOutput& out) const
{
    const RagdollRig& rig = *ctx.rig;
    const float weight = MathUtil::Clamp(MathUtil::Max(std::abs(m_fore), std::abs(m_side)), 0.0f, 1.0f) * settings.strength;

    for (uint32 i = 0; i < RagdollRig::LimbCount; i++)
    {
        const RagdollRigArm& arm = rig.arms[i];
        const float side = arm.sideSign;

        // a lean reaches out on the same side, and the far arm lifts
        const float outward = settings.sideReach * side * MathUtil::Max(side * m_side, 0.0f);
        const float raised = settings.raiseReach * MathUtil::Max(-side * m_side, 0.0f);
        const float forward = settings.foreReach * m_fore;

        const JPH::Vec3 shoulder = ctx.bodies[arm.upper].origin;
        const JPH::Vec3 target = shoulder + ctx.forward * forward + ctx.right * outward + ctx.up * raised;

        out.hands[i] = RagdollBehaviourGoal { target, weight };
    }
}

#pragma endregion ArmBalance

#pragma region Shot

RagdollShotBehaviour::RagdollShotBehaviour()
    : TRagdollBehaviour(NAME("Shot"))
{
}

void RagdollShotBehaviour::Init(const RagdollRig& rig, uint32 bodyCount)
{
    m_loosen.Resize(bodyCount);

    for (float& loosen : m_loosen)
    {
        loosen = 1.0f;
    }

    m_wound = Wound {};
    m_flinchTime = -1.0f;
    m_flinchPitch = 0.0f;
    m_lookBody = -1;
    m_lookTime = 0.0f;
}

void RagdollShotBehaviour::OnEvent(const RagdollBehaviourContext& ctx, const RagdollEvent& event)
{
    if (event.type != RagdollEventType::Hit || event.body < 0 || uint32(event.body) >= ctx.GetBodyCount())
    {
        return;
    }

    StartIfIdle(ctx);

    if (!IsActive())
    {
        return;
    }

    const uint32 body = uint32(event.body);
    const JPH::Vec3 chest = ctx.bodies[ctx.rig->chest].center;

    LoosenAround(ctx, body);

    m_flinchTime = 0.0f;
    m_flinchSide = 1.0f;

    if ((event.point - chest).Dot(ctx.right) < 0.0f)
    {
        m_flinchSide = -1.0f;
    }

    // the shot pushes the body away from the shooter: from the front it arches back, from behind it bows forward
    const JPH::Vec3 push = ctx.Horizontal(event.direction).NormalizedOr(JPH::Vec3::sZero());
    m_flinchPitch = push.Dot(ctx.Horizontal(ctx.forward));

    if (ctx.ragdoll->GetBodyPart(body) != RagdollBodyPart::Head)
    {
        m_lookBody = int32(body);
        m_lookLocal = ctx.ToBodyLocal(body, event.point);
        m_lookTime = 0.0f;
    }

    StartGrab(ctx, event);
}

void RagdollShotBehaviour::LoosenAround(const RagdollBehaviourContext& ctx, uint32 body)
{
    const Ragdoll& ragdoll = *ctx.ragdoll;
    const uint32 bodyCount = ctx.GetBodyCount();
    const float loosen = MathUtil::Clamp(settings.loosen, 0.0f, 1.0f);

    if (ragdoll.IsAnchor(body))
    {
        m_loosen[body] = MathUtil::Min(m_loosen[body], loosen);

        return;
    }

    const auto childCount = [&](int32 parent)
    {
        uint32 count = 0;

        for (uint32 i = 0; i < bodyCount; i++)
        {
            if (ragdoll.GetParentBody(i) == parent)
            {
                count++;
            }
        }

        return count;
    };

    int32 top = int32(body);

    for (int32 parent = ragdoll.GetParentBody(uint32(top)); parent >= 0 && !ragdoll.IsAnchor(uint32(parent)) && childCount(parent) == 1; parent = ragdoll.GetParentBody(uint32(top)))
    {
        top = parent;
    }

    for (uint32 i = 0; i < bodyCount; i++)
    {
        for (int32 walk = int32(i); walk >= 0; walk = ragdoll.GetParentBody(uint32(walk)))
        {
            if (walk == top)
            {
                m_loosen[i] = MathUtil::Min(m_loosen[i], loosen);
                break;
            }
        }
    }
}

void RagdollShotBehaviour::StartGrab(const RagdollBehaviourContext& ctx, const RagdollEvent& event)
{
    const uint32 body = uint32(event.body);

    if (event.damage <= 0.0f || ctx.ragdoll->GetBodyPart(body) == RagdollBodyPart::Head || settings.grabTime <= 0.0f)
    {
        return;
    }

    const int32 hitArm = ctx.FindArm(body);

    int32 bestArm = -1;
    float bestDistance = 0.0f;

    for (uint32 i = 0; i < RagdollRig::LimbCount; i++)
    {
        if (int32(i) == hitArm)
        {
            continue;
        }

        const RagdollRigArm& arm = ctx.rig->arms[i];
        const float distance = (event.point - ctx.bodies[arm.upper].origin).Length();
        const float reach = settings.grabReach * (arm.upperLength + arm.lowerLength);

        if (distance > reach)
        {
            continue;
        }

        if (bestArm < 0 || distance < bestDistance)
        {
            bestArm = int32(i);
            bestDistance = distance;
        }
    }

    if (bestArm < 0)
    {
        return;
    }

    const JPH::Vec3 outward = (event.point - ctx.bodies[body].center).NormalizedOr(-event.direction);

    m_wound.body = body;
    m_wound.localPoint = ctx.ToBodyLocal(body, event.point);
    m_wound.localNormal = ctx.ToLocalDirection(body, outward);
    m_wound.arm = bestArm;
    m_wound.time = 0.0f;
}

void RagdollShotBehaviour::Update(const RagdollBehaviourContext& ctx)
{
    if (!IsActive())
    {
        return;
    }

    const float dt = ctx.deltaTime;

    float recover = 1.0f;

    if (settings.recoverTime > 0.0f)
    {
        recover = dt / settings.recoverTime;
    }

    bool isLoose = false;

    for (float& loosen : m_loosen)
    {
        loosen = MathUtil::Min(loosen + recover, 1.0f);
        isLoose = isLoose || loosen < 1.0f;
    }

    if (m_flinchTime >= 0.0f)
    {
        m_flinchTime += dt;

        if (m_flinchTime >= settings.flinchTime)
        {
            m_flinchTime = -1.0f;
        }
    }

    if (m_wound.arm >= 0)
    {
        m_wound.time += dt;

        if (m_wound.time >= settings.grabDelay + settings.grabTime)
        {
            m_wound.arm = -1;
        }
    }

    if (m_lookBody >= 0)
    {
        m_lookTime += dt;
    }

    const bool isLooking = m_lookBody >= 0 && m_lookTime < settings.lookTime;

    if (!isLoose && m_flinchTime < 0.0f && m_wound.arm < 0 && !isLooking)
    {
        Stop();
    }
}

void RagdollShotBehaviour::Contribute(const RagdollBehaviourContext& ctx, RagdollBehaviourOutput& out) const
{
    const RagdollRig& rig = *ctx.rig;

    for (size_t i = 0; i < m_loosen.Size(); i++)
    {
        out.strength[i] = m_loosen[i];
    }

    const float flinch = MathUtil::DegToRad(settings.flinch) * Envelope(m_flinchTime, FlinchAttack, settings.flinchTime);

    if (flinch > 0.0f)
    {
        // positive about Right pitches the body forward, so this is negative for a shot from the front
        const float pitch = flinch * m_flinchPitch;

        for (uint32 i = 0; i < rig.spineCount; i++)
        {
            out.AddWorldRotation(ctx, rig.spine[i], ctx.right, pitch * 0.5f);
            out.AddWorldRotation(ctx, rig.spine[i], ctx.forward, -m_flinchSide * flinch * 0.4f);
        }

        out.AddWorldRotation(ctx, rig.neck, ctx.right, pitch * 0.6f);
    }

    LookAtWound(ctx, out);

    if (m_wound.arm < 0 || m_wound.time < settings.grabDelay)
    {
        return;
    }

    const float grabTime = m_wound.time - settings.grabDelay;
    const float rampIn = MathUtil::Clamp(grabTime / GrabRamp, 0.0f, 1.0f);
    const float rampOut = 1.0f - MathUtil::Clamp((grabTime - (settings.grabTime - GrabRamp)) / GrabRamp, 0.0f, 1.0f);

    const JPH::Vec3 wound = ctx.FromBodyLocal(m_wound.body, m_wound.localPoint);
    const JPH::Vec3 normal = ctx.ToBodyDirection(m_wound.body, m_wound.localNormal);

    out.hands[m_wound.arm] = RagdollBehaviourGoal { wound + normal * settings.handOffset, SmoothStep(rampIn) * rampOut };
}

void RagdollShotBehaviour::LookAtWound(const RagdollBehaviourContext& ctx, RagdollBehaviourOutput& out) const
{
    if (m_lookBody < 0)
    {
        return;
    }

    const float weight = Envelope(m_lookTime, LookAttack, settings.lookTime);
    const RagdollRig& rig = *ctx.rig;
    const JPH::Vec3 head = ctx.bodies[rig.neck].center;
    const JPH::Vec3 toWound = ctx.FromBodyLocal(uint32(m_lookBody), m_lookLocal) - head;

    if (weight <= 0.0f || toWound.Length() < MinLookDistance)
    {
        return;
    }

    const JPH::Vec3 flat = ctx.Horizontal(toWound);
    const float yaw = std::atan2(flat.Dot(ctx.right), flat.Dot(ctx.forward));
    const float pitch = std::atan2(toWound.Dot(ctx.up), flat.Length());

    const float maxYaw = MathUtil::DegToRad(settings.lookYaw);
    const float maxPitch = MathUtil::DegToRad(settings.lookPitch);

    // positive yaw about Up turns the forward vector towards Right, and positive about Right pitches it down
    out.AddWorldRotation(ctx, rig.neck, ctx.up, MathUtil::Clamp(yaw, -maxYaw, maxYaw) * weight);
    out.AddWorldRotation(ctx, rig.neck, ctx.right, -MathUtil::Clamp(pitch, -maxPitch, maxPitch) * weight);
}

#pragma endregion Shot

#pragma region Brace

RagdollBraceBehaviour::RagdollBraceBehaviour()
    : TRagdollBehaviour(NAME("Brace"))
{
}

void RagdollBraceBehaviour::OnEvent(const RagdollBehaviourContext& ctx, const RagdollEvent& event)
{
    if (event.type == RagdollEventType::LostSupport && ctx.isAlive)
    {
        StartIfIdle(ctx);

        return;
    }

    if ((event.type == RagdollEventType::Impact || event.type == RagdollEventType::OnGround) && IsActive())
    {
        m_landing.hasLanded = true;
    }
}

void RagdollBraceBehaviour::OnStart(const RagdollBehaviourContext& ctx)
{
    m_landing.Reset();
    m_hasTarget = false;
}

void RagdollBraceBehaviour::Update(const RagdollBehaviourContext& ctx)
{
    if (!IsActive())
    {
        return;
    }

    if (!ctx.isAlive || m_time > FallTimeout)
    {
        Stop();

        return;
    }

    for (const RagdollRigArm& arm : ctx.rig->arms)
    {
        m_landing.hasLanded = m_landing.hasLanded || ctx.bodies[arm.hand].isOnGround;
    }

    if (m_landing.hasLanded)
    {
        if (m_landing.Advance(ctx.deltaTime, settings.hold))
        {
            Stop();
        }

        return;
    }

    if (!ctx.isImpactPredicted || ctx.impactTime > settings.horizon || ctx.velocity.Length() < settings.minSpeed)
    {
        return;
    }

    if (!IsInFront(ctx, ctx.impactPoint))
    {
        m_hasTarget = false;

        return;
    }

    m_hasTarget = true;
    m_point = ctx.impactPoint;
    m_normal = ctx.impactNormal;
}

void RagdollBraceBehaviour::Contribute(const RagdollBehaviourContext& ctx, RagdollBehaviourOutput& out) const
{
    if (!m_hasTarget)
    {
        return;
    }

    const JPH::Vec3 side = (ctx.right - m_normal * ctx.right.Dot(m_normal)).NormalizedOr(ctx.right);

    for (uint32 i = 0; i < RagdollRig::LimbCount; i++)
    {
        const RagdollRigArm& arm = ctx.rig->arms[i];
        const JPH::Vec3 shoulder = ctx.bodies[arm.upper].origin;

        JPH::Vec3 target = m_point + side * (arm.sideSign * settings.spread) + m_normal * BraceHandLift;
        target = target + (shoulder - target) * BraceElbowBend;

        out.hands[i] = RagdollBehaviourGoal { target, 1.0f };
    }

    out.ScaleArmStrengths(*ctx.rig, settings.strength);
}

#pragma endregion Brace

#pragma region ProtectHead

RagdollProtectHeadBehaviour::RagdollProtectHeadBehaviour()
    : TRagdollBehaviour(NAME("ProtectHead"))
{
}

void RagdollProtectHeadBehaviour::OnEvent(const RagdollBehaviourContext& ctx, const RagdollEvent& event)
{
    if (event.type == RagdollEventType::Impact || event.type == RagdollEventType::OnGround)
    {
        if (IsActive())
        {
            m_landing.hasLanded = true;
        }

        return;
    }

    if (!ctx.isAlive)
    {
        return;
    }

    float duration = -1.0f;

    if (event.type == RagdollEventType::LostSupport)
    {
        if (!IsFallingBackward(ctx))
        {
            return;
        }
    }
    else if (event.type == RagdollEventType::Hit && event.body >= 0 && ctx.ragdoll->GetBodyPart(uint32(event.body)) == RagdollBodyPart::Head)
    {
        duration = settings.hitTime;
    }
    else
    {
        return;
    }

    Start(ctx);
    m_duration = duration;
}

void RagdollProtectHeadBehaviour::OnStart(const RagdollBehaviourContext& ctx)
{
    m_duration = -1.0f;
    m_landing.Reset();
}

bool RagdollProtectHeadBehaviour::IsFallingBackward(const RagdollBehaviourContext& ctx) const
{
    const bool isBackward = ctx.velocity.Dot(ctx.Horizontal(ctx.forward)) < -settings.minBackward;
    const bool landsBehind = ctx.isImpactPredicted && !IsInFront(ctx, ctx.impactPoint);

    return isBackward || landsBehind;
}

void RagdollProtectHeadBehaviour::Update(const RagdollBehaviourContext& ctx)
{
    if (!IsActive())
    {
        const bool isFalling = ctx.isAlive && ctx.isFreePose && ctx.isImpactPredicted && ctx.impactTime <= settings.horizon;

        if (isFalling && IsFallingBackward(ctx))
        {
            Start(ctx);
        }

        return;
    }

    if (!ctx.isAlive || m_time > FallTimeout || (m_duration > 0.0f && m_time >= m_duration))
    {
        Stop();

        return;
    }

    if (m_landing.hasLanded && m_landing.Advance(ctx.deltaTime, settings.hold))
    {
        Stop();
    }
}

void RagdollProtectHeadBehaviour::Contribute(const RagdollBehaviourContext& ctx, RagdollBehaviourOutput& out) const
{
    const RagdollRig& rig = *ctx.rig;
    const JPH::Vec3 head = ctx.bodies[rig.neck].center;
    const JPH::Vec3 chestUp = ctx.GetChestUp();

    for (uint32 i = 0; i < RagdollRig::LimbCount; i++)
    {
        const RagdollRigArm& arm = rig.arms[i];
        const JPH::Vec3 target = head + ctx.right * (arm.sideSign * HeadHandSide) + ctx.forward * HeadHandForward - chestUp * HeadHandDrop;

        out.hands[i] = RagdollBehaviourGoal { target, 1.0f };
    }

    const float tuck = MathUtil::DegToRad(settings.tuck);

    out.AddWorldRotation(ctx, rig.neck, ctx.right, tuck);

    for (uint32 i = 0; i < rig.spineCount; i++)
    {
        out.AddWorldRotation(ctx, rig.spine[i], ctx.right, tuck * 0.3f);
    }

    out.ScaleArmStrengths(rig, settings.strength);
}

#pragma endregion ProtectHead

#pragma region HighFall

RagdollHighFallBehaviour::RagdollHighFallBehaviour()
    : TRagdollBehaviour(NAME("HighFall"))
{
}

bool RagdollHighFallBehaviour::IsHighFall(const RagdollBehaviourContext& ctx) const
{
    if (!ctx.isAirborne)
    {
        return false;
    }

    return ctx.dropHeight > settings.minHeight || -ctx.velocity.Dot(ctx.up) > settings.minSpeed;
}

void RagdollHighFallBehaviour::OnEvent(const RagdollBehaviourContext& ctx, const RagdollEvent& event)
{
    if (event.type == RagdollEventType::LostSupport && ctx.isFreePose && IsHighFall(ctx))
    {
        StartIfIdle(ctx);

        return;
    }

    if ((event.type == RagdollEventType::Impact || event.type == RagdollEventType::OnGround) && IsActive())
    {
        Stop();
    }
}

void RagdollHighFallBehaviour::OnStart(const RagdollBehaviourContext& ctx)
{
    m_groundedTime = 0.0f;
}

void RagdollHighFallBehaviour::Update(const RagdollBehaviourContext& ctx)
{
    if (!IsActive())
    {
        if (ctx.isFreePose && ctx.airTime >= settings.minAirTime && IsHighFall(ctx))
        {
            Start(ctx);
        }

        return;
    }

    if (!ctx.isFreePose)
    {
        Stop();

        return;
    }

    if (ctx.isAirborne)
    {
        m_groundedTime = 0.0f;

        return;
    }

    m_groundedTime += ctx.deltaTime;

    if (m_groundedTime >= GroundedStopTime)
    {
        Stop();
    }
}

void RagdollHighFallBehaviour::Contribute(const RagdollBehaviourContext& ctx, RagdollBehaviourOutput& out) const
{
    const RagdollRig& rig = *ctx.rig;
    const float phase = TwoPi * settings.frequency * m_time;
    const float armSwing = MathUtil::DegToRad(settings.armSwing);
    const float legSwing = MathUtil::DegToRad(settings.legSwing);

    for (const RagdollRigArm& arm : rig.arms)
    {
        float offset = 0.0f;

        if (arm.sideSign <= 0.0f)
        {
            offset = Pi;
        }

        out.AddWorldRotation(ctx, arm.upper, ctx.right, armSwing * std::sin(phase + offset));
        out.AddWorldRotation(ctx, arm.upper, ctx.forward, arm.sideSign * MathUtil::DegToRad(30.0f));
        out.AddWorldRotation(ctx, arm.lower, ctx.right, -MathUtil::DegToRad(20.0f + 15.0f * std::sin(phase * 1.3f + offset)));
    }

    for (const RagdollRigLeg& leg : rig.legs)
    {
        float offset = Pi * 0.5f;

        if (leg.sideSign <= 0.0f)
        {
            offset += Pi;
        }

        const float legPhase = phase * 0.8f + offset;

        out.AddWorldRotation(ctx, leg.thigh, ctx.right, legSwing * std::sin(legPhase));
        out.AddWorldRotation(ctx, leg.shin, ctx.right, MathUtil::DegToRad(35.0f + 25.0f * std::sin(legPhase + 0.6f)));
    }

    for (uint32 i = 0; i < rig.spineCount; i++)
    {
        out.AddWorldRotation(ctx, rig.spine[i], ctx.right, -MathUtil::DegToRad(8.0f));
    }

    out.ScaleAllStrengths(settings.strength);
}

#pragma endregion HighFall

#pragma region Writhe

RagdollWritheBehaviour::RagdollWritheBehaviour()
    : TRagdollBehaviour(NAME("Writhe"))
{
}

void RagdollWritheBehaviour::OnEvent(const RagdollBehaviourContext& ctx, const RagdollEvent& event)
{
    if (event.type == RagdollEventType::OnGround && ctx.isFreePose && !IsActive())
    {
        Start(ctx);
    }
}

void RagdollWritheBehaviour::Update(const RagdollBehaviourContext& ctx)
{
    if (IsActive() && !ctx.isFreePose)
    {
        Stop();
    }
}

void RagdollWritheBehaviour::Contribute(const RagdollBehaviourContext& ctx, RagdollBehaviourOutput& out) const
{
    const RagdollRig& rig = *ctx.rig;

    const float amplitude = MathUtil::DegToRad(settings.amplitude);
    const float time = m_time * settings.frequency;
    const float curl = MathUtil::DegToRad(settings.curl);

    const auto addMovementNoise = [&](uint32 body, float scale)
    {
        const float seed = float(body) * 2.0f;

        out.AddWorldRotation(ctx, body, ctx.right, amplitude * scale * GetBehaviourNoise(time, seed + 0.37f));
        out.AddWorldRotation(ctx, body, ctx.forward, amplitude * scale * 0.6f * GetBehaviourNoise(time * 0.83f, seed + 1.11f));
    };

    for (uint32 i = 0; i < rig.spineCount; i++)
    {
        addMovementNoise(rig.spine[i], 0.5f);
        out.AddWorldRotation(ctx, rig.spine[i], ctx.right, curl * 0.5f);
    }

    addMovementNoise(rig.neck, 0.8f);

    for (const RagdollRigArm& arm : rig.arms)
    {
        addMovementNoise(arm.clavicle, 0.3f);
        addMovementNoise(arm.upper, 1.2f);
        addMovementNoise(arm.lower, 0.8f);
        addMovementNoise(arm.hand, 0.4f);
    }

    for (const RagdollRigLeg& leg : rig.legs)
    {
        addMovementNoise(leg.thigh, 1.0f);
        addMovementNoise(leg.shin, 0.9f);
        addMovementNoise(leg.foot, 0.3f);

        out.AddWorldRotation(ctx, leg.thigh, ctx.right, -curl);
        out.AddWorldRotation(ctx, leg.shin, ctx.right, curl * 1.5f);
    }

    out.ScaleAllStrengths(settings.strength);
}

#pragma endregion Writhe

#pragma region Dying

RagdollDyingBehaviour::RagdollDyingBehaviour()
    : TRagdollBehaviour(NAME("Dying")),
      m_random(std::random_device {}())
{
}

void RagdollDyingBehaviour::OnEvent(const RagdollBehaviourContext& ctx, const RagdollEvent& event)
{
    if (event.type != RagdollEventType::Died || IsActive())
    {
        return;
    }

    Start(ctx);

    if (!IsActive())
    {
        return;
    }

    m_isHeadShot = event.body >= 0 && ctx.ragdoll->GetBodyPart(uint32(event.body)) == RagdollBodyPart::Head;

    if (m_isHeadShot)
    {
        m_duration = settings.headTime;
    }
    else
    {
        const float unit = std::uniform_real_distribution<float>(0.0f, 1.0f)(m_random);

        m_duration = settings.minTime + (MathUtil::Max(settings.maxTime, settings.minTime) - settings.minTime) * unit;
    }

    m_duration = MathUtil::Max(m_duration, 1e-3f);
}

void RagdollDyingBehaviour::OnStart(const RagdollBehaviourContext& ctx)
{
    m_isFinished = false;
    m_isHeadShot = false;

    m_duration = MathUtil::Max(settings.minTime, 1e-3f);
}

void RagdollDyingBehaviour::Update(const RagdollBehaviourContext& ctx)
{
    if (IsActive() && m_time >= m_duration)
    {
        m_isFinished = true;
    }
}

float RagdollDyingBehaviour::GetStrengthScale() const
{
    if (!IsActive())
    {
        return 1.0f;
    }

    return 1.0f - SmoothStep(m_time / m_duration);
}

void RagdollDyingBehaviour::Contribute(const RagdollBehaviourContext& ctx, RagdollBehaviourOutput& out) const
{
    const float scale = GetStrengthScale();

    out.ScaleAllStrengths(scale);

    if (!ctx.isFreePose && !m_isHeadShot && scale > settings.balanceStrength)
    {
        out.balance = RagdollBehaviourBalance::On;
    }
    else
    {
        out.balance = RagdollBehaviourBalance::Off;
    }
}

#pragma endregion Dying

} // namespace Hyperion
