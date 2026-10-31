/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <ActiveRagdoll/Ragdoll.hpp>

#include <Physics/PhysicsWorld.hpp>

#include <Scene/Animation/Skeleton.hpp>

#include <Core/Logging/Logger.hpp>
#include <Core/Logging/LogChannels.hpp>

#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Constraints/SwingTwistConstraint.h>
#include <Jolt/Skeleton/Skeleton.h>

#include <atomic>
#include <string_view>

namespace Hyperion {

using namespace RagdollMath;

namespace {

static constexpr float SelfCollisionMargin = 0.02f;
static constexpr uint32 SolverVelocitySteps = 20;
static constexpr float HingeSlackDegrees = 5.0f;
static constexpr float HingeMinBend = 0.17f;

struct BuildFrame
{
    const SkeletonBindData& bindData;
    Span<const Mat4f> modelMatrices;
    const Mat4f& entityWorldMatrix;
    Span<const RagdollResolvedBody> bodies;

    float scale = 1.0f;
    float totalMass = 0.0f;

    JPH::Vec3 up = JPH::Vec3::sAxisY();
    JPH::Vec3 forward = JPH::Vec3::sZero();

    Mat4f GetBoneWorld(uint32 slot) const
    {
        return entityWorldMatrix * modelMatrices[slot];
    }

    JPH::Vec3 GetBonePosition(uint32 slot) const
    {
        return ToJolt(GetBoneWorld(slot).ExtractTranslation());
    }

    // the middle of the bone's children, or past a leaf bone by its own length
    JPH::Vec3 GetBoneTip(uint32 slot) const
    {
        JPH::Vec3 sum = JPH::Vec3::sZero();
        uint32 count = 0;

        for (uint32 child = 0; child < bindData.NumSlots(); child++)
        {
            if (bindData.parentSlots[child] == slot)
            {
                sum += GetBonePosition(child);
                count++;
            }
        }

        const JPH::Vec3 position = GetBonePosition(slot);

        if (count > 0)
        {
            return sum / float(count);
        }

        const uint32 parentSlot = bindData.parentSlots[slot];

        if (parentSlot == SkeletonBindData::InvalidSlot)
        {
            return position + up * (0.1f * scale);
        }

        return position + (position - GetBonePosition(parentSlot));
    }

    JPH::Vec3 GetBodyEnd(const RagdollResolvedBody& body) const
    {
        if (body.endAtTip)
        {
            return GetBoneTip(body.endSlot);
        }

        return GetBonePosition(body.endSlot);
    }

