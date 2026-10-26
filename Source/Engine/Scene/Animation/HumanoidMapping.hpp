/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Containers/Array.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Reflection/ObjectMacros.hpp>

#include <Core/Utilities/Span.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

struct SkeletonBindData;

HYP_ENUM()
enum class HumanoidBone : uint32
{
    Hips = 0,
    Spine,
    Chest,
    UpperChest,
    Neck,
    Head,
    LeftShoulder,
    LeftUpperArm,
    LeftLowerArm,
    LeftHand,
    RightShoulder,
    RightUpperArm,
    RightLowerArm,
    RightHand,
    LeftUpperLeg,
    LeftLowerLeg,
    LeftFoot,
    LeftToes,
    RightUpperLeg,
    RightLowerLeg,
    RightFoot,
    RightToes
};

static constexpr uint32 NumHumanoidBones = uint32(HumanoidBone::RightToes) + 1;

HYP_STRUCT()
struct HumanoidBoneBinding
{
    HYP_STRUCT_BODY(HumanoidBoneBinding);

    HYP_FIELD(Property = "Bone", Serialize = true, Editor = true)
    HumanoidBone bone = HumanoidBone::Hips;

    HYP_FIELD(Property = "BoneName", Serialize = true, Editor = true, EditorType = "Bone")
    Name boneName;

    HYP_FORCE_INLINE bool operator==(const HumanoidBoneBinding& other) const
    {
        return bone == other.bone && boneName == other.boneName;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(uint32(bone));
        hc.Add(boneName);

        return hc;
    }
};

HYP_STRUCT()
struct HumanoidMapping
{
    HYP_STRUCT_BODY(HumanoidMapping);

    HYP_FIELD(Property = "Bones", Serialize = true, Editor = true)
    Array<HumanoidBoneBinding> bones;

    Name GetBoneName(HumanoidBone bone) const;

    // an invalid name unmaps the bone
    void SetBoneName(HumanoidBone bone, Name boneName);

    // the hips, spine, head, arms, hands, legs and feet are all mapped
    bool IsHumanoid() const;

    bool IsHumanoid(const SkeletonBindData& bindData) const;

    static bool IsRequired(HumanoidBone bone);

    static HumanoidMapping AutoMap(const SkeletonBindData& bindData);

    HYP_FORCE_INLINE bool operator==(const HumanoidMapping& other) const
    {
        return bones == other.bones;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;

        for (const HumanoidBoneBinding& binding : bones)
        {
            hc.Add(binding.GetHashCode());
        }

        return hc;
    }
};

} // namespace Hyperion
