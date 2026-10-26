/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <Animation/HumanoidMapping.hpp>
#include <Scene/Animation/Skeleton.hpp>
#include <Scene/Animation/Animation.hpp>

#include <Asset/Assets.hpp>
#include <Asset/AssetRegistry.hpp>
#include <Asset/SerializationUtils.hpp>

#include <Core/Logging/Logger.hpp>
#include <Core/Logging/LogChannels.hpp>

#include <Core/Utilities/StringUtil.hpp>

#include <HumanoidMapping.generated.inl>

namespace Hyperion {

namespace {

struct HumanoidBoneNames
{
    HumanoidBone bone;
    Array<ANSIStringView> names;
};

/// Auto mapping table
static const HumanoidBoneNames HumanoidAutoMapTable[] = {
    { HumanoidBone::Hips, { "hips", "pelvis", "hip", "ccbasehip", "defspine" } },
    { HumanoidBone::Spine, { "spine", "spine01", "ccbasewaist", "defspine001", "abdomenlower" } },
    { HumanoidBone::Chest, { "chest", "spine1", "spine02", "ccbasespine01", "defspine002", "abdomenupper" } },
    { HumanoidBone::UpperChest, { "upperchest", "spine2", "spine03", "ccbasespine02", "defspine003", "chestupper" } },
    { HumanoidBone::Neck, { "neck", "neck01", "ccbasenecktwist01", "defspine004", "necklower" } },
    { HumanoidBone::Head, { "head", "ccbasehead", "defspine006" } },
    { HumanoidBone::LeftShoulder, { "leftshoulder", "claviclel", "ccbaselclavicle", "defshoulderl", "lcollar" } },
    { HumanoidBone::LeftUpperArm, { "leftupperarm", "leftarm", "upperarml", "ccbaselupperarm", "defupperarml", "lshldr" } },
    { HumanoidBone::LeftLowerArm, { "leftlowerarm", "leftforearm", "lowerarml", "forearml", "ccbaselforearm", "defforearml", "lforearmbend" } },
    { HumanoidBone::LeftHand, { "lefthand", "handl", "ccbaselhand", "defhandl", "lhand" } },
    { HumanoidBone::RightShoulder, { "rightshoulder", "clavicler", "ccbaserclavicle", "defshoulderr", "rcollar" } },
    { HumanoidBone::RightUpperArm, { "rightupperarm", "rightarm", "upperarmr", "ccbaserupperarm", "defupperarmr", "rshldr" } },
    { HumanoidBone::RightLowerArm, { "rightlowerarm", "rightforearm", "lowerarmr", "forearmr", "ccbaserforearm", "defforearmr", "rforearmbend" } },
    { HumanoidBone::RightHand, { "righthand", "handr", "ccbaserhand", "defhandr", "rhand" } },
    { HumanoidBone::LeftUpperLeg, { "leftupperleg", "leftupleg", "thighl", "ccbaselthigh", "defthighl", "lthigh" } },
    { HumanoidBone::LeftLowerLeg, { "leftlowerleg", "leftleg", "calfl", "shinl", "ccbaselcalf", "defshinl", "lshin" } },
    { HumanoidBone::LeftFoot, { "leftfoot", "footl", "ccbaselfoot", "deffootl", "lfoot" } },
    { HumanoidBone::LeftToes, { "lefttoes", "lefttoebase", "balll", "toel", "ccbaseltoebase", "deftoel", "ltoe" } },
    { HumanoidBone::RightUpperLeg, { "rightupperleg", "rightupleg", "thighr", "ccbaserthigh", "defthighr", "rthigh" } },
    { HumanoidBone::RightLowerLeg, { "rightlowerleg", "rightleg", "calfr", "shinr", "ccbasercalf", "defshinr", "rshin" } },
    { HumanoidBone::RightFoot, { "rightfoot", "footr", "ccbaserfoot", "deffootr", "rfoot" } },
    { HumanoidBone::RightToes, { "righttoes", "righttoebase", "ballr", "toer", "ccbasertoebase", "deftoer", "rtoe" } }
};

static constexpr const char* MixamoPrefix = "mixamorig";

Array<ANSIString> SplitBoneName(Name boneName)
{
    return ANSIString(boneName.LookupString()).ToLower().Split('_', '-', '.', ':', ' ');
}

bool HasNumericSuffix(const Array<ANSIString>& parts)
{
    return parts.Size() > 1 && StringUtil::IsDigits(parts.Back());
}

// the name as compared: its parts joined, without Mixamo's prefix and, for an importer-numbered rig, the trailing number
ANSIString GetBoneNameKey(Array<ANSIString> parts, bool stripNumericSuffix)
{
    if (stripNumericSuffix && HasNumericSuffix(parts))
    {
        parts.PopBack();
    }

    ANSIString key = ANSIString::Join(parts);

    const ANSIString mixamoPrefix(MixamoPrefix);

    if (key.StartsWith(mixamoPrefix) && key.Size() > mixamoPrefix.Size())
    {
        key = ANSIString(key.Substr(mixamoPrefix.Size()));
    }

    return key;
}

} // namespace

Name HumanoidMapping::GetBoneName(HumanoidBone bone) const
{
    for (const HumanoidBoneBinding& binding : bones)
    {
        if (binding.bone == bone)
        {
            return binding.boneName;
        }
    }

    return Name::Invalid();
}

void HumanoidMapping::SetBoneName(HumanoidBone bone, Name boneName)
{
    for (auto it = bones.Begin(); it != bones.End(); ++it)
    {
        if (it->bone != bone)
        {
            continue;
        }

        if (boneName.IsValid())
        {
            it->boneName = boneName;
        }
        else
        {
            bones.Erase(it);
        }

        return;
    }

    if (boneName.IsValid())
    {
        bones.PushBack(HumanoidBoneBinding { bone, boneName });
    }
}

bool HumanoidMapping::IsRequired(HumanoidBone bone)
{
    switch (bone)
    {
    case HumanoidBone::Chest:
    case HumanoidBone::UpperChest:
    case HumanoidBone::Neck:
    case HumanoidBone::LeftShoulder:
    case HumanoidBone::RightShoulder:
    case HumanoidBone::LeftToes:
    case HumanoidBone::RightToes:
        return false;
    default:
        return true;
    }
}

bool HumanoidMapping::IsHumanoid() const
{
    for (uint32 boneIndex = 0; boneIndex < NumHumanoidBones; ++boneIndex)
    {
        if (IsRequired(HumanoidBone(boneIndex)) && !GetBoneName(HumanoidBone(boneIndex)).IsValid())
        {
            return false;
        }
    }

    return true;
}

bool HumanoidMapping::IsHumanoid(const SkeletonBindData& bindData) const
{
    for (uint32 boneIndex = 0; boneIndex < NumHumanoidBones; ++boneIndex)
    {
        if (!IsRequired(HumanoidBone(boneIndex)))
        {
            continue;
        }

        const Name boneName = GetBoneName(HumanoidBone(boneIndex));

        if (!boneName.IsValid() || bindData.FindSlot(boneName) == SkeletonBindData::InvalidSlot)
        {
            return false;
        }
    }

    return true;
}

HumanoidMapping HumanoidMapping::AutoMap(const SkeletonBindData& bindData)
{
    HumanoidMapping mapping;

    Array<Array<ANSIString>> partsBySlot;
    partsBySlot.Resize(bindData.NumSlots());

    uint32 numNamed = 0;
    uint32 numSuffixed = 0;

    for (uint32 slot = 0; slot < bindData.NumSlots(); ++slot)
    {
        if (!bindData.boneNames[slot].IsValid())
        {
            continue;
        }

        partsBySlot[slot] = SplitBoneName(bindData.boneNames[slot]);

        ++numNamed;
        numSuffixed += HasNumericSuffix(partsBySlot[slot]) ? 1 : 0;
    }

    // an importer that numbers nodes numbers nearly all of them, while a rig's own numbered bones ("spine_01") are a few
    const bool stripNumericSuffix = numSuffixed * 2 > numNamed;

    Bitset isSlotUsed;

    Array<ANSIString> keysBySlot;
    keysBySlot.Resize(bindData.NumSlots());

    for (uint32 slot = 0; slot < bindData.NumSlots(); ++slot)
    {
        if (!partsBySlot[slot].Empty())
        {
            keysBySlot[slot] = GetBoneNameKey(partsBySlot[slot], stripNumericSuffix);
        }
    }

    for (const HumanoidBoneNames& entry : HumanoidAutoMapTable)
    {
        if (mapping.GetBoneName(entry.bone).IsValid())
        {
            continue;
        }

        for (const ANSIStringView& name : entry.names)
        {
            for (uint32 slot = 0; slot < bindData.NumSlots(); ++slot)
            {
                if (!isSlotUsed.Test(slot)
                    && !keysBySlot[slot].Empty()
                    && keysBySlot[slot] == name)
                {
                    mapping.SetBoneName(entry.bone, bindData.boneNames[slot]);
                    isSlotUsed.Set(slot, true);

                    break;
                }
            }
        }
    }

    return mapping;
}

void HumanoidSkeletonData::SetMapping(const HumanoidMapping& mapping)
{
    m_mapping = mapping;

    NotifyChanged();
}

void HumanoidSkeletonData::SetBones(const Array<HumanoidBoneBinding>& bones)
{
    m_mapping.bones = bones;

    NotifyChanged();
}

Handle<Skeleton> HumanoidSkeletonData::GetSkeleton() const
{
    Handle<AssetObject> owner = GetOwner();

    return owner.IsValid() && owner->IsA<Skeleton>() ? Cast<Skeleton>(owner) : Handle<Skeleton>::empty;
}

void HumanoidSkeletonData::InitForAsset(const AssetObject& asset)
{
    const Skeleton& skeleton = static_cast<const Skeleton&>(asset);

    if (BoxedValue legacyHumanoid = skeleton.TakeLegacyHumanoid(); legacyHumanoid.IsValid())
    {
        TResult<BoxedValue> legacyResult = BoxedAsType(legacyHumanoid, TypeInfo::ForType<HumanoidMapping>());

        if (legacyResult.HasValue() && legacyResult.GetValue().Is<HumanoidMapping>())
        {
            m_mapping = legacyResult.GetValue().Get<HumanoidMapping>();
        }
        else
        {
            HYP_LOG(Animation, Warning, "Skeleton '{}' has a Humanoid property that doesn't read as a humanoid mapping; auto-mapping it instead", skeleton.GetName());
        }
    }

    if (m_mapping.bones.Empty())
    {
        m_mapping = HumanoidMapping::AutoMap(skeleton.GetBindData());
    }
}

void HumanoidSkeletonData::AutoMap()
{
    const Handle<Skeleton> skeleton = GetSkeleton();

    if (!skeleton.IsValid())
    {
        return;
    }

    const HumanoidMapping autoMapped = HumanoidMapping::AutoMap(skeleton->GetBindData());

    HumanoidMapping mapping = m_mapping;

    for (const HumanoidBoneBinding& binding : autoMapped.bones)
    {
        if (!mapping.GetBoneName(binding.bone).IsValid())
        {
            mapping.SetBoneName(binding.bone, binding.boneName);
        }
    }

    SetMapping(mapping);
}

Array<Name> HumanoidSkeletonData::GetCompatibleClipNames() const
{
    const Handle<Skeleton> skeleton = GetSkeleton();

    if (!skeleton.IsValid())
    {
        return {};
    }

    Array<Name> clipNames = skeleton->GetAnimationNames();

    if (!m_mapping.IsHumanoid(skeleton->GetBindData()))
    {
        return clipNames;
    }

    for (const Handle<Skeleton>& otherSkeleton : CollectHumanoidSkeletons())
    {
        if (otherSkeleton == skeleton)
        {
            continue;
        }

        for (const Handle<Animation>& animation : otherSkeleton->GetAnimations())
        {
            if (animation.IsValid())
            {
                clipNames.PushBack(NAME_FMT("{} ({})", animation->GetName(), otherSkeleton->GetName()));
            }
        }
    }

    return clipNames;
}

Handle<Animation> HumanoidSkeletonData::FindCompatibleClip(Name name) const
{
    const Handle<Skeleton> skeleton = GetSkeleton();

    if (!skeleton.IsValid())
    {
        return Handle<Animation>::empty;
    }

    if (const Handle<Animation>& animation = skeleton->FindAnimation(name); animation.IsValid())
    {
        return animation;
    }

    if (!m_mapping.IsHumanoid(skeleton->GetBindData()))
    {
        return Handle<Animation>::empty;
    }

    for (const Handle<Skeleton>& otherSkeleton : CollectHumanoidSkeletons())
    {
        for (const Handle<Animation>& animation : otherSkeleton->GetAnimations())
        {
            if (animation.IsValid() && NAME_FMT("{} ({})", animation->GetName(), otherSkeleton->GetName()) == name)
            {
                return animation;
            }
        }
    }

    return Handle<Animation>::empty;
}

Handle<HumanoidSkeletonData> HumanoidSkeletonData::ForSkeleton(const Skeleton& skeleton)
{
    const Skeleton& source = skeleton.GetInstanceSource() ? *skeleton.GetInstanceSource() : skeleton;

    return source.GetOrAddExtension<HumanoidSkeletonData>();
}

const HumanoidMapping& HumanoidSkeletonData::GetSkeletonMapping(const Skeleton& skeleton)
{
    static const HumanoidMapping s_emptyMapping;

    const Handle<HumanoidSkeletonData> data = ForSkeleton(skeleton);

    // the skeleton holds its extension, so the mapping outlives this handle
    return data.IsValid() ? data->m_mapping : s_emptyMapping;
}

bool HumanoidSkeletonData::IsHumanoid(const Skeleton& skeleton)
{
    return GetSkeletonMapping(skeleton).IsHumanoid(skeleton.GetBindData());
}

void HumanoidSkeletonData::AutoMapSkeleton(Skeleton& skeleton)
{
    if (const Handle<HumanoidSkeletonData> data = ForSkeleton(skeleton))
    {
        data->AutoMap();
    }
}

Array<Handle<Skeleton>> HumanoidSkeletonData::CollectHumanoidSkeletons()
{
    Array<Handle<Skeleton>> skeletons;

    for (const Handle<AssetRegistry>& registry : { GetCurrentAssetRegistry(), GetEngineAssetRegistry() })
    {
        if (!registry.IsValid())
        {
            continue;
        }

        Array<AssetDesc> skeletonDescs;
        registry->GetBucketAssetDescs(AssetBuckets::Skeletons.GetIndex(), skeletonDescs);

        for (const AssetDesc& skeletonDesc : skeletonDescs)
        {
            Handle<Skeleton> skeleton = registry->GetAsset<Skeleton>(AssetBuckets::Skeletons, skeletonDesc.name);

            if (skeleton.IsValid() && IsHumanoid(*skeleton) && !skeletons.Contains(skeleton))
            {
                skeletons.PushBack(std::move(skeleton));
            }
        }
    }

    return skeletons;
}

} // namespace Hyperion
