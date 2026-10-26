/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Asset/AssetExtension.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Reflection/ObjectMacros.hpp>

#include <Core/Utilities/Span.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

struct SkeletonBindData;

class Skeleton;
class Animation;

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
    RightToes,

    Max
};

static constexpr uint32 NumHumanoidBones = uint32(HumanoidBone::Max);

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
        return bone == other.bone
            && boneName == other.boneName;
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

/// a skeleton's humanoid bone roles, which retargeting, foot IK and motion matching read
HYP_CLASS(ExtendsAsset = "Skeleton", Label = "Humanoid")
class ENGINE_API HumanoidSkeletonData final : public AssetExtension
{
    HYP_OBJECT_BODY(HumanoidSkeletonData);

public:
    HumanoidSkeletonData() = default;

    explicit HumanoidSkeletonData(const HumanoidMapping& mapping)
        : m_mapping(mapping)
    {
    }

    HYP_FORCE_INLINE const HumanoidMapping& GetMapping() const
    {
        return m_mapping;
    }

    void SetMapping(const HumanoidMapping& mapping);

    HYP_METHOD(Property = "Bones", NoScriptBindings)
    const Array<HumanoidBoneBinding>& GetBones() const
    {
        return m_mapping.bones;
    }

    HYP_METHOD(Property = "Bones", NoScriptBindings)
    void SetBones(const Array<HumanoidBoneBinding>& bones);

    /// fills in the bones the mapping is missing from the skeleton's bone names
    HYP_METHOD(EditorOnly, EditorAction = "Auto-map")
    void AutoMap();

    /// the skeleton's own clips, then for a humanoid skeleton the clips of every other humanoid rig, named "Clip (Rig)"
    HYP_METHOD()
    Array<Name> GetCompatibleClipNames() const;

    HYP_METHOD()
    Handle<Animation> FindCompatibleClip(Name name) const;

    static Handle<HumanoidSkeletonData> ForSkeleton(const Skeleton& skeleton);

    static const HumanoidMapping& GetSkeletonMapping(const Skeleton& skeleton);

    static bool IsHumanoid(const Skeleton& skeleton);

    static void AutoMapSkeleton(Skeleton& skeleton);

    /// the humanoid skeletons in the current project and the engine content
    static Array<Handle<Skeleton>> CollectHumanoidSkeletons();

protected:
    /// from the Humanoid property of a skeleton saved before this extension existed,
    /// else, auto-mapped from the bone names
    void InitForAsset(const AssetObject& asset) override;

private:
    Handle<Skeleton> GetSkeleton() const;

    HumanoidMapping m_mapping;
};

} // namespace Hyperion
