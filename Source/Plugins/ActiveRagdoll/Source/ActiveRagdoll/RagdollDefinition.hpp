/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Asset/AssetObject.hpp>

#include <Animation/HumanoidMapping.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

class Skeleton;
struct SkeletonBindData;

HYP_ENUM()
enum class RagdollJoint : uint8
{
    Cone = 0,
    HingeForward,
    HingeBackward
};

HYP_ENUM()
enum class RagdollBodyPart : uint8
{
    Torso = 0,
    Head,
    Arm,
    Leg
};

HYP_STRUCT()
struct RagdollMuscle
{
    HYP_STRUCT_BODY(RagdollMuscle);

    // motor frequency (Hz) at full strength
    HYP_FIELD(Property = "Stiffness", Serialize = true, Editor = true)
    float stiffness = 10.0f;

    HYP_FIELD(Property = "Damping", Serialize = true, Editor = true)
    float damping = 1.0f;

    HYP_FIELD(Property = "MaxTorque", Serialize = true, Editor = true)
    float maxTorque = 150.0f;

    HYP_FORCE_INLINE bool operator==(const RagdollMuscle& other) const
    {
        return stiffness == other.stiffness
            && damping == other.damping
            && maxTorque == other.maxTorque;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(stiffness);
        hc.Add(damping);
        hc.Add(maxTorque);

        return hc;
    }
};

// one physics body from a bone to the start of the next; distances are metres at an entity scale of 1, angles degrees
HYP_STRUCT()
struct RagdollBodyDefinition
{
    HYP_STRUCT_BODY(RagdollBodyDefinition);

    // the first of these the skeleton maps is the body's bone
    HYP_FIELD(Property = "Bones", Serialize = true, Editor = true)
    Array<HumanoidBone> bones;

    // the body ends at the first of these the skeleton maps, else at the tip of its own bone
    HYP_FIELD(Property = "Ends", Serialize = true, Editor = true)
    Array<HumanoidBone> ends;

    // end past the end bone, at the middle of its children
    HYP_FIELD(Property = "EndAtTip", Serialize = true, Editor = true)
    bool endAtTip = false;

    HYP_FIELD(Property = "Part", Serialize = true, Editor = true)
    RagdollBodyPart part = RagdollBodyPart::Torso;

    HYP_FIELD(Property = "Joint", Serialize = true, Editor = true)
    RagdollJoint joint = RagdollJoint::Cone;

    HYP_FIELD(Property = "ExtraLength", Serialize = true, Editor = true)
    float extraLength = 0.0f;

    HYP_FIELD(Property = "Radius", Serialize = true, Editor = true)
    float radius = 0.05f;

    HYP_FIELD(Property = "Swing", Serialize = true, Editor = true)
    float swing = 0.0f;

    // anatomical swing ranges from the pose the ragdoll is built in, replacing Swing when any is set: towards the body's
    // front (or up, for a bone along it), away from the midline (or up, for a bone pointing sideways), and the opposites
    HYP_FIELD(Property = "SwingForward", Serialize = true, Editor = true)
    float swingForward = 0.0f;

    HYP_FIELD(Property = "SwingBack", Serialize = true, Editor = true)
    float swingBack = 0.0f;

    HYP_FIELD(Property = "SwingOut", Serialize = true, Editor = true)
    float swingOut = 0.0f;

    HYP_FIELD(Property = "SwingIn", Serialize = true, Editor = true)
    float swingIn = 0.0f;

    // passive resistance of the joint's tissue (N m), felt whenever its muscles aren't driving it
    HYP_FIELD(Property = "JointFriction", Serialize = true, Editor = true)
    float jointFriction = 0.5f;

    HYP_FIELD(Property = "Twist", Serialize = true, Editor = true)
    float twist = 0.0f;

    // range of a hinge joint
    HYP_FIELD(Property = "Flex", Serialize = true, Editor = true)
    float flex = 0.0f;

    HYP_FIELD(Property = "MassFraction", Serialize = true, Editor = true)
    float massFraction = 0.0f;

    // a foot is a box this wide, starting this far behind the ankle
    HYP_FIELD(Property = "Foot", Serialize = true, Editor = true)
    bool foot = false;

