/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <ActiveRagdoll/RagdollBalance.hpp>
#include <ActiveRagdoll/RagdollContacts.hpp>
#include <ActiveRagdoll/RagdollDefinition.hpp>
#include <ActiveRagdoll/RagdollMath.hpp>
#include <ActiveRagdoll/RagdollRig.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Math/Transform.hpp>
#include <Core/Utilities/Span.hpp>

#include <Jolt/Jolt.h>
#include <Jolt/Core/Reference.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/EActivation.h>
#include <Jolt/Physics/PhysicsStepListener.h>
#include <Jolt/Physics/Ragdoll/Ragdoll.h>
#include <Jolt/Physics/Constraints/SixDOFConstraint.h>

namespace JPH {
class BodyInterface;
class PhysicsSystem;
class SwingTwistConstraint;
} // namespace JPH

namespace Hyperion {

class PhysicsWorldBase;
class DebugDrawCommandList;
struct SkeletonBindData;

HYP_ENUM()
enum class RagdollBodyMode : uint8
{
    Kinematic = 0,
    Powered,
    Limp
};

class Ragdoll final : public JPH::PhysicsStepListener
{
    // balance is part of the motor system and writes targets and motor overrides directly
    friend class RagdollBalance;

    enum class MotorOverrideMode : uint8
    {
        None,
        Off,
        Target
    };

    struct BodyInfo
    {
        // the bone's model space scale, stripped for Jolt and put back when posing
        Vec3f boneScale = Vec3f::One();
        JPH::Vec3 parentAnchor = JPH::Vec3::sZero();
        JPH::Vec3 localAxis = JPH::Vec3::sAxisY();
        JPH::Vec3 localCenter = JPH::Vec3::sZero();
        JPH::Vec3 boxHalfExtent = JPH::Vec3::sZero();
        JPH::Quat boxRotation = JPH::Quat::sIdentity();

        JPH::Vec3 hingeLocal = JPH::Vec3::sZero();
        JPH::Vec3 parentHingeLocal = JPH::Vec3::sZero();
        JPH::Vec3 restPosition = JPH::Vec3::sZero();
        JPH::Quat restOrientation = JPH::Quat::sIdentity();

        int32 parentBody = -1;

        RagdollBodyPart part = RagdollBodyPart::Torso;
        RagdollBodyMode mode = RagdollBodyMode::Limp;

        RagdollMuscle muscle;

        float length = 0.0f;
        float radius = 0.0f;
        bool isBox = false;

        float mass = 0.0f;
        float inertia = 0.0f;

        float strength = 1.0f;
        float blend = 1.0f;
        uint32 slot = 0;

        bool isHinge = false;
        bool isAnchor = false;
        bool isFoot = false;
        bool needsSnap = false;
    };

    struct BodyTarget
    {
        JPH::Vec3 position = JPH::Vec3::sZero();
        JPH::Quat orientation = JPH::Quat::sIdentity();
        JPH::Vec3 linearVelocity = JPH::Vec3::sZero();
        JPH::Vec3 angularVelocity = JPH::Vec3::sZero();
    };

    struct MotorOverride
    {
        MotorOverrideMode mode = MotorOverrideMode::None;
        JPH::Quat target = JPH::Quat::sIdentity();
        float frequencyScale = 1.0f;
        float inertia = 0.0f;
        bool isSupported = false;
        bool skipGravityComp = false;
    };

public:
    // one read of a body; the subtree fields are only filled by UpdateSubtrees
    struct BodyState
    {
        JPH::Vec3 origin = JPH::Vec3::sZero();
        JPH::Vec3 center = JPH::Vec3::sZero();
        JPH::Vec3 velocity = JPH::Vec3::sZero();
        JPH::Vec3 angularVelocity = JPH::Vec3::sZero();
        JPH::Quat orientation = JPH::Quat::sIdentity();
        float subtreeMass = 0.0f;
        JPH::Vec3 subtreeMoment = JPH::Vec3::sZero();
        float subtreeSecondMoment = 0.0f;
    };

