/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#ifdef HYP_TESTS

#include <Framework/Testing/EngineTest.hpp>
#include <Framework/EngineGlobals.hpp>

#include <Asset/AssetRegistry.hpp>
#include <Asset/SerializationUtils.hpp>

#include <Scene/Animation/Skeleton.hpp>
#include <Scene/Animation/Bone.hpp>
#include <Scene/Animation/Animation.hpp>
#include <Scene/Animation/AnimationPose.hpp>
#include <Scene/Animation/AnimationGraph.hpp>
#include <Scene/Animation/AnimationGraphInstance.hpp>
#include <Scene/Animation/HumanoidMapping.hpp>
#include <Scene/Animation/MotionMatching.hpp>

#include <Core/Utilities/GlobalContext.hpp>

#include <Core/DataProcessing/HMF/HMF.hpp>

#include <Core/Math/MathUtil.hpp>
#include <Core/Math/Quat4f.hpp>

namespace Hyperion {
namespace tests {
namespace scene {

namespace {

struct TestBone
{
    const char* name;
    int32 parent;
    Vec3f position;
};

// a small humanoid in model space, UE mannequin names, standing up +Y and facing +Z
static const TestBone MannequinBones[] = {
    { "pelvis", -1, Vec3f(0.0f, 1.0f, 0.0f) },
    { "spine_01", 0, Vec3f(0.0f, 1.15f, 0.0f) },
    { "spine_02", 1, Vec3f(0.0f, 1.3f, 0.0f) },
    { "neck_01", 2, Vec3f(0.0f, 1.55f, 0.0f) },
    { "head", 3, Vec3f(0.0f, 1.7f, 0.0f) },
    { "clavicle_l", 2, Vec3f(-0.08f, 1.5f, 0.0f) },
    { "upperarm_l", 5, Vec3f(-0.2f, 1.5f, 0.0f) },
    { "lowerarm_l", 6, Vec3f(-0.48f, 1.5f, 0.0f) },
    { "hand_l", 7, Vec3f(-0.72f, 1.5f, 0.0f) },
    { "clavicle_r", 2, Vec3f(0.08f, 1.5f, 0.0f) },
    { "upperarm_r", 9, Vec3f(0.2f, 1.5f, 0.0f) },
    { "lowerarm_r", 10, Vec3f(0.48f, 1.5f, 0.0f) },
    { "hand_r", 11, Vec3f(0.72f, 1.5f, 0.0f) },
    { "thigh_l", 0, Vec3f(-0.1f, 0.95f, 0.0f) },
    { "calf_l", 13, Vec3f(-0.1f, 0.5f, 0.0f) },
    { "foot_l", 14, Vec3f(-0.1f, 0.08f, 0.0f) },
    { "thigh_r", 0, Vec3f(0.1f, 0.95f, 0.0f) },
    { "calf_r", 16, Vec3f(0.1f, 0.5f, 0.0f) },
    { "foot_r", 17, Vec3f(0.1f, 0.08f, 0.0f) }
};

// the same bones by their Mixamo names, as a glTF import writes them
static const char* const MixamoNames[] = {
    "mixamorig:Hips_01", "mixamorig:Spine_02", "mixamorig:Spine1_03", "mixamorig:Neck_04", "mixamorig:Head_05",
    "mixamorig:LeftShoulder_06", "mixamorig:LeftArm_07", "mixamorig:LeftForeArm_08", "mixamorig:LeftHand_09",
    "mixamorig:RightShoulder_10", "mixamorig:RightArm_11", "mixamorig:RightForeArm_12", "mixamorig:RightHand_13",
    "mixamorig:LeftUpLeg_14", "mixamorig:LeftLeg_15", "mixamorig:LeftFoot_16",
    "mixamorig:RightUpLeg_17", "mixamorig:RightLeg_18", "mixamorig:RightFoot_19"
};

static constexpr uint32 NumTestBones = uint32(sizeof(MannequinBones) / sizeof(MannequinBones[0]));

// builds a skeleton whose bones sit at the given model-space positions with the given model-space rotations
Handle<Skeleton> BuildSkeleton(Name skeletonName, const char* const* boneNames, const Array<Vec3f>& positions, const Array<Quat4f>& rotations)
{
    Array<Handle<Bone>> bones;

    for (uint32 boneIndex = 0; boneIndex < NumTestBones; ++boneIndex)
    {
        const int32 parent = MannequinBones[boneIndex].parent;

        Vec3f localPosition = positions[boneIndex];
        Quat4f localRotation = rotations[boneIndex];

        if (parent >= 0)
        {
            const Quat4f inverseParentRotation = rotations[parent].Inverse();

            localPosition = inverseParentRotation.RotateVector(positions[boneIndex] - positions[parent]);
            localRotation = inverseParentRotation * rotations[boneIndex];
        }

        Handle<Bone> bone = MakeHandle<Bone>(Name(boneNames[boneIndex]));
        bone->SetBindingTransform(Transform(localPosition, Vec3f::One(), localRotation));

        if (parent >= 0)
        {
            bones[parent]->AddChild(bone);
        }

        bones.PushBack(bone);
    }

    Handle<Skeleton> skeleton = MakeHandle<Skeleton>(bones[0]);
    skeleton->SetName(skeletonName);

    return skeleton;
}

Array<Vec3f> ModelPositions(const SkeletonBindData& bindData, Span<const Transform> pose)
{
    Array<Mat4f> modelMatrices;
    Array<Mat4f> skinningMatrices;
    modelMatrices.Resize(bindData.NumSlots());
    skinningMatrices.Resize(bindData.NumSlots());

    PoseOps::ComputeMatrices(bindData, pose, modelMatrices.ToSpan(), skinningMatrices.ToSpan());

    Array<Vec3f> positions;

    for (const Mat4f& modelMatrix : modelMatrices)
    {
        positions.PushBack(modelMatrix.ExtractTranslation());
    }

    return positions;
}

Handle<AnimationTrack> MakeTrack(Name boneName, const Transform& transform)
{
    Handle<AnimationTrack> track = MakeHandle<AnimationTrack>(boneName, boneName);

    const Keyframe keyframes[] = { Keyframe(0.0f, transform), Keyframe(1.0f, transform) };
    track->SetKeyframes(Span<const Keyframe>(keyframes, 2));

    return track;
}

} // namespace

HYP_ENGINE_TEST(HumanoidRetarget, AutoMapFindsMannequinAndMixamoNames)
{
    Array<Vec3f> positions;
    Array<Quat4f> rotations;

    for (const TestBone& bone : MannequinBones)
    {
        positions.PushBack(bone.position);
        rotations.PushBack(Quat4f::Identity());
    }

    const char* mannequinNames[NumTestBones];

    for (uint32 boneIndex = 0; boneIndex < NumTestBones; ++boneIndex)
    {
        mannequinNames[boneIndex] = MannequinBones[boneIndex].name;
    }

    Handle<Skeleton> mannequin = BuildSkeleton(NAME("Mannequin"), mannequinNames, positions, rotations);
    Handle<Skeleton> mixamo = BuildSkeleton(NAME("Mixamo"), MixamoNames, positions, rotations);

    const HumanoidMapping mannequinMapping = HumanoidMapping::AutoMap(mannequin->GetBindData());
    const HumanoidMapping mixamoMapping = HumanoidMapping::AutoMap(mixamo->GetBindData());

    HYP_TEST_CHECK(mannequinMapping.IsHumanoid());
    HYP_TEST_CHECK(mixamoMapping.IsHumanoid());

    HYP_TEST_CHECK(mannequinMapping.GetBoneName(HumanoidBone::Hips) == NAME("pelvis"));
    HYP_TEST_CHECK(mannequinMapping.GetBoneName(HumanoidBone::Spine) == NAME("spine_01"));
    HYP_TEST_CHECK(mannequinMapping.GetBoneName(HumanoidBone::Chest) == NAME("spine_02"));
    HYP_TEST_CHECK(mannequinMapping.GetBoneName(HumanoidBone::LeftLowerLeg) == NAME("calf_l"));

    HYP_TEST_CHECK(mixamoMapping.GetBoneName(HumanoidBone::Hips) == NAME("mixamorig:Hips_01"));
    HYP_TEST_CHECK(mixamoMapping.GetBoneName(HumanoidBone::Chest) == NAME("mixamorig:Spine1_03"));
    HYP_TEST_CHECK(mixamoMapping.GetBoneName(HumanoidBone::RightUpperArm) == NAME("mixamorig:RightArm_11"));
    HYP_TEST_CHECK(mixamoMapping.GetBoneName(HumanoidBone::LeftShoulder) == NAME("mixamorig:LeftShoulder_06"));
}

HYP_ENGINE_TEST(HumanoidRetarget, ClipMovesJointsOnAnotherRig)
{
    Handle<AssetRegistry> scratchRegistry = MakeHandle<AssetRegistry>(AssetRegistryId::Game, EngineGlobals::GetTempDirectory() / "HumanoidRetargetTests");
    scratchRegistry->Initialize(nullptr);

    GlobalContextScope registryScope { AssetRegistryContext { scratchRegistry } };

    // the target rig faces another way, is 100 times larger and has its own bone axes
    constexpr float TargetScale = 100.0f;
    const Quat4f targetFacing(Vec3f(0.3f, 1.0f, 0.2f).Normalized(), 1.1f);

    Array<Vec3f> sourcePositions;
    Array<Quat4f> sourceRotations;
    Array<Vec3f> targetPositions;
    Array<Quat4f> targetRotations;

    const char* mannequinNames[NumTestBones];

    for (uint32 boneIndex = 0; boneIndex < NumTestBones; ++boneIndex)
    {
        mannequinNames[boneIndex] = MannequinBones[boneIndex].name;

        const Vec3f position = MannequinBones[boneIndex].position;
        const Quat4f sourceRotation(Vec3f(1.0f, 0.0f, 0.0f), 0.05f * float(boneIndex));
        const Quat4f boneAxes(Vec3f(0.0f, 0.0f, 1.0f).Normalized(), 0.4f + 0.13f * float(boneIndex));

        sourcePositions.PushBack(position);
        sourceRotations.PushBack(sourceRotation);
        targetPositions.PushBack(targetFacing.RotateVector(position * TargetScale));
        targetRotations.PushBack(targetFacing * sourceRotation * boneAxes);
    }

    Handle<Skeleton> source = BuildSkeleton(NAME("RetargetSource"), mannequinNames, sourcePositions, sourceRotations);
    Handle<Skeleton> target = BuildSkeleton(NAME("RetargetTarget"), MixamoNames, targetPositions, targetRotations);

    source->AutoMapHumanoid();
    target->AutoMapHumanoid();

    HYP_TEST_REQUIRE(source->IsHumanoid() && target->IsHumanoid());

    scratchRegistry->PutAssetsDeep(source, /* overwriteExisting */ true);

    const SkeletonBindData& sourceBindData = source->GetBindData();
    const SkeletonBindData& targetBindData = target->GetBindData();

    // the construction itself: both rigs' joints line up before anything plays
    const Array<Vec3f> sourceBindPositions = ModelPositions(sourceBindData, sourceBindData.restLocals.ToSpan());
    const Array<Vec3f> targetBindPositions = ModelPositions(targetBindData, targetBindData.restLocals.ToSpan());

    HYP_TEST_REQUIRE((targetBindPositions[8] - targetFacing.RotateVector(sourceBindPositions[8] * TargetScale)).Length() < 1e-2f);

    // bend the left arm and the spine, turn the right leg, and drop the hips
    Handle<Animation> clip = MakeHandle<Animation>(NAME("Wave"));

    const auto turned = [&sourceBindData](uint32 slot, const Vec3f& axis, float radians)
    {
        Transform transform = sourceBindData.restLocals[slot];
        transform.SetRotation(transform.GetRotation() * Quat4f(axis.Normalized(), radians));

        return transform;
    };

    Transform hips = sourceBindData.restLocals[0];
    hips.SetTranslation(hips.GetTranslation() + Vec3f(0.05f, -0.2f, 0.1f));

    clip->AddTrack(MakeTrack(NAME("pelvis"), hips));
    clip->AddTrack(MakeTrack(NAME("spine_01"), turned(1, Vec3f(1.0f, 0.0f, 0.0f), 0.4f)));
    clip->AddTrack(MakeTrack(NAME("upperarm_l"), turned(6, Vec3f(0.0f, 0.3f, 1.0f), -0.9f)));
    clip->AddTrack(MakeTrack(NAME("lowerarm_l"), turned(7, Vec3f(0.0f, 1.0f, 0.0f), 1.2f)));
    clip->AddTrack(MakeTrack(NAME("thigh_r"), turned(16, Vec3f(1.0f, 0.2f, 0.0f), 0.7f)));
    clip->StampSourceSkeleton(*source);

    // the clip names its skeleton by path, which survives a save, and finds it without holding on to it
    HYP_TEST_CHECK(clip->GetSourceSkeleton().GetAssetPath() == source->GetPath());
    HYP_TEST_CHECK(!clip->GetSourceSkeleton().IsLoaded());
    HYP_TEST_CHECK(clip->ResolveSourceSkeleton() == source);
    HYP_TEST_CHECK(!clip->GetSourceSkeleton().IsLoaded());

    scratchRegistry->PutAssetsDeep(clip, /* overwriteExisting */ true);

    String clipText;
    HYP_TEST_REQUIRE(!ObjectToHMF(Animation::StaticClass(), BoxedValue(clip), clipText).HasError());

    HMF::ParseResult parsedClip = HMF::Parse(clipText);
    HYP_TEST_REQUIRE(parsedClip.HasValue() && parsedClip.GetValue().Is<Handle<ObjectBase>>());

    const Handle<Animation> loadedClip = TryCast<Animation>(parsedClip.GetValue().Get<Handle<ObjectBase>>());
    HYP_TEST_REQUIRE(loadedClip.IsValid());
    HYP_TEST_CHECK_MSG(loadedClip->GetSourceSkeleton().GetAssetPath() == source->GetPath(), "read back as {}", loadedClip->GetSourceSkeleton().GetAssetPath());

    const ClipBinding binding = PoseOps::BindClip(*clip, *target);
    HYP_TEST_CHECK_MSG(bool(binding.retarget), "the clip should retarget");
    HYP_TEST_REQUIRE(bool(binding.retarget));
    HYP_TEST_CHECK(PoseOps::CountBoundSlots(binding) == NumTestBones);

    Array<Transform> sourcePose = PoseOps::MakeRestPose(sourceBindData);
    PoseOps::SampleClip(*clip, PoseOps::BindClip(*clip, *source), sourceBindData, 0.5f, sourcePose.ToSpan());

    Array<Transform> targetPose = PoseOps::MakeRestPose(targetBindData);
    PoseOps::SampleClip(*clip, binding, targetBindData, 0.5f, targetPose.ToSpan());

    const Array<Vec3f> sourceJoints = ModelPositions(sourceBindData, sourcePose.ToSpan());
    const Array<Vec3f> targetJoints = ModelPositions(targetBindData, targetPose.ToSpan());

    for (uint32 boneIndex = 0; boneIndex < NumTestBones; ++boneIndex)
    {
        const Vec3f expected = targetFacing.RotateVector(sourceJoints[boneIndex] * TargetScale);
        const float error = (targetJoints[boneIndex] - expected).Length();

        HYP_TEST_CHECK_MSG(error < 0.05f, "{} is {} off where the source puts it", MixamoNames[boneIndex], error);
    }
}

namespace {

Handle<Skeleton> BuildMannequin()
{
    Array<Vec3f> positions;
    Array<Quat4f> rotations;

    const char* mannequinNames[NumTestBones];

    for (uint32 boneIndex = 0; boneIndex < NumTestBones; ++boneIndex)
    {
        mannequinNames[boneIndex] = MannequinBones[boneIndex].name;
        positions.PushBack(MannequinBones[boneIndex].position);
        rotations.PushBack(Quat4f::Identity());
    }

    Handle<Skeleton> skeleton = BuildSkeleton(NAME("Mannequin"), mannequinNames, positions, rotations);
    skeleton->AutoMapHumanoid();

    return skeleton;
}

Array<Quat4f> ModelRotations(const SkeletonBindData& bindData, Span<const Transform> pose)
{
    Array<Quat4f> rotations;
    rotations.Resize(bindData.NumSlots());

    for (uint32 slot = 0; slot < bindData.NumSlots(); ++slot)
    {
        const uint32 parentSlot = bindData.parentSlots[slot];

        rotations[slot] = parentSlot != SkeletonBindData::InvalidSlot ? rotations[parentSlot] * pose[slot].GetRotation() : pose[slot].GetRotation();
    }

    return rotations;
}

// the pelvis walks 2 units forward over a second, bobbing up halfway
Handle<Animation> MakeRootMotionClip()
{
    Handle<Animation> clip = MakeHandle<Animation>(NAME("Stride"));

    Handle<AnimationTrack> track = MakeHandle<AnimationTrack>(NAME("pelvis"), NAME("pelvis"));

    const Keyframe keyframes[] = {
        Keyframe(0.0f, Transform(Vec3f(0.0f, 1.0f, 0.0f), Vec3f::One(), Quat4f::Identity())),
        Keyframe(0.5f, Transform(Vec3f(0.0f, 1.1f, 1.0f), Vec3f::One(), Quat4f::Identity())),
        Keyframe(1.0f, Transform(Vec3f(0.0f, 1.0f, 2.0f), Vec3f::One(), Quat4f::Identity()))
    };

    track->SetKeyframes(Span<const Keyframe>(keyframes, 3));

    clip->AddTrack(track);
    clip->SetRootMotion(true);

    return clip;
}

} // namespace

HYP_ENGINE_TEST(HumanoidPose, FootIKPlantsFeetOnOffsets)
{
    Handle<Skeleton> skeleton = BuildMannequin();
    HYP_TEST_REQUIRE(skeleton->IsHumanoid());

    const SkeletonBindData& bindData = skeleton->GetBindData();
    const Vec3f up = PoseOps::GetModelUp(*skeleton);

    HYP_TEST_CHECK((up - Vec3f::UnitY()).Length() < 1e-4f);

    const uint32 pelvis = bindData.FindSlot(NAME("pelvis"));
    const uint32 leftKnee = bindData.FindSlot(NAME("calf_l"));
    const uint32 leftFoot = bindData.FindSlot(NAME("foot_l"));
    const uint32 rightFoot = bindData.FindSlot(NAME("foot_r"));

    // straight legs: the left foot steps up, the right one down, so the pelvis comes down with it
    Array<Transform> pose = PoseOps::MakeRestPose(bindData);
    PoseOps::ApplyFootIK(*skeleton, up, 0.1f, -0.05f, pose.ToSpan());

    Array<Vec3f> joints = ModelPositions(bindData, pose.ToSpan());

    HYP_TEST_CHECK_MSG((joints[leftFoot] - Vec3f(-0.1f, 0.18f, 0.0f)).Length() < 5e-3f, "left foot at {}", joints[leftFoot]);
    HYP_TEST_CHECK_MSG((joints[rightFoot] - Vec3f(0.1f, 0.03f, 0.0f)).Length() < 5e-3f, "right foot at {}", joints[rightFoot]);
    HYP_TEST_CHECK(MathUtil::Abs(joints[pelvis].y - 0.95f) < 1e-4f);
    HYP_TEST_CHECK_MSG(joints[leftKnee].z > 0.05f, "left knee at {}", joints[leftKnee]);

    // the feet keep their rotation in model space
    const Array<Quat4f> rotations = ModelRotations(bindData, pose.ToSpan());

    HYP_TEST_CHECK(MathUtil::Abs(rotations[leftFoot].w) > 0.9999f);
    HYP_TEST_CHECK(MathUtil::Abs(rotations[rightFoot].w) > 0.9999f);

    // a bent leg bends further to reach higher
    PoseOps::ApplyFootIK(*skeleton, up, 0.05f, 0.0f, pose.ToSpan());
    joints = ModelPositions(bindData, pose.ToSpan());

    HYP_TEST_CHECK_MSG((joints[leftFoot] - Vec3f(-0.1f, 0.23f, 0.0f)).Length() < 5e-3f, "left foot at {}", joints[leftFoot]);
    HYP_TEST_CHECK(joints[leftKnee].z > 0.05f);
}

HYP_ENGINE_TEST(HumanoidPose, RootMotionMovesAcrossGround)
{
    Handle<Skeleton> skeleton = BuildMannequin();
    const SkeletonBindData& bindData = skeleton->GetBindData();
    const Vec3f up = PoseOps::GetModelUp(*skeleton);

    Handle<Animation> clip = MakeRootMotionClip();
    const ClipBinding binding = PoseOps::BindClip(*clip, *skeleton);

    // the bob up and down stays in the pose; only the ground movement comes out
    const Vec3f stride = PoseOps::GetRootMotion(*clip, binding, up, 0.25f, 0.75f, false);
    HYP_TEST_CHECK_MSG((stride - Vec3f(0.0f, 0.0f, 1.0f)).Length() < 1e-4f, "stride {}", stride);

    const Vec3f wrapped = PoseOps::GetRootMotion(*clip, binding, up, 0.75f, 0.25f, true);
    HYP_TEST_CHECK_MSG((wrapped - Vec3f(0.0f, 0.0f, 1.0f)).Length() < 1e-4f, "wrapped {}", wrapped);

    const Vec3f stepBack = PoseOps::GetRootMotion(*clip, binding, up, 0.75f, 0.25f, false);
    HYP_TEST_CHECK_MSG((stepBack - Vec3f(0.0f, 0.0f, -1.0f)).Length() < 1e-4f, "step back {}", stepBack);

    Array<Transform> pose = PoseOps::MakeRestPose(bindData);
    PoseOps::SampleClip(*clip, binding, bindData, 0.5f, pose.ToSpan());
    PoseOps::RemoveRootMotion(*clip, binding, up, 0.5f, pose.ToSpan());

    const uint32 pelvis = bindData.FindSlot(NAME("pelvis"));
    HYP_TEST_CHECK_MSG((pose[pelvis].GetTranslation() - Vec3f(0.0f, 1.1f, 0.0f)).Length() < 1e-4f, "pelvis at {}", pose[pelvis].GetTranslation());

    // through a graph, the instance reports the movement and plays the clip in place
    Handle<AnimationGraph> graph = MakeHandle<AnimationGraph>(NAME("RootMotionGraph"));

    Handle<AnimationClipNode> clipNode = MakeHandle<AnimationClipNode>();
    clipNode->clip = clip;
    graph->SetRootNode(graph->AddNode(clipNode));

    Handle<AnimationGraphInstance> instance = MakeHandle<AnimationGraphInstance>(graph);

    instance->Update(0.1f, *skeleton);
    instance->Update(0.25f, *skeleton);

    HYP_TEST_CHECK_MSG((instance->GetRootMotion() - Vec3f(0.0f, 0.0f, 0.5f)).Length() < 1e-4f, "graph root motion {}", instance->GetRootMotion());
    HYP_TEST_CHECK(instance->GetRootMotionWeight() == 1.0f);

    Array<Transform> graphPose;
    graphPose.Resize(bindData.NumSlots());
    instance->Evaluate(*skeleton, graphPose.ToSpan());

    HYP_TEST_CHECK_MSG(MathUtil::Abs(graphPose[pelvis].GetTranslation().z) < 1e-4f, "graph pelvis at {}", graphPose[pelvis].GetTranslation());
}

HYP_ENGINE_TEST(HumanoidPose, MotionDatabaseFollowsRootMotion)
{
    Handle<AssetRegistry> scratchRegistry = MakeHandle<AssetRegistry>(AssetRegistryId::Game, EngineGlobals::GetTempDirectory() / "HumanoidRetargetTests");
    scratchRegistry->Initialize(nullptr);

    GlobalContextScope registryScope { AssetRegistryContext { scratchRegistry } };

    Handle<Skeleton> skeleton = BuildMannequin();

    MotionDatabaseClip stride;
    stride.clip = MakeRootMotionClip();

    Handle<MotionDatabase> database = MakeHandle<MotionDatabase>(NAME("StrideMotion"));
    database->SetSkeleton(skeleton);
    database->SetClips({ stride });
    HYP_TEST_REQUIRE(database->Build());

    // halfway through, the trajectory's last point lies past the loop, where the stride carries on from its end
    const uint32 frame = database->FindFrame(0, 0.5f);
    HYP_TEST_REQUIRE(frame != ~0u);

    const Span<const float> features = database->GetFrameFeatures(frame);

    for (uint32 point = 0; point < MotionDatabase::NumTrajectoryPoints; ++point)
    {
        const float ahead = features[uint32(MotionFeature::TrajectoryPosition) + point * 2 + 1];

        HYP_TEST_CHECK_MSG(MathUtil::Abs(ahead - 2.0f * database->GetTrajectoryTime(point)) < 1e-3f, "point {} is {} ahead", point, ahead);
        HYP_TEST_CHECK(MathUtil::Abs(features[uint32(MotionFeature::TrajectoryPosition) + point * 2]) < 1e-3f);
    }

    // the feet ride along with the pelvis: the left one sits to the character's left and moves forward at the stride's speed
    HYP_TEST_CHECK_MSG(MathUtil::Abs(features[uint32(MotionFeature::LeftFootPosition)] + 0.1f) < 1e-3f, "left foot x {}", features[uint32(MotionFeature::LeftFootPosition)]);
    HYP_TEST_CHECK_MSG(MathUtil::Abs(features[uint32(MotionFeature::LeftFootVelocity) + 2] - 2.0f) < 1e-2f, "left foot speed {}", features[uint32(MotionFeature::LeftFootVelocity) + 2]);
    HYP_TEST_CHECK(MathUtil::Abs(features[uint32(MotionFeature::HipVelocity) + 2] - 2.0f) < 1e-2f);
}

} // namespace scene
} // namespace tests
} // namespace Hyperion

#endif // HYP_TESTS
