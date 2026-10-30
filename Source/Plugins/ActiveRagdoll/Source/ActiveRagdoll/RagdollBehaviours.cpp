/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <ActiveRagdoll/RagdollBehaviours.hpp>

#include <Rendering/DebugDrawer.hpp>

#include <Core/Math/Color.hpp>

#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyInterface.h>

#include <algorithm>

namespace Hyperion {

using namespace RagdollMath;

namespace {

static constexpr float GroundNormalUp = 0.5f;
static constexpr float LostSupportTime = 0.15f;
static constexpr float OnGroundTime = 0.25f;
static constexpr float OnGroundMaxSpeed = 1.5f;
static constexpr float OnGroundRearmTime = 0.5f;
static constexpr float DropRayLength = 50.0f;
static constexpr uint32 ImpactSteps = 16;
static constexpr float ImpactStep = 0.05f;

static constexpr float ElbowDown = 1.0f;
static constexpr float ElbowBack = 0.3f;
static constexpr float ElbowOut = 0.6f;

static constexpr float DebugCross = 0.05f;

void DrawCross(DebugDrawCommandList& list, JPH::Vec3Arg point, const Color& color)
{
    const Vec3f center = FromJolt(point);

    list.line(center - Vec3f(DebugCross, 0.0f, 0.0f), center + Vec3f(DebugCross, 0.0f, 0.0f), color);
    list.line(center - Vec3f(0.0f, DebugCross, 0.0f), center + Vec3f(0.0f, DebugCross, 0.0f), color);
    list.line(center - Vec3f(0.0f, 0.0f, DebugCross), center + Vec3f(0.0f, 0.0f, DebugCross), color);
}

} // namespace

RagdollBehaviours::RagdollBehaviours()
    : m_behaviours { &m_fallen, &m_stagger, &m_armBalance, &m_shot, &m_brace, &m_protectHead, &m_highFall, &m_writhe, &m_dying },
      m_random(std::random_device {}())
{
    std::copy(m_behaviours, m_behaviours + BehaviourCount, m_order);
}

bool RagdollBehaviours::Init(Ragdoll& ragdoll)
{
    Reset();

    m_rig = nullptr;

    if (!ragdoll.IsCreated() || !ragdoll.GetRig().isValid)
    {
        return false;
    }

    m_rig = &ragdoll.GetRig();

    const uint32 bodyCount = ragdoll.GetBodyCount();

    m_context.ragdoll = &ragdoll;
    m_context.rig = m_rig;
    m_context.balance = &ragdoll.GetBalanceStatus();
    m_context.bodies.Resize(bodyCount);
    m_samples.Resize(bodyCount);

    m_output.Init(bodyCount);

    m_mixLayers.Resize(bodyCount);
    m_mixStrength.Resize(bodyCount);

    for (RagdollBehaviour* behaviour : m_behaviours)
    {
        behaviour->Init(*m_rig, bodyCount);
    }

    ClearMix();

    return true;
}

void RagdollBehaviours::Reset()
{
    for (RagdollBehaviour* behaviour : m_behaviours)
    {
        behaviour->Reset();
    }

    m_context.isFreePose = false;
    m_context.isAlive = true;
    m_context.time = 0.0f;
    m_context.airTime = 0.0f;
    m_context.groundTime = 0.0f;

    m_unsupportedTime = 0.0f;
    m_offGroundTime = 0.0f;
    m_sentLostSupport = false;
    m_sentOnGround = false;
    m_frameEvents = 0;
    m_pendingEvents.Clear();

    ClearMix();
}

void RagdollBehaviours::ApplySettings(const RagdollBehaviourSettings& settings)
{
    m_fallen.SetSettings(settings.fallen);
    m_stagger.SetSettings(settings.stagger);
    m_armBalance.SetSettings(settings.armBalance);
    m_shot.SetSettings(settings.shot);
    m_brace.SetSettings(settings.brace);
    m_protectHead.SetSettings(settings.protectHead);
    m_highFall.SetSettings(settings.highFall);
    m_writhe.SetSettings(settings.writhe);
    m_dying.SetSettings(settings.dying);
}

void RagdollBehaviours::ClearMix()
{
    for (Ragdoll::BodyLayer& layer : m_mixLayers)
    {
        layer = Ragdoll::BodyLayer {};
    }

    for (float& strength : m_mixStrength)
    {
        strength = 1.0f;
    }

    for (RagdollBehaviourGoal& goal : m_mixGoals)
    {
        goal = RagdollBehaviourGoal {};
    }

    m_mixBalance = RagdollBehaviourBalance::Unset;
    m_mixGravity = 1.0f;
}

void RagdollBehaviours::SetFreePose(bool isFreePose)
{
    m_context.isFreePose = isFreePose;
}

void RagdollBehaviours::SetAlive(bool isAlive)
{
    m_context.isAlive = isAlive;
}

void RagdollBehaviours::Sense(Ragdoll& ragdoll, float deltaTime)
{
    JPH::BodyInterface& bodyInterface = ragdoll.GetPhysicsSystem()->GetBodyInterface();
    const GravityFrame frame = ragdoll.GetGravityFrame();

    RagdollBehaviourContext& ctx = m_context;

    ctx.deltaTime = deltaTime;
    ctx.time += deltaTime;
    ctx.up = frame.up;
    ctx.gravity = frame.gravity;
    ctx.balance = &ragdoll.GetBalanceStatus();
    ctx.isBalancing = ragdoll.IsBalancing();

    const bool isPowered = !ragdoll.IsLimp() && ragdoll.GetBodyMode(0) == RagdollBodyMode::Powered;
    const bool isAwake = ragdoll.IsAwake();

    bool anyContact = false;
    bool areFeetSupported = false;
    bool isBodyOnGround = false;

    for (uint32 i = 0; i < ctx.GetBodyCount(); i++)
    {
        Ragdoll::BodyState& sample = m_samples[i];
        RagdollBehaviourBody& body = ctx.bodies[i];

        ragdoll.SampleBody(bodyInterface, i, sample);

        body.origin = sample.origin;
        body.orientation = sample.orientation;
        body.center = sample.center;
        body.velocity = sample.velocity;

        RagdollGroundContact contact;
        body.hasContact = isPowered && ragdoll.GetBodyGroundContact(i, contact);
        body.isOnGround = body.hasContact && Ragdoll::IsSupportNormal(contact, frame.up, GroundNormalUp);

        anyContact = anyContact || body.hasContact;

        const RagdollBodyPart part = ragdoll.GetBodyPart(i);

        areFeetSupported = areFeetSupported || (ragdoll.IsFoot(i) && body.isOnGround);
        isBodyOnGround = isBodyOnGround || (body.isOnGround && (part == RagdollBodyPart::Torso || part == RagdollBodyPart::Head));
    }

    JPH::Vec3 centerOfMass;
    JPH::Vec3 velocity;

    if (ragdoll.GetMassAverage(m_samples.ToSpan(), centerOfMass, velocity))
    {
        ctx.centerOfMass = centerOfMass;
        ctx.velocity = velocity;
    }

    ctx.areFeetSupported = areFeetSupported;
    ctx.isBodyOnGround = isBodyOnGround;
    ctx.isAirborne = isPowered && isAwake && !anyContact;

    if (ctx.isAirborne)
    {
        ctx.airTime += deltaTime;
    }
    else
    {
        ctx.airTime = 0.0f;
    }

    if (isBodyOnGround)
    {
        ctx.groundTime += deltaTime;
    }
    else
    {
        ctx.groundTime = 0.0f;
    }

    SenseChestFrame(ragdoll);

    ctx.dropHeight = 0.0f;
    ctx.isImpactPredicted = false;

    if (!isPowered || !ctx.isFreePose)
    {
        return;
    }

    JPH::Vec3 floorPoint;
    JPH::Vec3 floorNormal;

    if (ragdoll.CastRayStatic(ctx.centerOfMass, -frame.up * DropRayLength, floorPoint, floorNormal))
    {
        ctx.dropHeight = (ctx.centerOfMass - floorPoint).Dot(frame.up);
    }
    else
    {
        ctx.dropHeight = DropRayLength;
    }

    if (!isBodyOnGround)
    {
        PredictImpact(ragdoll);
    }
}

void RagdollBehaviours::SenseChestFrame(const Ragdoll& ragdoll)
{
    RagdollBehaviourContext& ctx = m_context;

    const JPH::Quat delta = ctx.bodies[m_rig->chest].orientation * ragdoll.GetRestOrientation(m_rig->chest).Conjugated();

    ctx.forward = (delta * m_rig->restForward).Normalized();
    ctx.right = (delta * ctx.up.Cross(m_rig->restForward)).NormalizedOr(JPH::Vec3::sAxisX());
}

void RagdollBehaviours::PredictImpact(const Ragdoll& ragdoll)
{
    RagdollBehaviourContext& ctx = m_context;

    const RagdollBehaviourBody& chest = ctx.bodies[m_rig->chest];
    const JPH::Vec3 gravity = ctx.up * -ctx.gravity;

    JPH::Vec3 point = chest.center;

    for (uint32 step = 1; step <= ImpactSteps; step++)
    {
        const float time = ImpactStep * float(step);

        const JPH::Vec3 next = chest.center + chest.velocity * time + gravity * (0.5f * time * time);

        const JPH::Vec3 segment = next - point;
        const float segmentLength = segment.Length();

        JPH::Vec3 hitPoint;
        JPH::Vec3 hitNormal;

        if (ragdoll.CastRayStatic(point, segment, hitPoint, hitNormal))
        {
            float fraction = 0.0f;

            if (segmentLength > 1e-5f)
            {
                fraction = (hitPoint - point).Length() / segmentLength;
            }

            ctx.isImpactPredicted = true;
            ctx.impactPoint = hitPoint;
            ctx.impactNormal = hitNormal.NormalizedOr(ctx.up);
            ctx.impactTime = time - ImpactStep * (1.0f - MathUtil::Clamp(fraction, 0.0f, 1.0f));

            return;
        }

        point = next;
    }
}

void RagdollBehaviours::EmitSupportEvents(float deltaTime)
{
    const RagdollBehaviourContext& ctx = m_context;

    if (ctx.areFeetSupported || ctx.isBodyOnGround || !ctx.isAirborne)
    {
        m_unsupportedTime = 0.0f;

        if (ctx.areFeetSupported || ctx.isBodyOnGround)
        {
            m_sentLostSupport = false;
        }
    }
    else
    {
        m_unsupportedTime += deltaTime;

        if (m_unsupportedTime >= LostSupportTime && !m_sentLostSupport)
        {
            m_sentLostSupport = true;

            RagdollEvent event;
            event.type = RagdollEventType::LostSupport;
            event.magnitude = -ctx.velocity.Dot(ctx.up);

            Dispatch(event);
        }
    }

    if (!ctx.isBodyOnGround)
    {
        m_offGroundTime += deltaTime;

        if (m_offGroundTime >= OnGroundRearmTime)
        {
            m_sentOnGround = false;
        }

        return;
    }

    m_offGroundTime = 0.0f;

    if (!m_sentOnGround && ctx.groundTime >= OnGroundTime && ctx.velocity.Length() < OnGroundMaxSpeed)
    {
        m_sentOnGround = true;

        RagdollEvent event;
        event.type = RagdollEventType::OnGround;
        event.point = ctx.centerOfMass;

        Dispatch(event);
    }
}

void RagdollBehaviours::Dispatch(const RagdollEvent& event)
{
    static constexpr float DelayJitter = 1.0f / 3.0f;

    m_frameEvents |= 1u << uint32(event.type);

    const bool hasBody = event.body >= 0 && uint32(event.body) < m_context.GetBodyCount();

    for (uint32 i = 0; i < BehaviourCount; i++)
    {
        RagdollBehaviour* behaviour = m_behaviours[i];

        if (!behaviour->IsEnabled())
        {
            continue;
        }

        const float delay = behaviour->GetReactionDelay();

        if (delay <= 0.0f)
        {
            behaviour->OnEvent(m_context, event);

            continue;
        }

        PendingEvent pending;
        pending.event = event;
        pending.behaviour = i;
        pending.timeLeft = delay * std::uniform_real_distribution<float>(1.0f - DelayJitter, 1.0f + DelayJitter)(m_random);

        if (hasBody)
        {
            pending.localPoint = m_context.ToBodyLocal(uint32(event.body), event.point);
            pending.localDirection = m_context.ToLocalDirection(uint32(event.body), event.direction);
        }

        m_pendingEvents.PushBack(pending);
    }
}

void RagdollBehaviours::DeliverPendingEvents(float deltaTime)
{
    for (size_t i = 0; i < m_pendingEvents.Size();)
    {
        PendingEvent& pending = m_pendingEvents[i];
        pending.timeLeft -= deltaTime;

        if (pending.timeLeft > 0.0f)
        {
            i++;

            continue;
        }

        RagdollEvent event = pending.event;

        if (event.body >= 0 && uint32(event.body) < m_context.GetBodyCount())
        {
            event.point = m_context.FromBodyLocal(uint32(event.body), pending.localPoint);
            event.direction = m_context.ToBodyDirection(uint32(event.body), pending.localDirection);
        }

        RagdollBehaviour* behaviour = m_behaviours[pending.behaviour];

        m_pendingEvents.EraseAt(i);

        if (behaviour->IsEnabled())
        {
            behaviour->OnEvent(m_context, event);
        }
    }
}

void RagdollBehaviours::SortByPriority()
{
    for (uint32 i = 1; i < BehaviourCount; i++)
    {
        RagdollBehaviour* behaviour = m_order[i];
        uint32 j = i;

        while (j > 0 && m_order[j - 1]->GetPriority() > behaviour->GetPriority())
        {
            m_order[j] = m_order[j - 1];
            j--;
        }

        m_order[j] = behaviour;
    }
}

bool RagdollBehaviours::CanRun(const Ragdoll& ragdoll) const
{
    return IsValid() && ragdoll.IsCreated() && !ragdoll.IsLimp();
}

void RagdollBehaviours::Resolve(Ragdoll& ragdoll)
{
    SortByPriority();
    Mix();

    ragdoll.SetBodyLayers(m_mixLayers.ToSpan(), m_mixGravity);
}

void RagdollBehaviours::HandleEvent(Ragdoll& ragdoll, const RagdollEvent& event)
{
    if (!CanRun(ragdoll))
    {
        return;
    }

    Sense(ragdoll, 0.0f);
    Dispatch(event);
    Resolve(ragdoll);
}

void RagdollBehaviours::Update(Ragdoll& ragdoll, float deltaTime)
{
    if (!CanRun(ragdoll))
    {
        return;
    }

    m_frameEvents = 0;

    Sense(ragdoll, deltaTime);
    DeliverPendingEvents(deltaTime);
    EmitSupportEvents(deltaTime);

    for (RagdollBehaviour* behaviour : m_behaviours)
    {
        if (!behaviour->IsEnabled())
        {
            continue;
        }

        behaviour->Update(m_context);
        behaviour->Advance(deltaTime);
    }

    Resolve(ragdoll);
}

void RagdollBehaviours::Mix()
{
    ClearMix();

    for (const RagdollBehaviour* behaviour : m_order)
    {
        const float weight = behaviour->GetWeight();

        if (!behaviour->IsEnabled() || !behaviour->IsRunning() || weight <= 0.0f)
        {
            continue;
        }

        m_output.Reset();
        behaviour->Contribute(m_context, m_output);
        FoldOutput(m_output, weight);
    }
}

void RagdollBehaviours::FoldOutput(const RagdollBehaviourOutput& output, float weight)
{
    for (uint32 i = 0; i < uint32(m_mixLayers.Size()); i++)
    {
        const float mask = output.mask[i] * weight;

        if (mask > 0.0f)
        {
            if (output.isAdditive)
            {
                Ragdoll::BodyLayer& layer = m_mixLayers[i];
                layer.offset = (layer.offset * JPH::Quat::sIdentity().SLERP(output.pose[i], mask)).Normalized();
            }
            else
            {
                FoldOverride(i, output.pose[i], mask);
            }
        }

        m_mixStrength[i] *= 1.0f + (output.strength[i] - 1.0f) * weight;
    }

    m_mixGravity *= 1.0f + (output.gravityScale - 1.0f) * weight;

    for (uint32 arm = 0; arm < RagdollRig::LimbCount; arm++)
    {
        const float goalWeight = output.hands[arm].weight * weight;

        if (goalWeight > 0.0f)
        {
            SolveArm(arm, output.hands[arm], goalWeight);
        }
    }

    if (output.balance != RagdollBehaviourBalance::Unset)
    {
        m_mixBalance = output.balance;
    }
}

void RagdollBehaviours::FoldOverride(uint32 body, JPH::QuatArg pose, float weight)
{
    Ragdoll::BodyLayer& layer = m_mixLayers[body];

    const float previous = layer.weight;
    const float total = previous + (1.0f - previous) * weight;

    if (previous <= 0.0f)
    {
        layer.pose = pose;
    }
    else
    {
        layer.pose = layer.pose.SLERP(pose, weight / total).Normalized();
    }

    // an override also fades out the additive offsets folded in below it
    layer.weight = total;
    layer.offset = layer.offset.SLERP(JPH::Quat::sIdentity(), weight).Normalized();
}

void RagdollBehaviours::SolveArm(uint32 arm, const RagdollBehaviourGoal& goal, float weight)
{
    const RagdollBehaviourContext& ctx = m_context;
    const RagdollRigArm& chain = m_rig->arms[arm];

    const JPH::Vec3 chestUp = ctx.GetChestUp();
    const JPH::Vec3 shoulder = ctx.bodies[chain.upper].origin;
    const JPH::Vec3 hint = -chestUp * ElbowDown - ctx.forward * ElbowBack + ctx.right * (chain.sideSign * ElbowOut);

    const TwoBoneIKResult ik = SolveTwoBoneIK(shoulder, goal.target, chain.upperLength, chain.lowerLength, hint);

    const JPH::Vec3 upperDirection = (ik.mid - shoulder).NormalizedOr(-chestUp);
    const JPH::Vec3 lowerDirection = (ik.end - ik.mid).NormalizedOr(upperDirection);

    JPH::Vec3 hinge = upperDirection.Cross(lowerDirection);

    if (hinge.Length() < 1e-3f)
    {
        hinge = upperDirection.Cross(hint);
    }

    hinge = hinge.NormalizedOr(ctx.right);

    JPH::Quat upperWorld;
    JPH::Quat lowerWorld;
    ctx.ragdoll->GetHingeChainRotations(chain.upper, chain.lower, upperDirection, lowerDirection, hinge, upperWorld, lowerWorld);

    FoldOverride(chain.upper, RelativeRotation(ctx.bodies[chain.clavicle].orientation, upperWorld), weight);
    FoldOverride(chain.lower, RelativeRotation(upperWorld, lowerWorld), weight);

    if (weight >= m_mixGoals[arm].weight)
    {
        m_mixGoals[arm] = RagdollBehaviourGoal { goal.target, weight };
    }
}

RagdollBehaviour* RagdollBehaviours::Find(Name name) const
{
    for (RagdollBehaviour* behaviour : m_behaviours)
    {
        if (behaviour->GetName() == name)
        {
            return behaviour;
        }
    }

    return nullptr;
}

bool RagdollBehaviours::Trigger(Ragdoll& ragdoll, Name name)
{
    RagdollBehaviour* behaviour = Find(name);

    if (behaviour == nullptr || !CanRun(ragdoll))
    {
        return false;
    }

    Sense(ragdoll, 0.0f);
    behaviour->Start(m_context);
    Resolve(ragdoll);

    return behaviour->IsActive();
}

bool RagdollBehaviours::Stop(Name name)
{
    RagdollBehaviour* behaviour = Find(name);

    if (behaviour == nullptr)
    {
        return false;
    }

    behaviour->Stop();

    return true;
}

bool RagdollBehaviours::IsRunning(Name name) const
{
    const RagdollBehaviour* behaviour = Find(name);

    return behaviour != nullptr && behaviour->IsRunning();
}

bool RagdollBehaviours::IsIdle() const
{
    for (const RagdollBehaviour* behaviour : m_behaviours)
    {
        if (behaviour->IsRunning())
        {
            return false;
        }
    }

    return true;
}

bool RagdollBehaviours::HadEvent(RagdollEventType type) const
{
    return (m_frameEvents & (1u << uint32(type))) != 0;
}

void RagdollBehaviours::DebugDraw(DebugDrawCommandList& list) const
{
    if (!IsValid() || m_context.bodies.Empty())
    {
        return;
    }

    if (m_context.isImpactPredicted)
    {
        DrawCross(list, m_context.impactPoint, Color(1.0f, 0.24f, 0.24f));
    }

    for (uint32 arm = 0; arm < RagdollRig::LimbCount; arm++)
    {
        const RagdollBehaviourGoal& goal = m_mixGoals[arm];

        if (goal.weight <= 0.0f)
        {
            continue;
        }

        const JPH::Vec3 shoulder = m_context.bodies[m_rig->arms[arm].upper].origin;
        const Color color(0.16f, 0.3f + 0.7f * MathUtil::Clamp(goal.weight, 0.0f, 1.0f), 0.16f);

        list.line(FromJolt(shoulder), FromJolt(goal.target), color);
        DrawCross(list, goal.target, color);
    }
}

} // namespace Hyperion
