/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <ActiveRagdoll/Ragdoll.hpp>

#include <Physics/PhysicsWorld.hpp>

#include <Scene/Animation/Skeleton.hpp>

#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Constraints/SwingTwistConstraint.h>

#include <limits>

#include <Ragdoll.generated.inl>

namespace Hyperion {

using namespace RagdollMath;

static void GetBoneWorld(const Mat4f& entityWorldMatrix, const Mat4f& modelMatrix, JPH::Vec3& outPosition, JPH::Quat& outOrientation)
{
    const Mat4f world = entityWorldMatrix * modelMatrix;

    outPosition = ToJolt(world.ExtractTranslation());
    outOrientation = ToJolt(world.ExtractRotation()).Normalized();
}

Ragdoll::~Ragdoll()
{
    Destroy();
}

JPH::BodyInterface& Ragdoll::GetBodyInterface() const
{
    return m_physicsSystem->GetBodyInterface();
}

JPH::BodyID Ragdoll::GetBodyID(uint32 bodyIndex) const
{
    return m_ragdoll->GetBodyID(int(bodyIndex));
}

bool Ragdoll::IsAwake() const
{
    return m_ragdoll != nullptr && m_ragdoll->IsActive();
}

GravityFrame Ragdoll::GetGravityFrame() const
{
    if (m_physicsSystem == nullptr)
    {
        return GravityFrame {};
    }

    return GravityFrame::FromGravity(m_physicsSystem->GetGravity());
}

void Ragdoll::Activate(JPH::Vec3Arg velocity)
{
    if (m_ragdoll == nullptr)
    {
        return;
    }

    m_ragdoll->SetLinearVelocity(velocity);
    m_ragdoll->Activate();

    m_isLimp = true;
}

void Ragdoll::Drive(Span<const Mat4f> modelMatrices, const Mat4f& entityWorldMatrix, float deltaTime, bool teleport)
{
    if (m_ragdoll == nullptr || m_isLimp)
    {
        return;
    }

    m_worldInverse = entityWorldMatrix.Inverse();

    JPH::BodyInterface& bodyInterface = GetBodyInterface();

    for (uint32 i = 0; i < GetBodyCount(); i++)
    {
        BodyInfo& info = m_bodies[i];
        BodyTarget& target = m_targets[i];

        if (info.slot >= modelMatrices.Size())
        {
            continue;
        }

        JPH::Vec3 position;
        JPH::Quat orientation;
        GetBoneWorld(entityWorldMatrix, modelMatrices[info.slot], position, orientation);

        if (teleport || !m_hasTargets || deltaTime <= 0.0f)
        {
            target.linearVelocity = JPH::Vec3::sZero();
            target.angularVelocity = JPH::Vec3::sZero();
        }
        else
        {
            target.linearVelocity = (position - target.position) / deltaTime;
            target.angularVelocity = GetRotationVector(orientation * target.orientation.Conjugated()) / deltaTime;
        }

        target.orientation = orientation;
        target.position = position;

        if (info.mode == RagdollBodyMode::Kinematic)
        {
            if (teleport || info.needsSnap)
            {
                SnapBodyToTarget(bodyInterface, i, JPH::EActivation::DontActivate);
                info.needsSnap = false;
            }
        }
        else if (info.mode == RagdollBodyMode::Powered && teleport)
        {
            SnapBodyToTarget(bodyInterface, i, JPH::EActivation::Activate);
        }
    }

    m_hasTargets = true;
}

void Ragdoll::SnapBodyToTarget(JPH::BodyInterface& bodyInterface, uint32 bodyIndex, JPH::EActivation activation)
{
    const BodyTarget& target = m_targets[bodyIndex];
    const JPH::BodyID bodyId = GetBodyID(bodyIndex);

    bodyInterface.SetPositionAndRotation(bodyId, JPH::RVec3(target.position), target.orientation, activation);
    bodyInterface.SetLinearAndAngularVelocity(bodyId, JPH::Vec3::sZero(), JPH::Vec3::sZero());
}

void Ragdoll::GoLimp(const Mat4f& entityWorldMatrix)
{
    if (m_ragdoll == nullptr || m_isLimp)
    {
        return;
    }

    m_worldInverse = entityWorldMatrix.Inverse();

    SetAllBodyModes(RagdollBodyMode::Limp);
    SetAllBodyBlends(1.0f);

    m_ragdoll->Activate();

    m_isLimp = true;
}

void Ragdoll::SetBodyMode(uint32 bodyIndex, RagdollBodyMode mode)
{
    if (m_ragdoll == nullptr || bodyIndex >= GetBodyCount() || m_bodies[bodyIndex].mode == mode)
    {
        return;
    }

    BodyInfo& info = m_bodies[bodyIndex];
    const RagdollBodyMode previousMode = info.mode;
    info.mode = mode;

    JPH::BodyInterface& bodyInterface = GetBodyInterface();
    const JPH::BodyID bodyId = GetBodyID(bodyIndex);

    if (mode == RagdollBodyMode::Kinematic)
    {
        bodyInterface.SetMotionType(bodyId, JPH::EMotionType::Kinematic, JPH::EActivation::DontActivate);
        info.needsSnap = true;

        return;
    }

    bodyInterface.SetMotionType(bodyId, JPH::EMotionType::Dynamic, JPH::EActivation::Activate);
    bodyInterface.SetGravityFactor(bodyId, 1.0f);
    bodyInterface.ActivateBody(bodyId);

    // keep the animation's velocity (taken at the centre of mass) so the body doesn't stop dead
    if (previousMode == RagdollBodyMode::Kinematic)
    {
        const BodyTarget& target = m_targets[bodyIndex];
        const JPH::Vec3 offset = JPH::Vec3(bodyInterface.GetCenterOfMassPosition(bodyId) - bodyInterface.GetPosition(bodyId));

        bodyInterface.SetLinearAndAngularVelocity(bodyId, target.linearVelocity + target.angularVelocity.Cross(offset), target.angularVelocity);
    }
}

void Ragdoll::SetAllBodyModes(RagdollBodyMode mode)
{
    bool wasKinematic = false;

    for (uint32 i = 0; i < GetBodyCount(); i++)
    {
        if (m_bodies[i].mode == RagdollBodyMode::Kinematic)
        {
            wasKinematic = true;
        }

        SetBodyMode(i, mode);
    }

    if (wasKinematic && mode != RagdollBodyMode::Kinematic)
    {
        PlaceFeetOnGround();
    }
}

void Ragdoll::PlaceFeetOnGround()
{
    static constexpr float ProbeUp = 0.3f;
    static constexpr float ProbeDown = 1.0f;

    if (m_ragdoll == nullptr || !m_rig.canBalance)
    {
        return;
    }

    JPH::BodyInterface& bodyInterface = GetBodyInterface();
    const JPH::Vec3 up = GetGravityFrame().up;

    // animated feet sink into the floor (box soles tip their corners under it as the foot rolls), and left there the
    // depenetration kicks the body the moment it turns dynamic
    for (const RagdollRigLeg& leg : m_rig.legs)
    {
        const JPH::BodyID bodyId = GetBodyID(leg.foot);
        const JPH::Vec3 ankle = JPH::Vec3(bodyInterface.GetPosition(bodyId));
        JPH::Quat orientation = bodyInterface.GetRotation(bodyId);

        JPH::Vec3 groundPoint;
        JPH::Vec3 groundNormal;

        if (!CastRayStatic(ankle + up * ProbeUp, -up * (ProbeUp + ProbeDown), groundPoint, groundNormal))
        {
            continue;
        }

        const float groundHeight = groundPoint.Dot(up);

        const auto getLowest = [&](JPH::QuatArg rotation)
        {
            float lowest = std::numeric_limits<float>::max();

            for (const JPH::Vec3& corner : leg.soleCorners)
            {
                lowest = MathUtil::Min(lowest, (ankle + rotation * corner).Dot(up));
            }

            return lowest - groundHeight;
        };

        if (getLowest(orientation) >= 0.0f)
        {
            continue;
        }

        // tip the sole flat about the ankle, then lift whatever is still under the floor
        const JPH::Vec3 soleUp = orientation * (m_bodies[leg.foot].restOrientation.Conjugated() * up);
        orientation = (JPH::Quat::sFromTo(soleUp, up) * orientation).Normalized();

        const float depth = MathUtil::Min(getLowest(orientation), 0.0f);
        const JPH::Vec3 position = ankle - up * depth;

        bodyInterface.SetPositionAndRotation(bodyId, JPH::RVec3(position), orientation, JPH::EActivation::Activate);

        JPH::Vec3 velocity = bodyInterface.GetLinearVelocity(bodyId);

        if (velocity.Dot(up) < 0.0f)
        {
            velocity -= up * velocity.Dot(up);
        }

        bodyInterface.SetLinearAndAngularVelocity(bodyId, velocity, JPH::Vec3::sZero());
    }
}

void Ragdoll::SetBodyStrength(uint32 bodyIndex, float strength)
{
    if (m_ragdoll == nullptr || bodyIndex >= GetBodyCount())
    {
        return;
    }

    m_bodies[bodyIndex].strength = MathUtil::Clamp(strength, 0.0f, 1.0f);
}

void Ragdoll::SetBodyBlend(uint32 bodyIndex, float blend)
{
    if (bodyIndex < GetBodyCount())
    {
        m_bodies[bodyIndex].blend = MathUtil::Clamp(blend, 0.0f, 1.0f);
    }
}

void Ragdoll::SetAllBodyBlends(float blend)
{
    for (uint32 i = 0; i < GetBodyCount(); i++)
    {
        SetBodyBlend(i, blend);
    }
}

void Ragdoll::SetRootAssist(float assist)
{
    m_rootAssist = MathUtil::Clamp(assist, 0.0f, 1.0f);
}

void Ragdoll::SetBalancing(bool balancing)
{
    if (m_ragdoll == nullptr || !m_balance.IsValid())
    {
        balancing = false;
    }

    if (balancing == m_isBalancing)
    {
        return;
    }

    m_isBalancing = balancing;

    if (balancing)
    {
        m_balance.Reset();
        m_overrideFade = 0.0f;
    }
    else
    {
        m_balance.Clear(*this);
        m_overrideFade = 1.0f;
    }
}

void Ragdoll::RebaseTargets()
{
    m_hasTargets = false;
}

void Ragdoll::ClearBodyLayers()
{
    for (BodyLayer& layer : m_layers)
    {
        layer = BodyLayer {};
    }
}

void Ragdoll::SetBodyLayers(Span<const BodyLayer> layers, float gravityScale)
{
    for (uint32 i = 0; i < uint32(m_layers.Size()) && i < uint32(layers.Size()); i++)
    {
        m_layers[i] = layers[i];
        m_layers[i].gravityScale = gravityScale;
    }
}

void Ragdoll::AddImpulse(JPH::BodyID bodyId, JPH::Vec3Arg impulse, JPH::Vec3Arg point)
{
    const int32 bodyIndex = FindBodyIndex(bodyId);

    if (bodyIndex < 0 || m_bodies[bodyIndex].mode == RagdollBodyMode::Kinematic)
    {
        return;
    }

    JPH::BodyInterface& bodyInterface = GetBodyInterface();

    bodyInterface.ActivateBody(bodyId);
    bodyInterface.AddImpulse(bodyId, impulse, JPH::RVec3(point));
}

int32 Ragdoll::FindBodyIndex(JPH::BodyID bodyId) const
{
    if (m_ragdoll == nullptr)
    {
        return -1;
    }

    for (uint32 i = 0; i < GetBodyCount(); i++)
    {
        if (GetBodyID(i) == bodyId)
        {
            return int32(i);
        }
    }

    return -1;
}

float Ragdoll::GetBodyMass(uint32 bodyIndex) const
{
    if (bodyIndex >= GetBodyCount())
    {
        return 0.0f;
    }

    return m_bodies[bodyIndex].mass;
}

RagdollBodyPart Ragdoll::GetBodyPart(uint32 bodyIndex) const
{
    if (bodyIndex >= GetBodyCount())
    {
        return RagdollBodyPart::Torso;
    }

    return m_bodies[bodyIndex].part;
}

JPH::Vec3 Ragdoll::GetBodyOrigin(uint32 bodyIndex) const
{
    return JPH::Vec3(GetBodyInterface().GetPosition(GetBodyID(bodyIndex)));
}

JPH::Vec3 Ragdoll::GetBodyPosition(uint32 bodyIndex) const
{
    return JPH::Vec3(GetBodyInterface().GetCenterOfMassPosition(GetBodyID(bodyIndex)));
}

JPH::Vec3 Ragdoll::GetBodyTargetOrigin(uint32 bodyIndex) const
{
    if (bodyIndex >= m_targets.Size())
    {
        return JPH::Vec3::sZero();
    }

    return m_targets[bodyIndex].position;
}

JPH::Vec3 Ragdoll::GetPelvisHeading(bool animated) const
{
    if (m_bodies.Empty())
    {
        return JPH::Vec3::sZero();
    }

    JPH::Quat orientation = GetBodyInterface().GetRotation(GetBodyID(0));

    if (animated)
    {
        orientation = m_targets[0].orientation;
    }

    const GravityFrame frame = GetGravityFrame();
    const JPH::Vec3 forward = (orientation * m_bodies[0].restOrientation.Conjugated()) * m_rig.restForward;

    return frame.Horizontal(forward).NormalizedOr(JPH::Vec3::sZero());
}

void Ragdoll::SampleBody(JPH::BodyInterface& bodyInterface, uint32 bodyIndex, BodyState& outState) const
{
    const JPH::BodyID bodyId = GetBodyID(bodyIndex);

    JPH::RVec3 position;
    bodyInterface.GetPositionAndRotation(bodyId, position, outState.orientation);

    outState.origin = JPH::Vec3(position);
    outState.center = JPH::Vec3(bodyInterface.GetCenterOfMassPosition(bodyId));
    outState.velocity = bodyInterface.GetLinearVelocity(bodyId);
    outState.angularVelocity = bodyInterface.GetAngularVelocity(bodyId);
}

bool Ragdoll::GetMassAverage(Span<const BodyState> states, JPH::Vec3& outCenter, JPH::Vec3& outVelocity) const
{
    if (m_totalMass <= 0.0f)
    {
        return false;
    }

    JPH::Vec3 moment = JPH::Vec3::sZero();
    JPH::Vec3 momentum = JPH::Vec3::sZero();

    for (uint32 i = 0; i < GetBodyCount(); i++)
    {
        moment += states[i].center * m_bodies[i].mass;
        momentum += states[i].velocity * m_bodies[i].mass;
    }

    outCenter = moment / m_totalMass;
    outVelocity = momentum / m_totalMass;

    return true;
}

bool Ragdoll::GetBodyGroundContact(uint32 bodyIndex, RagdollGroundContact& outContact) const
{
    if (m_ragdoll == nullptr || m_contacts == nullptr || bodyIndex >= GetBodyCount())
    {
        outContact = RagdollGroundContact {};

        return false;
    }

    return m_contacts->GetGroundContact(GetBodyID(bodyIndex), outContact);
}

bool Ragdoll::IsSupportNormal(const RagdollGroundContact& contact, JPH::Vec3Arg up, float minUp)
{
    return contact.normal.Dot(up) >= minUp;
}

bool Ragdoll::IsBodyOnGround(uint32 bodyIndex, JPH::Vec3Arg up, float minUp) const
{
    RagdollGroundContact contact;

    return GetBodyGroundContact(bodyIndex, contact) && IsSupportNormal(contact, up, minUp);
}

bool Ragdoll::CastRayStatic(JPH::Vec3Arg origin, JPH::Vec3Arg vector, JPH::Vec3& outPoint, JPH::Vec3& outNormal) const
{
    const float length = vector.Length();

    if (m_physicsWorld == nullptr || length <= 1e-6f)
    {
        return false;
    }

    PhysicsRayHit hit;

    if (!m_physicsWorld->CastRayStatic(FromJolt(origin), FromJolt(vector / length), length, hit))
    {
        return false;
    }

    outPoint = ToJolt(hit.position);
    outNormal = ToJolt(hit.normal);

    return true;
}

void Ragdoll::GetBodyWorld(uint32 bodyIndex, JPH::Vec3& outPosition, JPH::Quat& outOrientation) const
{
    JPH::RVec3 position;
    GetBodyInterface().GetPositionAndRotation(GetBodyID(bodyIndex), position, outOrientation);

    outPosition = JPH::Vec3(position);
}

bool Ragdoll::WritePose(Span<const Transform> animatedPose, const SkeletonBindData& bindData, const Mat4f& entityWorldMatrix, Array<Transform>& outPose)
{
    const uint32 numSlots = bindData.NumSlots();

    if (m_ragdoll == nullptr || animatedPose.Size() != numSlots || m_bodyOfSlot.Size() != numSlots)
    {
        return false;
    }

    bool anyDriven = false;

    for (const BodyInfo& info : m_bodies)
    {
        if (m_isLimp || (info.mode != RagdollBodyMode::Kinematic && info.blend > 0.0f))
        {
            anyDriven = true;
            break;
        }
    }

    if (!anyDriven)
    {
        return false;
    }

    m_worldInverse = entityWorldMatrix.Inverse();

    outPose.Resize(numSlots);
    m_poseModelMatrices.Resize(numSlots);

    for (uint32 slot = 0; slot < numSlots; slot++)
    {
        const uint32 parentSlot = bindData.parentSlots[slot];

        Mat4f parentModel;

        if (parentSlot != SkeletonBindData::InvalidSlot)
        {
            parentModel = m_poseModelMatrices[parentSlot];
        }

        const int32 bodyIndex = m_bodyOfSlot[slot];
        bool isDriven = false;

        if (bodyIndex >= 0)
        {
            const BodyInfo& info = m_bodies[bodyIndex];

            isDriven = m_isLimp || (info.mode != RagdollBodyMode::Kinematic && info.blend > 0.0f);
        }

        if (!isDriven)
        {
            outPose[slot] = animatedPose[slot];
            m_poseModelMatrices[slot] = parentModel * animatedPose[slot].GetMatrix();

            continue;
        }

        const BodyInfo& info = m_bodies[bodyIndex];

        JPH::Vec3 position;
        JPH::Quat orientation;
        GetBodyWorld(uint32(bodyIndex), position, orientation);

        const Mat4f model = m_worldInverse * Mat4f::Translation(FromJolt(position)) * Mat4f::Rotation(FromJolt(orientation));

        Vec3f modelPosition = model.ExtractTranslation();
        Quat4f modelRotation = model.ExtractRotation();

        if (!m_isLimp && info.blend < 1.0f)
        {
            const Mat4f animatedModel = parentModel * animatedPose[slot].GetMatrix();

            const JPH::Quat from = ToJolt(animatedModel.ExtractRotation()).Normalized();
            const JPH::Vec3 fromPosition = ToJolt(animatedModel.ExtractTranslation());

            modelRotation = FromJolt(from.SLERP(ToJolt(modelRotation).Normalized(), info.blend).Normalized());
            modelPosition = FromJolt(fromPosition + (ToJolt(modelPosition) - fromPosition) * info.blend);
        }

        const Mat4f drivenModel = Mat4f::Translation(modelPosition) * Mat4f::Rotation(modelRotation) * Mat4f::Scaling(info.boneScale);
        const Mat4f local = parentModel.Inverse() * drivenModel;

        outPose[slot] = Transform(local.ExtractTranslation(), local.ExtractScale(), local.ExtractRotation());
        m_poseModelMatrices[slot] = parentModel * outPose[slot].GetMatrix();
    }

    return true;
}

void Ragdoll::Destroy()
{
    if (m_physicsSystem != nullptr && m_rootAssistConstraint != nullptr)
    {
        m_physicsSystem->RemoveConstraint(m_rootAssistConstraint);
    }

    m_rootAssistConstraint = nullptr;

    if (m_physicsSystem != nullptr && m_ragdoll != nullptr)
    {
        m_physicsSystem->RemoveStepListener(this);
    }

    if (m_ragdoll != nullptr)
    {
        m_ragdoll->RemoveFromPhysicsSystem();
        m_ragdoll = nullptr;
    }

    m_physicsSystem = nullptr;
    m_physicsWorld = nullptr;
    m_contacts = nullptr;

    m_bodies.Clear();
    m_bodyOfSlot.Clear();
    m_targets.Clear();
    m_states.Clear();
    m_motorOverrides.Clear();
    m_overrideTargets.Clear();
    m_hadOverride.Clear();
    m_layers.Clear();
    m_poseModelMatrices.Clear();
    m_totalMass = 0.0f;
    m_hasTargets = false;
    m_hasMotors = false;
    m_isLimp = false;
    m_rig = RagdollRig {};
    m_balance = RagdollBalance {};
    m_isBalancing = false;
}

} // namespace Hyperion
