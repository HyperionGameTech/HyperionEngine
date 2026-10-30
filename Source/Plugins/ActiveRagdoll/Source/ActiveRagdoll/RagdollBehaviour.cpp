/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <ActiveRagdoll/RagdollBehaviour.hpp>

#include <RagdollBehaviour.generated.inl>

namespace Hyperion {

JPH::Vec3 RagdollBehaviourContext::ToBodyLocal(uint32 body, JPH::Vec3Arg point) const
{
    const RagdollBehaviourBody& state = bodies[body];

    return state.orientation.Conjugated() * (point - state.origin);
}

JPH::Vec3 RagdollBehaviourContext::FromBodyLocal(uint32 body, JPH::Vec3Arg local) const
{
    const RagdollBehaviourBody& state = bodies[body];

    return state.origin + state.orientation * local;
}

JPH::Vec3 RagdollBehaviourContext::ToBodyDirection(uint32 body, JPH::Vec3Arg localDirection) const
{
    return bodies[body].orientation * localDirection;
}

JPH::Vec3 RagdollBehaviourContext::ToLocalDirection(uint32 body, JPH::Vec3Arg direction) const
{
    return bodies[body].orientation.Conjugated() * direction;
}

bool RagdollBehaviourContext::IsArmBody(uint32 arm, uint32 body) const
{
    const RagdollRigArm& chain = rig->arms[arm];

    return body == chain.upper || body == chain.lower || body == chain.hand;
}

int32 RagdollBehaviourContext::FindArm(uint32 body) const
{
    for (uint32 arm = 0; arm < RagdollRig::LimbCount; arm++)
    {
        if (IsArmBody(arm, body))
        {
            return int32(arm);
        }
    }

    return -1;
}

void RagdollBehaviourOutput::Init(uint32 bodyCount)
{
    pose.Resize(bodyCount);
    mask.Resize(bodyCount);
    strength.Resize(bodyCount);

    Reset();
}

void RagdollBehaviourOutput::Reset()
{
    for (size_t i = 0; i < pose.Size(); i++)
    {
        pose[i] = JPH::Quat::sIdentity();
        mask[i] = 0.0f;
        strength[i] = 1.0f;
    }

    for (RagdollBehaviourGoal& goal : hands)
    {
        goal = RagdollBehaviourGoal {};
    }

    balance = RagdollBehaviourBalance::Unset;
    gravityScale = 1.0f;
    isAdditive = true;
}

void RagdollBehaviourOutput::AddWorldRotation(const RagdollBehaviourContext& ctx, uint32 body, JPH::Vec3Arg worldAxis, float angle, float maskValue)
{
    if (body >= pose.Size() || angle == 0.0f)
    {
        return;
    }

    const JPH::Vec3 localAxis = (ctx.bodies[body].orientation.Conjugated() * worldAxis).NormalizedOr(JPH::Vec3::sAxisX());

    pose[body] = (pose[body] * JPH::Quat::sRotation(localAxis, angle)).Normalized();
    mask[body] = MathUtil::Max(mask[body], maskValue);
}

void RagdollBehaviourOutput::ScaleStrength(uint32 body, float scale)
{
    if (body < strength.Size())
    {
        strength[body] *= scale;
    }
}

void RagdollBehaviourOutput::ScaleAllStrengths(float scale)
{
    for (float& value : strength)
    {
        value *= scale;
    }
}

void RagdollBehaviourOutput::ScaleArmStrengths(const RagdollRig& rig, float scale)
{
    for (const RagdollRigArm& arm : rig.arms)
    {
        ScaleStrength(arm.clavicle, scale);
        ScaleStrength(arm.upper, scale);
        ScaleStrength(arm.lower, scale);
        ScaleStrength(arm.hand, scale);
    }
}

RagdollBehaviour::RagdollBehaviour(Name name)
    : m_name(name)
{
}

void RagdollBehaviour::Start(const RagdollBehaviourContext& ctx)
{
    if (!IsEnabled())
    {
        return;
    }

    m_time = 0.0f;
    m_isActive = true;

    if (GetTiming().fadeIn <= 0.0f)
    {
        m_weight = 1.0f;
    }

    OnStart(ctx);
}

void RagdollBehaviour::StartIfIdle(const RagdollBehaviourContext& ctx)
{
    if (!m_isActive)
    {
        Start(ctx);
    }
}

void RagdollBehaviour::Stop()
{
    m_isActive = false;

    if (GetTiming().fadeOut <= 0.0f)
    {
        m_weight = 0.0f;
    }
}

void RagdollBehaviour::Reset()
{
    m_isActive = false;
    m_weight = 0.0f;
    m_time = 0.0f;
}

void RagdollBehaviour::Advance(float deltaTime)
{
    if (!IsRunning())
    {
        return;
    }

    const RagdollBehaviourTiming timing = GetTiming();

    m_time += deltaTime;

    if (m_isActive && timing.fadeIn > 0.0f)
    {
        m_weight = MathUtil::Min(m_weight + deltaTime / timing.fadeIn, 1.0f);
    }
    else if (m_isActive)
    {
        m_weight = 1.0f;
    }
    else if (timing.fadeOut > 0.0f)
    {
        m_weight = MathUtil::Max(m_weight - deltaTime / timing.fadeOut, 0.0f);
    }
    else
    {
        m_weight = 0.0f;
    }
}

float GetBehaviourNoise(float time, float seed)
{
    static constexpr float TwoPi = 2.0f * MathUtil::pi<float>;

    const float phase = seed * 12.9898f;

    return 0.5f * std::sin(TwoPi * time + phase) + 0.3f * std::sin(TwoPi * 2.17f * time + phase * 1.7f) + 0.2f * std::sin(TwoPi * 3.41f * time + phase * 2.3f);
}

} // namespace Hyperion