    HYP_FIELD(Property = "FootWidth", Serialize = true, Editor = true)
    float footWidth = 0.0f;

    HYP_FIELD(Property = "HeelLength", Serialize = true, Editor = true)
    float heelLength = 0.0f;

    HYP_FIELD(Property = "Friction", Serialize = true, Editor = true)
    float friction = 0.8f;

    // loosening from a hit stops at an anchor instead of spreading up the chain
    HYP_FIELD(Property = "Anchor", Serialize = true, Editor = true)
    bool anchor = false;

    HYP_FIELD(Property = "Muscle", Serialize = true, Editor = true)
    RagdollMuscle muscle;

    HYP_FORCE_INLINE bool operator==(const RagdollBodyDefinition& other) const
    {
        return bones == other.bones
            && ends == other.ends
            && endAtTip == other.endAtTip
            && part == other.part
            && joint == other.joint
            && extraLength == other.extraLength
            && radius == other.radius
            && swing == other.swing
            && swingForward == other.swingForward
            && swingBack == other.swingBack
            && swingOut == other.swingOut
            && swingIn == other.swingIn
            && jointFriction == other.jointFriction
            && twist == other.twist
            && flex == other.flex
            && massFraction == other.massFraction
            && foot == other.foot
            && footWidth == other.footWidth
            && heelLength == other.heelLength
            && friction == other.friction
            && anchor == other.anchor
            && muscle == other.muscle;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;

        for (HumanoidBone bone : bones)
        {
            hc.Add(uint32(bone));
        }

        for (HumanoidBone end : ends)
        {
            hc.Add(uint32(end));
        }

        hc.Add(endAtTip);
        hc.Add(uint32(part));
        hc.Add(uint32(joint));
        hc.Add(extraLength);
        hc.Add(radius);
        hc.Add(swing);
        hc.Add(swingForward);
        hc.Add(swingBack);
        hc.Add(swingOut);
        hc.Add(swingIn);
        hc.Add(jointFriction);
        hc.Add(twist);
        hc.Add(flex);
        hc.Add(massFraction);
        hc.Add(foot);
        hc.Add(footWidth);
        hc.Add(heelLength);
        hc.Add(friction);
        hc.Add(anchor);
        hc.Add(muscle.GetHashCode());

        return hc;
    }
};

// a body of the definition found on a skeleton: its bone, and where it ends in the pose's model matrices
struct RagdollResolvedBody
{
    const RagdollBodyDefinition* definition = nullptr;
    uint32 slot = ~0u;
    uint32 endSlot = ~0u;
    bool endAtTip = false;
    float massFraction = 0.0f;
};

HYP_CLASS(AssetBucket = "PhysicsShapes")
class ACTIVERAGDOLL_API RagdollDefinition final : public AssetObject
{
    HYP_OBJECT_BODY(RagdollDefinition);

public:
    RagdollDefinition();
    explicit RagdollDefinition(Name name);

    ~RagdollDefinition() override;

    HYP_METHOD(Property = "TotalMass", NoScriptBindings)
    float GetTotalMass() const
    {
        return m_totalMass;
    }

    HYP_METHOD(Property = "TotalMass", NoScriptBindings)
    void SetTotalMass(float totalMass)
    {
        m_totalMass = totalMass;
    }

    HYP_METHOD(Property = "Bodies", NoScriptBindings)
    const Array<RagdollBodyDefinition>& GetBodies() const
    {
        return m_bodies;
    }

    HYP_METHOD(Property = "Bodies", NoScriptBindings)
    void SetBodies(const Array<RagdollBodyDefinition>& bodies)
    {
        m_bodies = bodies;
    }

    // bodies whose bones the skeleton's humanoid mapping has, parents first, with mass fractions summing to 1
    bool Resolve(const Skeleton& skeleton, Array<RagdollResolvedBody>& outBodies) const;

    // the humanoid ragdoll in the plugin's content
    static Handle<RagdollDefinition> GetDefaultHumanoid();

private:
    HYP_FIELD(Property = "TotalMass")
    float m_totalMass = 75.0f;

    HYP_FIELD(Property = "Bodies")
    Array<RagdollBodyDefinition> m_bodies;
};

} // namespace Hyperion
