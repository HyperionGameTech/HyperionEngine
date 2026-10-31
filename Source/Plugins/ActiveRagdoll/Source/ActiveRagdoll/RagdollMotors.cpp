/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <ActiveRagdoll/Ragdoll.hpp>

#include <Physics/PhysicsWorld.hpp>

#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Constraints/SwingTwistConstraint.h>

namespace Hyperion {

using namespace RagdollMath;

namespace {

static constexpr float TwoPi = 2.0f * MathUtil::pi<float>;

static constexpr float MotorMinFrequencyScale = 0.2f;
static constexpr float MinSubtreeInertia = 1e-4f;
static constexpr float GravityCompFullStrength = 0.25f;
static constexpr float OverrideFadeTime = 0.4f;

// Hill's force-velocity curve, linearised: a muscle shortening this fast (rad/s) has this fraction of its strength left
static constexpr float MuscleMaxShorteningSpeed = 20.0f;
static constexpr float MuscleMinForceScale = 0.5f;

static constexpr float RootAssistFrequency = 6.0f;
static constexpr float RootAssistDamping = 1.0f;
static constexpr float RootAssistMaxForce = 1500.0f;
static constexpr float RootAssistMaxTorque = 600.0f;

// a spring of natural frequency `omega` and damping ratio `damping` for `mass` (or a moment of inertia), times `scale`
JPH::SpringSettings MakeSpring(
    float mass,
    float omega,
    float damping,
    float scale = 1.0f)
{
    return JPH::SpringSettings(JPH::ESpringMode::StiffnessAndDamping, scale * mass * omega * omega, scale * 2.0f * damping * omega * mass);
}

// how far ahead to aim a damped spring along the target's velocity so it tracks without lag (2 zeta / omega)
float GetLeadTime(float damping, float omega)
{
    return 2.0f * damping / omega;
}

} // namespace

void Ragdoll::OnStep(const JPH::PhysicsStepListenerContext& inContext)
{
    if (m_ragdoll == nullptr)
    {
        return;
    }

    JPH::BodyInterface& bodyInterface = inContext.mPhysicsSystem->GetBodyInterfaceNoLock();

    if (inContext.mIsFirstStep)
    {
        m_substep = 0;
    }

    m_stepDeltaTime = inContext.mDeltaTime;

    const uint32 numSteps = static_cast<PhysicsWorld*>(m_physicsWorld)->GetAdapter().GetNumCollisionSteps();
    const uint32 stepsLeft = MathUtil::Max(numSteps - MathUtil::Min(m_substep, numSteps), 1u);

    float fraction = 1.0f;

    if (!inContext.mIsLastStep)
    {
        fraction = 1.0f / float(stepsLeft);
    }

    m_substep++;

    MoveKinematicBodies(bodyInterface, fraction, inContext.mDeltaTime);

    const JPH::Vec3 gravity = inContext.mPhysicsSystem->GetGravity();

    // ground contacts are cleared between updates and filled during collision steps, so on the first step after a clear
    // they are empty and balance keeps its previous contact state
    const uint32 contactGeneration = m_contacts->GetGeneration();
    const bool contactsFresh = contactGeneration == m_contactGeneration;
    m_contactGeneration = contactGeneration;

    if (!m_isLimp)
    {
        UpdateSubtrees(bodyInterface);

        if (m_isBalancing)
        {
            m_balance.Step(*this, bodyInterface, inContext.mDeltaTime, gravity, contactsFresh);
        }
    }

    for (uint32 i = 0; i < GetBodyCount(); i++)
    {
        ApplyMotor(i);
    }

    if (!m_isBalancing && m_overrideFade > 0.0f)
    {
        m_overrideFade = MathUtil::Max(m_overrideFade - inContext.mDeltaTime / OverrideFadeTime, 0.0f);
    }

    if (m_isLimp)
    {
        if (m_rootAssistConstraint != nullptr)
        {
            m_rootAssistConstraint->SetEnabled(false);
        }

        return;
    }

    ApplyGravityCompensation(bodyInterface, gravity);
    ApplyRootAssist(bodyInterface, gravity);
}

void Ragdoll::MoveKinematicBodies(JPH::BodyInterface& bodyInterface, float fraction, float deltaTime)
{
    for (uint32 i = 0; i < GetBodyCount(); i++)
    {
        if (m_bodies[i].mode != RagdollBodyMode::Kinematic)
        {
            continue;
        }

        const BodyTarget& target = m_targets[i];
        const JPH::BodyID bodyId = GetBodyID(i);

        JPH::RVec3 position;
        JPH::Quat orientation;
        bodyInterface.GetPositionAndRotation(bodyId, position, orientation);

        const JPH::Vec3 stepPosition = JPH::Vec3(position) + (target.position - JPH::Vec3(position)) * fraction;
        const JPH::Quat stepOrientation = orientation.SLERP(target.orientation, fraction).Normalized();

        bodyInterface.MoveKinematic(bodyId, JPH::RVec3(stepPosition), stepOrientation, deltaTime);
    }
}

void Ragdoll::UpdateSubtrees(JPH::BodyInterface& bodyInterface)
{
    for (uint32 i = 0; i < GetBodyCount(); i++)
    {
        const BodyInfo& info = m_bodies[i];
        BodyState& state = m_states[i];

        SampleBody(bodyInterface, i, state);

        float mass = 0.0f;
        float inertia = 0.0f;

        if (info.mode != RagdollBodyMode::Kinematic)
        {
            mass = info.mass;
        }

        if (mass > 0.0f)
        {
            inertia = info.inertia;
        }

        state.subtreeMass = mass;
        state.subtreeMoment = state.center * mass;
        state.subtreeSecondMoment = mass * state.center.LengthSq() + inertia;
    }

    for (uint32 i = GetBodyCount() - 1; i > 0; i--)
    {
        const BodyState& child = m_states[i];
        BodyState& parent = m_states[m_bodies[i].parentBody];

        parent.subtreeMass += child.subtreeMass;
        parent.subtreeMoment += child.subtreeMoment;
        parent.subtreeSecondMoment += child.subtreeSecondMoment;
    }
}

float Ragdoll::GetRawSubtreeInertia(uint32 bodyIndex, JPH::Vec3Arg pivot) const
{
    const BodyState& state = m_states[bodyIndex];

    return state.subtreeSecondMoment - 2.0f * pivot.Dot(state.subtreeMoment) + state.subtreeMass * pivot.LengthSq();
}

float Ragdoll::GetSubtreeInertia(uint32 bodyIndex, JPH::Vec3Arg pivot) const
{
    return MathUtil::Max(GetRawSubtreeInertia(bodyIndex, pivot), MinSubtreeInertia);
}

float Ragdoll::GetSupportedInertia(uint32 bodyIndex, JPH::Vec3Arg pivot) const
{
    const float inertia = GetRawSubtreeInertia(0, pivot) - GetRawSubtreeInertia(bodyIndex, pivot);

    return MathUtil::Max(inertia, MinSubtreeInertia);
}

JPH::SwingTwistConstraint* Ragdoll::GetJoint(uint32 bodyIndex) const
{
    // Jolt orders constraints by child part, and the root part has none
    return static_cast<JPH::SwingTwistConstraint*>(m_ragdoll->GetConstraint(int(bodyIndex) - 1));
}

Ragdoll::MotorOverride& Ragdoll::SetMotorTarget(
    uint32 bodyIndex,
    JPH::QuatArg parentWorld,
    JPH::QuatArg childWorld,
    float frequencyScale,
    bool isSupported)
{
    MotorOverride& motorOverride = m_motorOverrides[bodyIndex];

    motorOverride.mode = MotorOverrideMode::Target;
    motorOverride.target = RelativeRotation(parentWorld, childWorld);
    motorOverride.frequencyScale = frequencyScale;
    motorOverride.isSupported = isSupported;

    return motorOverride;
}

void Ragdoll::GetHingeChainRotations(
    uint32 upper,
    uint32 lower,
    JPH::Vec3Arg upperDirection,
    JPH::Vec3Arg lowerDirection,
    JPH::Vec3Arg hinge,
    JPH::Quat& outUpper,
    JPH::Quat& outLower) const
{
    const BodyInfo& upperBody = m_bodies[upper];
    const BodyInfo& lowerBody = m_bodies[lower];

    outUpper = FrameRotation(upperBody.localAxis, lowerBody.parentHingeLocal, upperDirection, hinge);
    outLower = FrameRotation(lowerBody.localAxis, lowerBody.hingeLocal, lowerDirection, hinge);
}

void Ragdoll::ApplyMotor(uint32 bodyIndex)
{
    const BodyInfo& info = m_bodies[bodyIndex];

    if (!m_hasMotors || info.parentBody < 0)
    {
        return;
    }

    const BodyTarget& target = m_targets[bodyIndex];
    JPH::SwingTwistConstraint* joint = GetJoint(bodyIndex);

    const MotorOverride& motorOverride = m_motorOverrides[bodyIndex];
    const bool isPowered = !m_isLimp && info.mode == RagdollBodyMode::Powered && motorOverride.mode != MotorOverrideMode::Off;

    if (!isPowered)
    {
        joint->SetSwingMotorState(JPH::EMotorState::Off);
        joint->SetTwistMotorState(JPH::EMotorState::Off);

        return;
    }

    const float frequency = info.muscle.stiffness * motorOverride.frequencyScale * (MotorMinFrequencyScale + (1.0f - MotorMinFrequencyScale) * info.strength);
    const float omega = TwoPi * frequency;

    float inertia = motorOverride.inertia;

    if (inertia <= 0.0f && motorOverride.isSupported)
    {
        inertia = GetSupportedInertia(bodyIndex, m_states[bodyIndex].origin);
    }
    else if (inertia <= 0.0f)
    {
        inertia = GetSubtreeInertia(bodyIndex, m_states[bodyIndex].origin);
    }

    JPH::Quat jointTarget = motorOverride.target;

    if (motorOverride.mode != MotorOverrideMode::Target)
    {
        const BodyTarget& parentTarget = m_targets[info.parentBody];
        const JPH::Quat parentInverse = parentTarget.orientation.Conjugated();
        const JPH::Vec3 relativeVelocity = parentInverse * (target.angularVelocity - parentTarget.angularVelocity);
        const float lead = GetLeadTime(info.muscle.damping, omega);

        jointTarget = IntegrateRotation(parentInverse * target.orientation, relativeVelocity, lead);
    }
    else
    {
        m_overrideTargets[bodyIndex] = jointTarget;
    }

    if (m_isBalancing)
    {
        m_hadOverride[bodyIndex] = motorOverride.mode == MotorOverrideMode::Target;
    }
    else if (m_overrideFade > 0.0f && m_hadOverride[bodyIndex])
    {
        jointTarget = jointTarget.SLERP(m_overrideTargets[bodyIndex], m_overrideFade);
    }

    const BodyLayer& layer = m_layers[bodyIndex];

    if (layer.weight > 0.0f)
    {
        jointTarget = jointTarget.SLERP(layer.pose, layer.weight);
    }

    jointTarget = (jointTarget * layer.offset).Normalized();

    const JPH::SpringSettings spring = MakeSpring(inertia, omega, info.muscle.damping);
    const float torque = info.muscle.maxTorque * info.strength * GetForceVelocityScale(bodyIndex, jointTarget);

    JPH::MotorSettings& swing = joint->GetSwingMotorSettings();
    JPH::MotorSettings& twist = joint->GetTwistMotorSettings();

    swing.mSpringSettings = spring;
    swing.SetTorqueLimit(torque);
    twist.mSpringSettings = spring;
    twist.SetTorqueLimit(torque);

    joint->SetSwingMotorState(JPH::EMotorState::Position);
    joint->SetTwistMotorState(JPH::EMotorState::Position);

    joint->SetTargetOrientationBS(jointTarget);
}

float Ragdoll::GetForceVelocityScale(uint32 bodyIndex, JPH::QuatArg jointTarget) const
{
    const BodyState& child = m_states[bodyIndex];
    const BodyState& parent = m_states[m_bodies[bodyIndex].parentBody];

    const JPH::Quat parentInverse = parent.orientation.Conjugated();
    const JPH::Vec3 jointVelocity = parentInverse * (child.angularVelocity - parent.angularVelocity);
    const JPH::Vec3 jointError = GetRotationVector(jointTarget * RelativeRotation(parent.orientation, child.orientation).Conjugated());

    // only a muscle shortening towards its target loses strength with speed; braking a joint, it keeps all of it
    const float shorteningSpeed = jointVelocity.Dot(jointError.NormalizedOr(JPH::Vec3::sZero()));

    if (shorteningSpeed <= 0.0f)
    {
        return 1.0f;
    }

    return MathUtil::Max(1.0f - shorteningSpeed / MuscleMaxShorteningSpeed, MuscleMinForceScale);
}

void Ragdoll::ApplyGravityCompensation(JPH::BodyInterface& bodyInterface, JPH::Vec3Arg gravity)
{
    for (uint32 i = 1; i < GetBodyCount(); i++)
    {
        const BodyInfo& info = m_bodies[i];
        const BodyState& state = m_states[i];

        if (info.mode != RagdollBodyMode::Powered || m_motorOverrides[i].skipGravityComp)
        {
            continue;
        }

        const float scale = MathUtil::Clamp(info.strength / GravityCompFullStrength, 0.0f, 1.0f) * m_layers[i].gravityScale;

        if (scale <= 0.0f || state.subtreeMass <= 0.0f)
        {
            continue;
        }

        const JPH::BodyID bodyId = GetBodyID(i);
        const JPH::Vec3 gravityTorque = (state.subtreeMoment - state.origin * state.subtreeMass).Cross(gravity) * scale;

        AddStepTorque(bodyInterface, bodyId, -gravityTorque);

        if (m_bodies[info.parentBody].mode != RagdollBodyMode::Kinematic)
        {
            AddStepTorque(bodyInterface, GetBodyID(uint32(info.parentBody)), gravityTorque);
        }
    }
}

void Ragdoll::ApplyRootAssist(JPH::BodyInterface& bodyInterface, JPH::Vec3Arg gravity)
{
    const bool isActive = m_rootAssistConstraint != nullptr && m_rootAssist > 0.0f && !m_isBalancing && m_bodies[0].mode == RagdollBodyMode::Powered;

    if (m_rootAssistConstraint != nullptr)
    {
        m_rootAssistConstraint->SetEnabled(isActive);
    }

    if (!isActive)
    {
        return;
    }

    const BodyTarget& target = m_targets[0];
    const BodyState& root = m_states[0];

    const float totalMass = root.subtreeMass;
    const JPH::Vec3 centerOfMass = root.subtreeMoment / totalMass;
    const float inertia = GetSubtreeInertia(0, root.center);

    const float omega = TwoPi * RootAssistFrequency;
    const float lead = GetLeadTime(RootAssistDamping, omega);

    const JPH::SpringSettings linearSpring = MakeSpring(totalMass, omega, RootAssistDamping, m_rootAssist);
    const JPH::SpringSettings angularSpring = MakeSpring(inertia, omega, RootAssistDamping, m_rootAssist);

    for (uint32 axis = 0; axis < JPH::SixDOFConstraint::EAxis::Num; axis++)
    {
        const JPH::SixDOFConstraint::EAxis joltAxis = JPH::SixDOFConstraint::EAxis(axis);
        JPH::MotorSettings& motor = m_rootAssistConstraint->GetMotorSettings(joltAxis);

        if (axis < JPH::SixDOFConstraint::EAxis::RotationX)
        {
            motor.mSpringSettings = linearSpring;
            motor.SetForceLimit(m_rootAssist * RootAssistMaxForce);
        }
        else
        {
            motor.mSpringSettings = angularSpring;
            motor.SetTorqueLimit(m_rootAssist * RootAssistMaxTorque);
        }

        m_rootAssistConstraint->SetMotorState(joltAxis, JPH::EMotorState::Position);
    }

    m_rootAssistConstraint->SetTargetPositionCS(target.position + target.linearVelocity * lead - m_rootAnchor);
    m_rootAssistConstraint->SetTargetOrientationBS(IntegrateRotation(target.orientation, target.angularVelocity, lead));

    const JPH::BodyID rootId = GetBodyID(0);

    AddStepForce(bodyInterface, rootId, -gravity * (totalMass * m_rootAssist));
    AddStepTorque(bodyInterface, rootId, -(centerOfMass - root.center).Cross(gravity * (totalMass * m_rootAssist)));
}

void Ragdoll::AddStepForce(JPH::BodyInterface& bodyInterface, JPH::BodyID bodyId, JPH::Vec3Arg force) const
{
    bodyInterface.AddImpulse(bodyId, force * m_stepDeltaTime);
}

void Ragdoll::AddStepTorque(JPH::BodyInterface& bodyInterface, JPH::BodyID bodyId, JPH::Vec3Arg torque) const
{
    bodyInterface.AddAngularImpulse(bodyId, torque * m_stepDeltaTime);
}

void Ragdoll::ClearMotorOverrides()
{
    for (MotorOverride& motorOverride : m_motorOverrides)
    {
        motorOverride = MotorOverride {};
    }
}

float Ragdoll::GetMotorTorqueFraction(uint32 bodyIndex) const
{
    const BodyInfo& info = m_bodies[bodyIndex];

    if (!m_hasMotors || info.parentBody < 0)
    {
        return 0.0f;
    }

    const float limit = info.muscle.maxTorque * info.strength;

    if (limit <= 0.0f)
    {
        return 0.0f;
    }

    const JPH::Vec3 torque = GetJoint(bodyIndex)->GetTotalLambdaMotor() / m_stepDeltaTime;

    return torque.Length() / limit;
}

} // namespace Hyperion