    // a pose the behaviours blend over the animated target, relative to the parent body
    struct BodyLayer
    {
        JPH::Quat pose = JPH::Quat::sIdentity();
        JPH::Quat offset = JPH::Quat::sIdentity();
        float weight = 0.0f;
        float gravityScale = 1.0f;
    };

    Ragdoll() = default;
    Ragdoll(const Ragdoll& other) = delete;
    Ragdoll& operator=(const Ragdoll& other) = delete;
    ~Ragdoll() override;

    // bodies from `modelMatrices`, the animated pose of the skeleton the bodies were resolved on
    bool Create(
        PhysicsWorldBase& physicsWorld,
        RagdollContacts& contacts,
        const SkeletonBindData& bindData,
        Span<const RagdollResolvedBody> bodies,
        float totalMass,
        Span<const Mat4f> modelMatrices,
        const Mat4f& entityWorldMatrix);

    void Destroy();

    void Activate(JPH::Vec3Arg velocity);

    // targets from the animated pose
    void Drive(Span<const Mat4f> modelMatrices, const Mat4f& entityWorldMatrix, float deltaTime, bool teleport = false);
    void GoLimp(const Mat4f& entityWorldMatrix);

    void SetBodyMode(uint32 bodyIndex, RagdollBodyMode mode);
    void SetAllBodyModes(RagdollBodyMode mode);
    void PlaceFeetOnGround();
    void SetBodyStrength(uint32 bodyIndex, float strength);
    void SetBodyBlend(uint32 bodyIndex, float blend);
    void SetAllBodyBlends(float blend);
    void SetRootAssist(float assist);
    void SetBalancing(bool balancing);

    HYP_FORCE_INLINE void SetBalanceAssist(const RagdollBalanceAssist& assist)
    {
        m_balance.SetAssist(assist);
    }

    HYP_FORCE_INLINE void SetStaggerVelocity(JPH::Vec3Arg velocity)
    {
        m_balance.SetStaggerVelocity(velocity);
    }

    HYP_FORCE_INLINE JPH::Vec3 GetStaggerVelocity() const
    {
        return m_balance.GetStaggerVelocity();
    }

    void RebaseTargets();
    void ClearBodyLayers();
    void SetBodyLayers(Span<const BodyLayer> layers, float gravityScale);

    int32 FindBodyIndex(JPH::BodyID bodyId) const;
    JPH::BodyID GetBodyID(uint32 bodyIndex) const;
    float GetBodyMass(uint32 bodyIndex) const;
    JPH::Vec3 GetBodyPosition(uint32 bodyIndex) const;
    JPH::Vec3 GetBodyOrigin(uint32 bodyIndex) const;
    JPH::Vec3 GetBodyTargetOrigin(uint32 bodyIndex) const;
    // horizontal direction the pelvis faces, as simulated or as last animated
    JPH::Vec3 GetPelvisHeading(bool animated) const;
    bool GetBodyGroundContact(uint32 bodyIndex, RagdollGroundContact& outContact) const;
    bool IsBodyOnGround(uint32 bodyIndex, JPH::Vec3Arg up, float minUp) const;
    static bool IsSupportNormal(const RagdollGroundContact& contact, JPH::Vec3Arg up, float minUp);
    RagdollBodyPart GetBodyPart(uint32 bodyIndex) const;

    void SampleBody(JPH::BodyInterface& bodyInterface, uint32 bodyIndex, BodyState& outState) const;
    bool GetMassAverage(Span<const BodyState> states, JPH::Vec3& outCenter, JPH::Vec3& outVelocity) const;
    void GetHingeChainRotations(uint32 upper, uint32 lower, JPH::Vec3Arg upperDirection, JPH::Vec3Arg lowerDirection, JPH::Vec3Arg hinge, JPH::Quat& outUpper, JPH::Quat& outLower) const;