    JPH::Vec3 GetBodySpan(const RagdollResolvedBody& body) const
    {
        return GetBodyEnd(body) - GetBonePosition(body.slot);
    }
};

struct BodyShape
{
    JPH::Vec3 centerLocal = JPH::Vec3::sZero();
    JPH::Vec3 boxHalfExtent = JPH::Vec3::sZero();
    JPH::Quat boxRotation = JPH::Quat::sIdentity();
    JPH::RefConst<JPH::Shape> shape;
    bool isBox = false;
};

struct BuiltBody
{
    BodyShape shape;
    JPH::Vec3 position = JPH::Vec3::sZero();
    JPH::Vec3 axisLocal = JPH::Vec3::sAxisY();
    JPH::Vec3 hinge = JPH::Vec3::sZero();
    JPH::Quat orientation = JPH::Quat::sIdentity();
    float length = 0.0f;
    float radius = 0.0f;
    bool isHinge = false;
};

int32 FindParentBody(const SkeletonBindData& bindData, Span<const int32> bodyOfSlot, uint32 slot)
{
    for (uint32 ancestor = bindData.parentSlots[slot]; ancestor != SkeletonBindData::InvalidSlot; ancestor = bindData.parentSlots[ancestor])
    {
        if (bodyOfSlot[ancestor] >= 0)
        {
            return bodyOfSlot[ancestor];
        }
    }

    return -1;
}

JPH::Vec3 GetRestForward(const BuildFrame& frame)
{
    const JPH::Vec3 bodyUp = frame.GetBodySpan(frame.bodies[0]).NormalizedOr(JPH::Vec3::sAxisY());

    JPH::Vec3 forward = JPH::Vec3::sZero();

    for (const RagdollResolvedBody& body : frame.bodies)
    {
        if (body.definition->foot)
        {
            forward += frame.GetBodySpan(body);
        }
    }

    return (forward - bodyUp * forward.Dot(bodyUp)).NormalizedOr(JPH::Vec3::sZero());
}

BodyShape BuildCapsule(
    JPH::Vec3Arg axisLocal,
    float length,
    float radius,
    float scale)
{
    const float halfHeight = MathUtil::Max(length * 0.5f - radius, 0.01f * scale);

    BodyShape shape;
    shape.centerLocal = axisLocal * (length * 0.5f);

    JPH::RefConst<JPH::Shape> capsule = new JPH::CapsuleShape(halfHeight, radius);
    shape.shape = new JPH::RotatedTranslatedShape(shape.centerLocal, JPH::Quat::sFromTo(JPH::Vec3::sAxisY(), axisLocal), capsule);

    return shape;
}

BodyShape BuildFootBox(
    const BuildFrame& frame,
    const RagdollBodyDefinition& definition,
    const JPH::Mat44& rotation,
    JPH::Vec3Arg position,
    JPH::Vec3Arg endPosition,
    JPH::Vec3Arg axisWorld,
    float radius)
{
    const JPH::Vec3 footForward = (axisWorld - frame.up * axisWorld.Dot(frame.up)).NormalizedOr(frame.forward);
    const JPH::Vec3 footSide = frame.up.Cross(footForward);

    const float top = position.Dot(frame.up);
    const float bottom = MathUtil::Min(top, endPosition.Dot(frame.up)) - radius;
    const float heel = -definition.heelLength * frame.scale;
    const float toe = (endPosition - position).Dot(footForward) + radius + definition.extraLength * frame.scale;

    const float boxHalfHeight = MathUtil::Max((top - bottom) * 0.5f, radius * 0.5f);

    BodyShape shape;
    shape.isBox = true;
    shape.boxHalfExtent = JPH::Vec3(definition.footWidth * frame.scale * 0.5f, boxHalfHeight, (toe - heel) * 0.5f);

    const JPH::Vec3 centerWorld = position + footForward * ((heel + toe) * 0.5f) + frame.up * (bottom + boxHalfHeight - top);

    const JPH::Mat44 boxWorld(JPH::Vec4(footSide, 0.0f), JPH::Vec4(frame.up, 0.0f), JPH::Vec4(footForward, 0.0f), JPH::Vec4(0.0f, 0.0f, 0.0f, 1.0f));

    shape.boxRotation = (rotation.Transposed3x3() * boxWorld).GetQuaternion().Normalized();
    shape.centerLocal = rotation.Multiply3x3Transposed(centerWorld - position);

    const float convexRadius = MathUtil::Min(0.01f * frame.scale, shape.boxHalfExtent.ReduceMin() * 0.5f);

    JPH::RefConst<JPH::Shape> box = new JPH::BoxShape(shape.boxHalfExtent, convexRadius);
    shape.shape = new JPH::RotatedTranslatedShape(shape.centerLocal, shape.boxRotation, box);

    return shape;
}

bool ApplyHingeLimits(
    JPH::SwingTwistConstraintSettings& joint,
    const BuildFrame& frame,
    const RagdollBodyDefinition& definition,
    const RagdollResolvedBody& parentBody,
    JPH::Vec3Arg axisWorld,
    JPH::Vec3& outHinge)
{
    const float flex = MathUtil::DegToRad(definition.flex);
    const float slack = MathUtil::DegToRad(HingeSlackDegrees);

    const JPH::Vec3 parentAxis = frame.GetBodySpan(parentBody).NormalizedOr(axisWorld);

    JPH::Vec3 flexDirection = -frame.forward;

    if (definition.joint == RagdollJoint::HingeForward)
    {
        flexDirection = frame.forward;
    }

    const JPH::Vec3 bendAxis = parentAxis.Cross(axisWorld);

    JPH::Vec3 hinge = axisWorld.Cross(flexDirection);

    if (bendAxis.Length() > HingeMinBend && (hinge.LengthSq() < 0.01f || bendAxis.Dot(hinge) > 0.0f))
    {
        hinge = bendAxis;
    }

    if (hinge.LengthSq() > 0.01f)
    {
        hinge = hinge.Normalized();
        outHinge = hinge;

        const float rest = std::atan2(hinge.Dot(parentAxis.Cross(axisWorld)), parentAxis.Dot(axisWorld));
        const float middle = (flex - slack) * 0.5f;

        joint.mTwistAxis1 = JPH::Quat::sRotation(hinge, middle - rest) * axisWorld;
        joint.mPlaneAxis1 = joint.mPlaneAxis2 = hinge;
        joint.mNormalHalfConeAngle = (flex + slack) * 0.5f;
        joint.mPlaneHalfConeAngle = slack;

        return true;
    }

    joint.mNormalHalfConeAngle = joint.mPlaneHalfConeAngle = flex * 0.5f;

    return false;
}

// an elliptical cone whose centre is turned towards the larger side of each range, so e.g. a hip flexes far forward but
// barely extends back. Ranges are measured from the anatomical neutral, with upper arms and thighs hanging straight down,
// so they don't depend on the pose the ragdoll happened to be built in
bool ApplyAnatomicalLimits(
    JPH::SwingTwistConstraintSettings& joint,
    const BuildFrame& frame,
    const RagdollBodyDefinition& definition,
    JPH::Vec3Arg position,
    JPH::Vec3Arg axisWorld)
{
    if (definition.swingForward + definition.swingBack + definition.swingOut + definition.swingIn <= 0.0f)
    {
        return false;
    }

    JPH::Vec3 neutral = axisWorld;

    if ((definition.part == RagdollBodyPart::Arm || definition.part == RagdollBodyPart::Leg) && !definition.foot)
    {
        neutral = -frame.up;
    }

    const JPH::Quat toNeutral = JPH::Quat::sFromTo(axisWorld, neutral);

    JPH::Vec3 front = frame.forward - neutral * frame.forward.Dot(neutral);

    if (front.LengthSq() < 0.1f)
    {
        front = frame.up - neutral * frame.up.Dot(neutral);
    }

    front = front.NormalizedOr(neutral.GetNormalizedPerpendicular());

    const JPH::Vec3 side = frame.up.Cross(frame.forward);
    const JPH::Vec3 pelvis = frame.GetBonePosition(frame.bodies[0].slot);

    JPH::Vec3 out = neutral.Cross(front).Normalized();

    if (std::abs(out.Dot(side)) < 0.3f)
    {
        out *= GetSign(out.Dot(frame.up));
    }
    else
    {
        out *= GetSign(out.Dot(side)) * GetSign((position - pelvis).Dot(side));
    }

    const JPH::Vec3 sagittalAxis = neutral.Cross(front).Normalized();
    const JPH::Vec3 frontalAxis = neutral.Cross(out).Normalized();

    const float forward = MathUtil::DegToRad(definition.swingForward);
    const float back = MathUtil::DegToRad(definition.swingBack);
    const float outward = MathUtil::DegToRad(definition.swingOut);
    const float inward = MathUtil::DegToRad(definition.swingIn);

    // a pure swing to the centre, since chaining the two turns would add a twist; a pyramid rather than a cone keeps the
    // corners of the range, which a lopsided range such as the shoulder's puts the neutral pose in
    const JPH::Vec3 centreAxis = JPH::Quat::sRotation(frontalAxis, (outward - inward) * 0.5f) * (JPH::Quat::sRotation(sagittalAxis, (forward - back) * 0.5f) * neutral);
    const JPH::Quat centre = JPH::Quat::sFromTo(neutral, centreAxis);

    joint.mSwingType = JPH::ESwingType::Pyramid;
    joint.mTwistAxis1 = centre * neutral;
    joint.mPlaneAxis1 = centre * sagittalAxis;
    joint.mTwistAxis2 = axisWorld;
    joint.mPlaneAxis2 = toNeutral.Conjugated() * sagittalAxis;
    // Jolt's constraint frame is (twist, normal, plane): the plane angle limits turning about the normal axis, the
    // normal angle about the plane axis, which here is the flexion axis
    joint.mNormalHalfConeAngle = MathUtil::Min((forward + back) * 0.5f, MathUtil::pi<float>);
    joint.mPlaneHalfConeAngle = MathUtil::Min((outward + inward) * 0.5f, MathUtil::pi<float>);

    return true;
}

JPH::SwingTwistConstraintSettings* BuildJoint(
    const BuildFrame& frame,
    const RagdollBodyDefinition& definition,
    const RagdollResolvedBody& parentBody,
    JPH::Vec3Arg position,
    JPH::Vec3Arg axisWorld,
    BuiltBody& outBody)
{
    JPH::SwingTwistConstraintSettings* joint = new JPH::SwingTwistConstraintSettings;

    joint->mSpace = JPH::EConstraintSpace::WorldSpace;
    joint->mPosition1 = joint->mPosition2 = JPH::RVec3(position);
    joint->mTwistAxis1 = joint->mTwistAxis2 = axisWorld;
    joint->mPlaneAxis1 = joint->mPlaneAxis2 = axisWorld.GetNormalizedPerpendicular();
    joint->mNormalHalfConeAngle = joint->mPlaneHalfConeAngle = MathUtil::DegToRad(definition.swing);
    joint->mTwistMinAngle = -MathUtil::DegToRad(definition.twist);
    joint->mTwistMaxAngle = MathUtil::DegToRad(definition.twist);
    joint->mMaxFrictionTorque = definition.jointFriction * frame.scale * frame.scale * frame.scale;
    joint->mNumVelocityStepsOverride = SolverVelocitySteps;
    joint->mSwingMotorSettings = JPH::MotorSettings(definition.muscle.stiffness, definition.muscle.damping);
    joint->mTwistMotorSettings = JPH::MotorSettings(definition.muscle.stiffness, definition.muscle.damping);

    if (definition.joint != RagdollJoint::Cone)
    {
        outBody.isHinge = ApplyHingeLimits(*joint, frame, definition, parentBody, axisWorld, outBody.hinge);
    }
    else
    {
        ApplyAnatomicalLimits(*joint, frame, definition, position, axisWorld);
    }

    return joint;
}

void ConfigurePart(
    JPH::RagdollSettings::Part& part,
    const BuiltBody& body,
    const RagdollBodyDefinition& definition,
    float mass)
{
    part.SetShape(body.shape.shape);
    part.mPosition = JPH::RVec3(body.position);
    part.mRotation = body.orientation;
    part.mMotionType = JPH::EMotionType::Dynamic;
    part.mObjectLayer = JoltLayers::RAGDOLL;
    part.mFriction = definition.friction;
    part.mRestitution = 0.0f;
    part.mLinearDamping = 0.05f;
    part.mAngularDamping = 0.3f;
    part.mNumVelocityStepsOverride = SolverVelocitySteps;
    part.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
    part.mMassPropertiesOverride.mMass = mass;
}

bool BuildBody(
    const BuildFrame& frame,
    uint32 bodyIndex,
    int32 parentBody,
    JPH::RagdollSettings& settings,
    BuiltBody& outBody)
{
    const RagdollResolvedBody& resolved = frame.bodies[bodyIndex];
    const RagdollBodyDefinition& definition = *resolved.definition;

    const Mat4f boneWorld = frame.GetBoneWorld(resolved.slot);
    const JPH::Mat44 rotation = JPH::Mat44::sRotation(ToJolt(boneWorld.ExtractRotation()).Normalized());

    outBody.orientation = rotation.GetQuaternion().Normalized();
    outBody.position = ToJolt(boneWorld.ExtractTranslation());

    const JPH::Vec3 endPosition = frame.GetBodyEnd(resolved);
    const JPH::Vec3 delta = endPosition - outBody.position;
    const float span = delta.Length();

    if (span < 1e-4f)
    {
        HYP_LOG(Physics, Warning, "Ragdoll: bone '{}' has no length, not creating a ragdoll", frame.bindData.boneNames[resolved.slot]);

        return false;
    }

    const JPH::Vec3 axisWorld = delta / span;

    outBody.length = span + definition.extraLength * frame.scale;
    outBody.radius = definition.radius * frame.scale;
    outBody.axisLocal = rotation.Multiply3x3Transposed(axisWorld);

    if (definition.foot && definition.footWidth > 0.0f)
    {
        outBody.shape = BuildFootBox(frame, definition, rotation, outBody.position, endPosition, axisWorld, outBody.radius);
    }
    else
    {
        outBody.shape = BuildCapsule(outBody.axisLocal, outBody.length, outBody.radius, frame.scale);
    }

    JPH::RagdollSettings::Part& part = settings.mParts[bodyIndex];
    ConfigurePart(part, outBody, definition, resolved.massFraction * frame.totalMass);

    if (parentBody >= 0)
    {
        part.mToParent = BuildJoint(frame, definition, frame.bodies[parentBody], outBody.position, axisWorld, outBody);
    }

    return true;
}

} // namespace

bool Ragdoll::Create(
    PhysicsWorldBase& physicsWorld,
    RagdollContacts& contacts,
    const SkeletonBindData& bindData,
    Span<const RagdollResolvedBody> bodies,
    float totalMass,
    Span<const Mat4f> modelMatrices,
    const Mat4f& entityWorldMatrix)
{
    Destroy();

    PhysicsWorld& joltWorld = static_cast<PhysicsWorld&>(physicsWorld);
    JPH::PhysicsSystem* physicsSystem = joltWorld.GetAdapter().GetPhysicsSystem();

    if (physicsSystem == nullptr || bodies.Size() < 2 || modelMatrices.Size() != bindData.NumSlots())
    {
        return false;
    }

    m_bodyOfSlot.Resize(bindData.NumSlots());

    for (int32& bodyIndex : m_bodyOfSlot)
    {
        bodyIndex = -1;
    }

    for (uint32 i = 0; i < uint32(bodies.Size()); i++)
    {
        m_bodyOfSlot[bodies[i].slot] = int32(i);
    }

    const Vec3f entityScale = entityWorldMatrix.ExtractScale();
    const float scale = (MathUtil::Abs(entityScale.x) + MathUtil::Abs(entityScale.y) + MathUtil::Abs(entityScale.z)) / 3.0f;
    const GravityFrame gravityFrame = GravityFrame::FromGravity(physicsSystem->GetGravity());

    BuildFrame frame {
        .bindData = bindData,
        .modelMatrices = modelMatrices,
        .entityWorldMatrix = entityWorldMatrix,
        .bodies = bodies,
        .scale = scale,
        .totalMass = totalMass * scale * scale * scale,
        .up = gravityFrame.up
    };
    frame.forward = GetRestForward(frame);

    JPH::Ref<JPH::Skeleton> joltSkeleton = new JPH::Skeleton;
    JPH::Ref<JPH::RagdollSettings> settings = new JPH::RagdollSettings;

    settings->mSkeleton = joltSkeleton;
    settings->mParts.resize(bodies.Size());

    JPH::Array<JPH::Mat44> jointMatrices;
    jointMatrices.reserve(bodies.Size());

    m_bodies.Reserve(bodies.Size());

    for (uint32 i = 0; i < uint32(bodies.Size()); i++)
    {
        const RagdollResolvedBody& resolved = bodies[i];
        const int32 parentBody = FindParentBody(bindData, m_bodyOfSlot.ToSpan(), resolved.slot);

        if (parentBody >= int32(i))
        {
            HYP_LOG(Physics, Warning, "Ragdoll: body '{}' is listed before its parent body, not creating a ragdoll", bindData.boneNames[resolved.slot]);

            Destroy();

            return false;
        }

        if (parentBody < 0 && i != 0)
        {
            HYP_LOG(Physics, Warning, "Ragdoll: bone '{}' has no ragdoll ancestor, not creating a ragdoll", bindData.boneNames[resolved.slot]);

            Destroy();

            return false;
        }

        joltSkeleton->AddJoint(std::string_view(bindData.boneNames[resolved.slot].LookupString()), parentBody);

        BuiltBody body;

        if (!BuildBody(frame, i, parentBody, *settings, body))
        {
            Destroy();

            return false;
        }

        jointMatrices.push_back(JPH::Mat44::sRotationTranslation(body.orientation, body.position));

        JPH::Vec3 parentAnchor = JPH::Vec3::sZero();
        JPH::Vec3 parentHingeLocal = JPH::Vec3::sZero();

        if (parentBody >= 0)
        {
            const JPH::RagdollSettings::Part& parent = settings->mParts[parentBody];

            parentAnchor = parent.mRotation.Conjugated() * JPH::Vec3(settings->mParts[i].mPosition - parent.mPosition);

            if (body.isHinge)
            {
                parentHingeLocal = parent.mRotation.Conjugated() * body.hinge;
            }
        }

        BodyInfo info;
        info.boneScale = modelMatrices[resolved.slot].ExtractScale();
        info.parentAnchor = parentAnchor;
        info.localAxis = body.axisLocal;
        info.localCenter = body.shape.centerLocal;
        info.boxHalfExtent = body.shape.boxHalfExtent;
        info.boxRotation = body.shape.boxRotation;
        info.hingeLocal = body.orientation.Conjugated() * body.hinge;
        info.parentHingeLocal = parentHingeLocal;
        info.restPosition = body.position;
        info.restOrientation = body.orientation;
        info.parentBody = parentBody;
        info.part = resolved.definition->part;
        info.muscle = resolved.definition->muscle;
        info.length = body.length;
        info.radius = body.radius;
        info.isBox = body.shape.isBox;
        info.mass = resolved.massFraction * frame.totalMass;
        info.slot = resolved.slot;
        info.isHinge = body.isHinge;
        info.isAnchor = resolved.definition->anchor;
        info.isFoot = resolved.definition->foot;

        m_bodies.PushBack(info);
    }

    for (const BodyInfo& body : m_bodies)
    {
        m_totalMass += body.mass;
    }

    m_physicsWorld = &physicsWorld;
    m_physicsSystem = physicsSystem;
    m_contacts = &contacts;

    // Stabilize evens out masses along each chain so the solver converges; the mass table goes back afterwards with the
    // inertia scaled to match
    if (!settings->Stabilize())
    {
        HYP_LOG(Physics, Warning, "Ragdoll: failed to stabilize the constraints");
    }

    RestoreTableMasses(*settings);

    settings->DisableParentChildCollisions(jointMatrices.data(), SelfCollisionMargin * scale);

    if (!AddToPhysicsSystem(*settings))
    {
        Destroy();

        return false;
    }

    CreateRootAssist(JPH::Vec3(settings->mParts[0].mPosition));

    m_hasMotors = m_ragdoll->GetConstraintCount() + 1 == size_t(GetBodyCount());

    InitBodyArrays();
    BuildRig(gravityFrame);

    if (!m_balance.Init(*this))
    {
        HYP_LOG(Physics, Info, "Ragdoll: no two-legged support found, balance is unavailable");
    }

    m_worldInverse = entityWorldMatrix.Inverse();

    HYP_LOG(Physics, Info, "Ragdoll: {} bodies, {} kg", GetBodyCount(), frame.totalMass);

    return true;
}

void Ragdoll::RestoreTableMasses(JPH::RagdollSettings& settings)
{
    for (uint32 i = 0; i < GetBodyCount(); i++)
    {
        JPH::MassProperties& massProperties = settings.mParts[i].mMassPropertiesOverride;
        const float tableMass = m_bodies[i].mass;

        if (massProperties.mMass > 0.0f)
        {
            massProperties.mInertia *= tableMass / massProperties.mMass;
            massProperties.mInertia.SetColumn4(3, JPH::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
        }

        massProperties.mMass = tableMass;

        const JPH::Mat44& inertia = massProperties.mInertia;
        m_bodies[i].inertia = (inertia(0, 0) + inertia(1, 1) + inertia(2, 2)) / 3.0f;
    }
}

bool Ragdoll::AddToPhysicsSystem(JPH::RagdollSettings& settings)
{
    static std::atomic<uint32> s_nextGroupId { 0 };

    const uint32 serial = ++s_nextGroupId;
    JPH::Ragdoll* ragdoll = settings.CreateRagdoll(serial, RagdollUserData::Tag | serial, m_physicsSystem);

    if (ragdoll == nullptr)
    {
        HYP_LOG(Physics, Warning, "Ragdoll: the physics system is out of bodies");

        return false;
    }

    m_ragdoll = ragdoll;
    m_serial = serial;
    m_ragdoll->AddToPhysicsSystem(JPH::EActivation::DontActivate);

    m_physicsSystem->AddStepListener(this);

    return true;
}

// a world space SixDOF from the world to the pelvis with every axis free; ApplyRootAssist drives its motors towards the
// animated root, holding the body up softly without pinning it
void Ragdoll::CreateRootAssist(JPH::Vec3Arg rootPosition)
{
    JPH::SixDOFConstraintSettings rootSettings;
    rootSettings.mSpace = JPH::EConstraintSpace::WorldSpace;
    rootSettings.mNumVelocityStepsOverride = SolverVelocitySteps;
    rootSettings.mPosition1 = rootSettings.mPosition2 = JPH::RVec3(rootPosition);

    for (uint32 axis = 0; axis < JPH::SixDOFConstraint::EAxis::Num; axis++)
    {
        rootSettings.MakeFreeAxis(JPH::SixDOFConstraint::EAxis(axis));
    }

    {
        JPH::BodyLockWrite lock(m_physicsSystem->GetBodyLockInterface(), m_ragdoll->GetBodyID(0));

        if (lock.Succeeded())
        {
            m_rootAssistConstraint = static_cast<JPH::SixDOFConstraint*>(rootSettings.Create(JPH::Body::sFixedToWorld, lock.GetBody()));
        }
    }

    if (m_rootAssistConstraint != nullptr)
    {
        m_rootAssistConstraint->SetEnabled(false);
        m_physicsSystem->AddConstraint(m_rootAssistConstraint);
        m_rootAnchor = rootPosition;
    }
}

void Ragdoll::InitBodyArrays()
{
    const size_t bodyCount = m_bodies.Size();

    m_targets.Resize(bodyCount);
    m_states.Resize(bodyCount);
    m_motorOverrides.Resize(bodyCount);
    m_overrideTargets.Resize(bodyCount);
    m_hadOverride.Resize(bodyCount);
    m_layers.Resize(bodyCount);

    for (size_t i = 0; i < bodyCount; i++)
    {
        m_overrideTargets[i] = JPH::Quat::sIdentity();
        m_hadOverride[i] = 0;
    }

    m_hasTargets = false;
}

int32 Ragdoll::FindChildBody(uint32 parent, RagdollBodyPart part) const
{
    for (uint32 i = parent + 1; i < GetBodyCount(); i++)
    {
        if (m_bodies[i].parentBody == int32(parent) && m_bodies[i].part == part)
        {
            return int32(i);
        }
    }

    return -1;
}

bool Ragdoll::FindLimbChain(uint32 root, uint32& outMiddle, uint32& outEnd) const
{
    const RagdollBodyPart part = m_bodies[root].part;
    const int32 middle = FindChildBody(root, part);

    if (middle < 0)
    {
        return false;
    }

    const int32 end = FindChildBody(uint32(middle), part);

    if (end < 0)
    {
        return false;
    }

    outMiddle = uint32(middle);
    outEnd = uint32(end);

    return true;
}

void Ragdoll::BuildRig(const GravityFrame& frame)
{
    m_rig = RagdollRig {};

    if (GetBodyCount() < 2)
    {
        return;
    }

    uint32 armCount = 0;
    uint32 legCount = 0;

    FindRigLimbs(armCount, legCount);

    const bool hasFeet = legCount == RagdollRig::LimbCount && InitRigFeet(frame);
    const bool hasChest = m_rig.neck != 0 && m_bodies[m_rig.neck].parentBody >= 0;

    if (hasChest)
    {
        m_rig.chest = uint32(m_bodies[m_rig.neck].parentBody);

        for (int32 walk = int32(m_rig.chest); walk > 0 && m_rig.spineCount < RagdollRig::MaxSpine; walk = m_bodies[walk].parentBody)
        {
            m_rig.spine[m_rig.spineCount++] = uint32(walk);
        }
    }

    InitRigSides(frame, armCount, legCount);

    const bool hasLimbs = armCount == RagdollRig::LimbCount && legCount == RagdollRig::LimbCount;
    const bool legsSided = m_rig.legs[0].sideSign != m_rig.legs[1].sideSign;
    const bool armsSided = m_rig.arms[0].sideSign != m_rig.arms[1].sideSign;

    m_rig.canBalance = hasFeet && hasChest && legsSided;
    m_rig.isValid = hasLimbs && hasChest && m_rig.chest != 0 && legsSided && armsSided;

    if (!m_rig.isValid)
    {
        HYP_LOG(Physics, Warning, "Ragdoll: expected two arms, two legs and a head on a spine ({} arms, {} legs)", armCount, legCount);
    }
}

void Ragdoll::FindRigLimbs(uint32& outArmCount, uint32& outLegCount)
{
    for (uint32 i = 1; i < GetBodyCount(); i++)
    {
        const BodyInfo& body = m_bodies[i];

        if (body.part == RagdollBodyPart::Head && m_rig.neck == 0)
        {
            m_rig.neck = i;
        }

        const bool isLimb = body.part == RagdollBodyPart::Arm || body.part == RagdollBodyPart::Leg;

        // a limb starts at the first body of its part below a body of another part
        if (!isLimb || m_bodies[body.parentBody].part == body.part)
        {
            continue;
        }

        uint32 middle = 0;
        uint32 end = 0;

        if (!FindLimbChain(i, middle, end))
        {
            continue;
        }

        if (body.part == RagdollBodyPart::Arm && outArmCount < RagdollRig::LimbCount)
        {
            RagdollRigArm& arm = m_rig.arms[outArmCount++];

            arm.clavicle = uint32(body.parentBody);
            arm.upper = i;
            arm.lower = middle;
            arm.hand = end;
            arm.upperLength = (m_bodies[middle].restPosition - body.restPosition).Length();
            arm.lowerLength = (m_bodies[end].restPosition - m_bodies[middle].restPosition).Length();
        }
        else if (body.part == RagdollBodyPart::Leg && outLegCount < RagdollRig::LimbCount)
        {
            RagdollRigLeg& leg = m_rig.legs[outLegCount++];

            leg.thigh = i;
            leg.shin = middle;
            leg.foot = end;
        }
    }
}

bool Ragdoll::InitRigFeet(const GravityFrame& frame)
{
    JPH::Vec3 forward = JPH::Vec3::sZero();

    for (RagdollRigLeg& leg : m_rig.legs)
    {
        const BodyInfo& thigh = m_bodies[leg.thigh];
        const BodyInfo& shin = m_bodies[leg.shin];
        const BodyInfo& foot = m_bodies[leg.foot];

        if (!foot.isFoot || !foot.isBox || !shin.isHinge || thigh.parentBody != 0)
        {
            return false;
        }

        leg.upper = (shin.restPosition - thigh.restPosition).Length();
        leg.lower = (foot.restPosition - shin.restPosition).Length();
        leg.hipOffset = thigh.parentAnchor;
        leg.restFoot = foot.restOrientation;

        const JPH::Vec3 half = foot.boxHalfExtent;

        for (uint32 corner = 0; corner < SoleCorners; corner++)
        {
            leg.soleCorners[corner] = foot.localCenter + foot.boxRotation * JPH::Vec3(SoleCornerSigns[corner][0] * half.GetX(), -half.GetY(), SoleCornerSigns[corner][1] * half.GetZ());
        }

        leg.soleCenter = foot.localCenter + foot.boxRotation * JPH::Vec3(0.0f, -half.GetY(), 0.0f);

        m_rig.footHalfWidth = half.GetX();
        m_rig.footHalfLength = half.GetZ();

        forward += foot.restOrientation * foot.boxRotation * JPH::Vec3::sAxisZ();
    }

    m_rig.restForward = frame.Horizontal(forward).NormalizedOr(JPH::Vec3::sAxisZ());

    return true;
}

void Ragdoll::InitRigSides(const GravityFrame& frame, uint32 armCount, uint32 legCount)
{
    const JPH::Vec3 side = frame.up.Cross(m_rig.restForward);
    const JPH::Vec3 pelvis = m_bodies[0].restPosition;

    for (uint32 i = 0; i < armCount; i++)
    {
        RagdollRigArm& arm = m_rig.arms[i];
        arm.sideSign = GetSign((m_bodies[arm.upper].restPosition - pelvis).Dot(side));
    }

    for (uint32 i = 0; i < legCount; i++)
    {
        RagdollRigLeg& leg = m_rig.legs[i];

        const BodyInfo& thigh = m_bodies[leg.thigh];
        const BodyInfo& shin = m_bodies[leg.shin];
        const BodyInfo& foot = m_bodies[leg.foot];

        const JPH::Vec3 legDirection = (foot.restPosition - thigh.restPosition).Normalized();
        const JPH::Vec3 restNormal = legDirection.Cross(m_rig.restForward);
        const JPH::Vec3 restHinge = shin.restOrientation * shin.hingeLocal;

        leg.sideSign = GetSign((thigh.restPosition - pelvis).Dot(side));
        leg.hingeSign = GetSign(restHinge.Dot(restNormal));
    }
}

} // namespace Hyperion
