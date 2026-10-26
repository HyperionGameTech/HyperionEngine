/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/Animation/HumanoidMapping.hpp>
#include <Scene/Animation/Skeleton.hpp>

#include <Core/Utilities/StringUtil.hpp>

#include <HumanoidMapping.generated.inl>

namespace Hyperion {

namespace {

struct HumanoidBoneNames
{
    HumanoidBone bone;
    const char* names[8];
};

// bone names as compared: lowercase, separators and a trailing "_<digits>" importer suffix removed, Mixamo's prefix dropped
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

    Array<bool> isSlotUsed;
    isSlotUsed.Resize(bindData.NumSlots());

    for (bool& isUsed : isSlotUsed)
    {
        isUsed = false;
    }

    Array<ANSIString> keysBySlot;
    keysBySlot.Resize(bindData.NumSlots());

    for (uint32 slot = 0; slot < bindData.NumSlots(); ++slot)
    {
        if (!partsBySlot[slot].Empty())
        {
            keysBySlot[slot] = GetBoneNameKey(partsBySlot[slot], stripNumericSuffix);
        }
    }

    // a part's names are in order of preference, so a rig's "chest" wins over an extra "spine-1" that reads like Mixamo's chest
    for (const HumanoidBoneNames& entry : HumanoidAutoMapTable)
    {
        for (const char* name : entry.names)
        {
            if (name == nullptr || mapping.GetBoneName(entry.bone).IsValid())
            {
                continue;
            }

            for (uint32 slot = 0; slot < bindData.NumSlots(); ++slot)
            {
                if (!isSlotUsed[slot] && !keysBySlot[slot].Empty() && keysBySlot[slot] == name)
                {
                    mapping.SetBoneName(entry.bone, bindData.boneNames[slot]);
                    isSlotUsed[slot] = true;

                    break;
                }
            }
        }
    }

    return mapping;
}

} // namespace Hyperion