    bool CastRayStatic(JPH::Vec3Arg origin, JPH::Vec3Arg vector, JPH::Vec3& outPoint, JPH::Vec3& outNormal) const;

    HYP_FORCE_INLINE uint32 GetBodyCount() const
    {
        return uint32(m_bodies.Size());
    }

    HYP_FORCE_INLINE float GetTotalMass() const
    {
        return m_totalMass;
    }

    HYP_FORCE_INLINE int32 GetParentBody(uint32 bodyIndex) const
    {
        return m_bodies[bodyIndex].parentBody;
    }

    HYP_FORCE_INLINE bool IsAnchor(uint32 bodyIndex) const
    {
        return m_bodies[bodyIndex].isAnchor;
    }

    HYP_FORCE_INLINE bool IsFoot(uint32 bodyIndex) const
    {
        return m_bodies[bodyIndex].isFoot;
    }

    HYP_FORCE_INLINE uint32 GetBodySlot(uint32 bodyIndex) const
    {
        return m_bodies[bodyIndex].slot;
    }

    HYP_FORCE_INLINE const JPH::Quat& GetRestOrientation(uint32 bodyIndex) const
    {
        return m_bodies[bodyIndex].restOrientation;
    }

    HYP_FORCE_INLINE RagdollBodyMode GetBodyMode(uint32 bodyIndex) const
    {
        return m_bodies[bodyIndex].mode;
    }

    HYP_FORCE_INLINE float GetBodyStrength(uint32 bodyIndex) const
    {
        return m_bodies[bodyIndex].strength;
    }

    HYP_FORCE_INLINE float GetRootAssist() const
    {
        return m_rootAssist;
    }

    HYP_FORCE_INLINE bool IsBalancing() const
    {
        return m_isBalancing;
    }

    HYP_FORCE_INLINE const RagdollBalanceStatus& GetBalanceStatus() const
    {
        return m_balance.GetStatus();
    }

    HYP_FORCE_INLINE const RagdollRig& GetRig() const
    {
        return m_rig;
    }

    HYP_FORCE_INLINE JPH::PhysicsSystem* GetPhysicsSystem() const
    {
        return m_physicsSystem;
    }

    HYP_FORCE_INLINE bool IsCreated() const
    {
        return m_ragdoll != nullptr;
    }

    bool IsAwake() const;

    HYP_FORCE_INLINE bool IsLimp() const
    {
        return m_isLimp;
    }

    HYP_FORCE_INLINE uint32 GetSerial() const
    {
        return m_serial;
    }

    RagdollMath::GravityFrame GetGravityFrame() const;

    void AddImpulse(JPH::BodyID bodyId, JPH::Vec3Arg impulse, JPH::Vec3Arg point);

    // the pose to show: animated bones, with the bodies that physics drives over them. False when none are driven
    bool WritePose(Span<const Transform> animatedPose, const SkeletonBindData& bindData, const Mat4f& entityWorldMatrix, Array<Transform>& outPose);

    void DebugDraw(DebugDrawCommandList& list) const;

    void OnStep(const JPH::PhysicsStepListenerContext& inContext) override;

private:
    void RestoreTableMasses(JPH::RagdollSettings& settings);
    bool AddToPhysicsSystem(JPH::RagdollSettings& settings);
    void CreateRootAssist(JPH::Vec3Arg rootPosition);
    void InitBodyArrays();

    void BuildRig(const RagdollMath::GravityFrame& frame);
    void FindRigLimbs(uint32& outArmCount, uint32& outLegCount);
    bool InitRigFeet(const RagdollMath::GravityFrame& frame);
    void InitRigSides(const RagdollMath::GravityFrame& frame, uint32 armCount, uint32 legCount);
    int32 FindChildBody(uint32 parent, RagdollBodyPart part) const;
    bool FindLimbChain(uint32 root, uint32& outMiddle, uint32& outEnd) const;

    void MoveKinematicBodies(JPH::BodyInterface& bodyInterface, float fraction, float deltaTime);
    void UpdateSubtrees(JPH::BodyInterface& bodyInterface);
    float GetRawSubtreeInertia(uint32 bodyIndex, JPH::Vec3Arg pivot) const;
    float GetSubtreeInertia(uint32 bodyIndex, JPH::Vec3Arg pivot) const;
    float GetSupportedInertia(uint32 bodyIndex, JPH::Vec3Arg pivot) const;
    JPH::SwingTwistConstraint* GetJoint(uint32 bodyIndex) const;
    MotorOverride& SetMotorTarget(uint32 bodyIndex, JPH::QuatArg parentWorld, JPH::QuatArg childWorld, float frequencyScale = 1.0f, bool isSupported = false);
    void ApplyMotor(uint32 bodyIndex);
    float GetForceVelocityScale(uint32 bodyIndex, JPH::QuatArg jointTarget) const;
    void ApplyGravityCompensation(JPH::BodyInterface& bodyInterface, JPH::Vec3Arg gravity);
    void ApplyRootAssist(JPH::BodyInterface& bodyInterface, JPH::Vec3Arg gravity);

    // Jolt keeps a force until the end of the whole update, so one added every collision step would stack up; these
    // apply the step's share as an impulse instead
    void AddStepForce(JPH::BodyInterface& bodyInterface, JPH::BodyID bodyId, JPH::Vec3Arg force) const;
    void AddStepTorque(JPH::BodyInterface& bodyInterface, JPH::BodyID bodyId, JPH::Vec3Arg torque) const;

    void ClearMotorOverrides();
    float GetMotorTorqueFraction(uint32 bodyIndex) const;

    void SnapBodyToTarget(JPH::BodyInterface& bodyInterface, uint32 bodyIndex, JPH::EActivation activation);
    void GetBodyWorld(uint32 bodyIndex, JPH::Vec3& outPosition, JPH::Quat& outOrientation) const;
    JPH::BodyInterface& GetBodyInterface() const;

    void DrawDebugBodies(DebugDrawCommandList& list, Span<const BodyState> samples, Array<JPH::Vec3>& outCenters) const;
    void DrawDebugLinks(DebugDrawCommandList& list, Span<const JPH::Vec3> centers) const;
    void DrawDebugSupport(DebugDrawCommandList& list, Span<const BodyState> samples) const;

    PhysicsWorldBase* m_physicsWorld = nullptr;
    JPH::PhysicsSystem* m_physicsSystem = nullptr;
    RagdollContacts* m_contacts = nullptr;
    JPH::Ref<JPH::Ragdoll> m_ragdoll;
    uint32 m_serial = 0;

    Array<BodyInfo> m_bodies;
    Array<int32> m_bodyOfSlot;
    Array<BodyTarget> m_targets;
    Array<BodyState> m_states;
    Array<MotorOverride> m_motorOverrides;
    // joint targets balance last drove, faded out after it lets go so the legs don't snap
    Array<JPH::Quat> m_overrideTargets;
    Array<uint8> m_hadOverride;
    float m_overrideFade = 0.0f;
    Array<BodyLayer> m_layers;
    float m_totalMass = 0.0f;
    bool m_hasTargets = false;
    bool m_hasMotors = false;
    bool m_isLimp = false;

    JPH::Ref<JPH::SixDOFConstraint> m_rootAssistConstraint;
    JPH::Vec3 m_rootAnchor = JPH::Vec3::sZero();
    float m_rootAssist = 0.0f;

    RagdollRig m_rig;
    RagdollBalance m_balance;
    bool m_isBalancing = false;
    uint32 m_contactGeneration = 0;
    uint32 m_substep = 0;
    float m_stepDeltaTime = 1.0f / 120.0f;

    Mat4f m_worldInverse;
    Array<Mat4f> m_poseModelMatrices;
};

} // namespace Hyperion
