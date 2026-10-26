/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#ifdef HYP_TESTS

#include <Framework/Testing/EngineTest.hpp>
#include <Framework/EngineGlobals.hpp>

#include <Asset/Assets.hpp>
#include <Asset/AssetRegistry.hpp>

#include <Scene/Node.hpp>
#include <Scene/Entity.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/Prefab.hpp>
#include <Scene/Components/MeshComponent.hpp>
#include <Animation/AnimationComponent.hpp>
#include <Scene/Animation/Skeleton.hpp>
#include <Scene/Animation/Bone.hpp>
#include <Scene/Animation/Animation.hpp>
#include <Animation/AnimationPose.hpp>
#include <Animation/BoneMask.hpp>

#include <Core/FileSystem/FilePath.hpp>

#include <Core/Utilities/GlobalContext.hpp>

#include <Core/Math/MathUtil.hpp>
#include <Core/Math/Quat4f.hpp>

#include <Core/Util.hpp>

#include <Core/Logging/Logger.hpp>
#include <Core/Logging/LogChannels.hpp>

#include <chrono>

namespace Hyperion {
namespace tests {
namespace scene {

namespace {

FilePath PoseTestDataPath(const char* fileName)
{
    return FilePath(__FILE__).BasePath() / ".." / ".." / ".." / ".." / "Tests" / "Engine" / "Data" / "Animation" / fileName;
}

Handle<Skeleton> LoadRigSkeleton(EngineTestContext& context, const char* fileName)
{
    Handle<AssetRegistry> scratchRegistry = MakeHandle<AssetRegistry>(AssetRegistryId::Game, EngineGlobals::GetTempDirectory() / "AnimationPoseTests");
    scratchRegistry->Initialize(nullptr);

    GlobalContextScope registryScope { AssetRegistryContext { scratchRegistry } };

    auto loadResult = g_assetManager->Load<Prefab>(PoseTestDataPath(fileName), String::empty, AssetLoadHint::Transient);

    if (!HYP_TEST_CHECK(loadResult.HasValue()))
    {
        return Handle<Skeleton>::empty;
    }

    Handle<Prefab> prefab = loadResult->Result();

    for (Node* descendant : prefab->GetRoot()->GetDescendants())
    {
        Entity* entity = TryCast<Entity>(descendant);

        if (!entity || !entity->GetEntityManager())
        {
            continue;
        }

        if (const MeshComponent* meshComponent = entity->TryGetComponent<MeshComponent>(); meshComponent && meshComponent->skeleton.IsValid())
        {
            return meshComponent->skeleton;
        }
    }

    HYP_TEST_CHECK_MSG(false, "{}: no skinned mesh", fileName);

    return Handle<Skeleton>::empty;
}

Animation* FindRigAnimation(const Skeleton& skeleton, Name name)
{
    for (const Handle<Animation>& animation : skeleton.GetAnimations())
    {
        if (animation.IsValid() && animation->GetName() == name)
        {
            return animation.Get();
        }
    }

    return nullptr;
}

Array<Mat4f> NodeSkinningMatrices(const Skeleton& skeleton)
{
    Array<Mat4f> matrices;
    matrices.PushBack(skeleton.GetRootBone()->GetBoneMatrix());

    for (Node* descendant : skeleton.GetRootBone()->GetDescendants())
    {
        const Bone* bone = TryCast<Bone>(descendant);
        matrices.PushBack(bone ? bone->GetBoneMatrix() : Mat4f::Identity());
    }

    return matrices;
}

float LargestDifference(const Mat4f& a, const Mat4f& b)
{
    float largest = 0.0f;

    for (uint32 element = 0; element < 16; ++element)
    {
        largest = MathUtil::Max(largest, MathUtil::Abs(a.values[element] - b.values[element]));
    }

    return largest;
}

// the node-based twist AnimationSystem used before poses
void NodeTwist(Skeleton& skeleton, Name rootBoneName, Name endBoneName, float angle)
{
    Bone* endBone = skeleton.FindBone(endBoneName);

    Array<Bone*> chain;

    for (Node* node = endBone; node != nullptr; node = node->GetParent())
    {
        Bone* bone = TryCast<Bone>(node);

        if (!bone)
        {
            return;
        }

        chain.PushBack(bone);

        if (bone->GetBoneName() == rootBoneName)
        {
            break;
        }
    }

    const Quat4f shareRotation(Vec3f::UnitY(), angle / float(chain.Size()));

    for (size_t index = chain.Size(); index > 0; --index)
    {
        chain[index - 1]->SetWorldRotation(chain[index - 1]->GetWorldRotation() * shareRotation);
    }
}

void CheckPoseMatchesNodes(EngineTestContext& context, const char* fileName)
{
    Handle<Skeleton> skeleton = LoadRigSkeleton(context, fileName);
    HYP_TEST_REQUIRE(skeleton.IsValid());

    Animation* wave = FindRigAnimation(*skeleton, NAME("Wave"));
    Animation* bend = FindRigAnimation(*skeleton, NAME("Bend"));
    HYP_TEST_REQUIRE(wave && bend);

    const SkeletonBindData& bindData = skeleton->GetBindData();
    HYP_TEST_REQUIRE(bindData.NumSlots() == uint32(NodeSkinningMatrices(*skeleton).Size()));

    const ClipBinding waveBinding = PoseOps::BindClip(*wave, bindData);
    const ClipBinding bendBinding = PoseOps::BindClip(*bend, bindData);

    Array<Mat4f> modelMatrices;
    Array<Mat4f> skinningMatrices;
    modelMatrices.Resize(bindData.NumSlots());
    skinningMatrices.Resize(bindData.NumSlots());

    const auto compare = [&](const char* step, uint32 frame)
    {
        const Array<Mat4f> nodeMatrices = NodeSkinningMatrices(*skeleton);

        float largest = 0.0f;
        float magnitude = 0.0f;

        for (uint32 slot = 0; slot < bindData.NumSlots(); ++slot)
        {
            if (bindData.boneNames[slot].IsValid())
            {
                largest = MathUtil::Max(largest, LargestDifference(nodeMatrices[slot], skinningMatrices[slot]));
                magnitude = MathUtil::Max(magnitude, LargestDifference(nodeMatrices[slot], Mat4f::Zeros()));
            }
        }

        // FBX rigs keep their unit scale in model space, so the matrices can run into the hundreds
        const float tolerance = MathUtil::Max(1e-3f, magnitude * 1e-4f);

        HYP_TEST_CHECK_MSG(largest < tolerance, "{}: {} frame {} differs from the node pose by {} (tolerance {})", fileName, step, frame, largest, tolerance);
    };

    Array<Transform> pose = PoseOps::MakeRestPose(bindData);
    skeleton->GetRootBone()->SetToBindingPose();

    for (uint32 frame = 0; frame < 12; ++frame)
    {
        ApplyAnimParams params {};
        params.time = float(frame) * 0.1f;
        params.blend = 0.5f;

        PoseOps::ApplyBlended(*wave, waveBinding, bindData, params, pose.ToSpan());
        wave->ApplyBlended(*skeleton, params);

        PoseOps::ComputeMatrices(bindData, pose.ToSpan(), modelMatrices.ToSpan(), skinningMatrices.ToSpan());
        compare("ApplyBlended", frame);
    }

    pose = PoseOps::MakeRestPose(bindData);
    skeleton->GetRootBone()->SetToBindingPose();

    for (uint32 frame = 0; frame < 12; ++frame)
    {
        ApplyAnimParams params {};
        params.time = float(frame) * 0.13f;
        params.blend = 0.5f;
        params.layerAnimation = bend;
        params.layerTime = float(frame) * 0.17f;
        params.layerWeight = 0.6f;
        params.layerExcludedBone = NAME("Tip");
        params.secondLayerAnimation = bend;
        params.secondLayerTime = float(frame) * 0.07f;
        params.secondLayerWeight = 0.3f;

        PoseOps::ApplyLayered(*wave, waveBinding, &bendBinding, &bendBinding, bindData, params, pose.ToSpan());
        wave->ApplyLayered(*skeleton, params);

        Array<Transform> renderPose = pose;
        PoseOps::ApplyTwist(bindData, NAME("Root"), NAME("Tip"), 0.4f, renderPose.ToSpan());
        NodeTwist(*skeleton, NAME("Root"), NAME("Tip"), 0.4f);

        PoseOps::ComputeMatrices(bindData, renderPose.ToSpan(), modelMatrices.ToSpan(), skinningMatrices.ToSpan());
        compare("ApplyLayered with twist", frame);

        // the node twist lives in the local transforms, so the next blend has to start from the keyframes like the pose does
        for (uint32 slot = 0; slot < bindData.NumSlots(); ++slot)
        {
            if (Bone* bone = skeleton->FindBone(bindData.boneNames[slot]); bone && bindData.boneNames[slot].IsValid())
            {
                bone->SetKeyframe(bone->GetKeyframe());
            }
        }
    }
}

} // namespace

HYP_ENGINE_TEST(AnimationPose, FlatPoseMatchesNodePoseGltf)
{
    CheckPoseMatchesNodes(context, "TwoTakeRig.glb");
}

HYP_ENGINE_TEST(AnimationPose, FlatPoseMatchesNodePoseFbx)
{
    CheckPoseMatchesNodes(context, "TwoTakeRig.fbx");
}

HYP_ENGINE_TEST(AnimationPose, InstancesPoseIndependently)
{
    Handle<Skeleton> skeleton = LoadRigSkeleton(context, "TwoTakeRig.glb");
    HYP_TEST_REQUIRE(skeleton.IsValid());

    Handle<Skeleton> first = skeleton->CreateInstance();
    Handle<Skeleton> second = skeleton->CreateInstance();
    HYP_TEST_REQUIRE(first.IsValid() && second.IsValid());

    HYP_TEST_CHECK(first->GetInstanceSource() == skeleton.Get());
    HYP_TEST_CHECK(first->GetRootBone().Get() != skeleton->GetRootBone().Get());
    HYP_TEST_CHECK(first->GetRootBone().Get() != second->GetRootBone().Get());
    HYP_TEST_CHECK(first->GetAnimations().Size() == skeleton->GetAnimations().Size());
    HYP_TEST_CHECK(first->GetBindData().NumSlots() == skeleton->GetBindData().NumSlots());

    for (uint32 slot = 0; slot < skeleton->GetBindData().NumSlots(); ++slot)
    {
        HYP_TEST_CHECK(first->GetBindData().boneNames[slot] == skeleton->GetBindData().boneNames[slot]);
        HYP_TEST_CHECK(LargestDifference(first->GetBindData().inverseBindMatrices[slot], skeleton->GetBindData().inverseBindMatrices[slot]) < 1e-4f);
    }

    const SkeletonBindData& bindData = skeleton->GetBindData();
    Animation* wave = FindRigAnimation(*skeleton, NAME("Wave"));
    HYP_TEST_REQUIRE(wave != nullptr);

    Array<Transform> pose = PoseOps::MakeRestPose(bindData);

    ApplyAnimParams params {};
    params.time = 0.5f;
    params.blend = 1.0f;
    PoseOps::ApplyBlended(*wave, PoseOps::BindClip(*wave, bindData), bindData, params, pose.ToSpan());

    Array<Mat4f> modelMatrices;
    Array<Mat4f> skinningMatrices;
    modelMatrices.Resize(bindData.NumSlots());
    skinningMatrices.Resize(bindData.NumSlots());
    PoseOps::ComputeMatrices(bindData, pose.ToSpan(), modelMatrices.ToSpan(), skinningMatrices.ToSpan());

    first->SetPose(pose.ToSpan());

    HYP_TEST_CHECK(first->GetPoseModelMatrices().Size() == bindData.NumSlots());
    HYP_TEST_CHECK(second->GetPoseModelMatrices().Size() == 0);
    HYP_TEST_CHECK(skeleton->GetPoseModelMatrices().Size() == 0);

    // the instance works out the same matrices the pose ops do
    for (uint32 slot = 0; slot < bindData.NumSlots(); ++slot)
    {
        HYP_TEST_CHECK(LargestDifference(first->GetPoseModelMatrices()[slot], modelMatrices[slot]) < 1e-5f);
    }

    // a pose of the wrong size shows the bind pose rather than reading past it
    first->SetPose(Span<const Transform>(pose.Data(), 1));
    HYP_TEST_CHECK(first->GetPoseModelMatrices().Size() == 0);

    // a change to the source's bones leaves its instances stale
    HYP_TEST_CHECK(first->IsInstanceCurrent());

    const Handle<Bone> rootBone = skeleton->GetRootBone();
    skeleton->SetRootBone(rootBone);

    HYP_TEST_CHECK(!first->IsInstanceCurrent());
}

HYP_ENGINE_TEST(AnimationPose, KeyframeLookupMatchesLinearScan)
{
    Handle<Skeleton> skeleton = LoadRigSkeleton(context, "TwoTakeRig.glb");
    HYP_TEST_REQUIRE(skeleton.IsValid());

    for (const Handle<Animation>& animation : skeleton->GetAnimations())
    {
        for (const Handle<AnimationTrack>& track : animation->GetTracks())
        {
            const Span<const Keyframe> keyframes = track->GetKeyframes();

            // the lookup GetKeyframe() did before it searched
            const auto sampleLinearly = [&keyframes](float time) -> Transform
            {
                if (time >= keyframes[keyframes.Size() - 1].time)
                {
                    return keyframes[keyframes.Size() - 1].transform;
                }

                int first = 0;
                int second = -1;

                for (int index = 0; index < int(keyframes.Size()) - 1; index++)
                {
                    if (MathUtil::InRange(time, { keyframes[index].time, keyframes[index + 1].time }))
                    {
                        first = index;
                        second = index + 1;

                        break;
                    }
                }

                Transform transform = keyframes[first].transform;

                if (second > first)
                {
                    const float delta = (time - keyframes[first].time) / (keyframes[second].time - keyframes[first].time);

                    transform.translation = transform.translation.Lerp(keyframes[second].transform.translation, delta);
                    transform.scale = transform.scale.Lerp(keyframes[second].transform.scale, delta);
                    transform.rotation = transform.rotation.Slerp(keyframes[second].transform.rotation, delta);
                }

                return transform;
            };

            for (size_t index = 0; index + 1 < keyframes.Size(); ++index)
            {
                for (const float fraction : { 0.0f, 0.3f, 0.5f, 1.0f })
                {
                    const float time = MathUtil::Lerp(keyframes[index].time, keyframes[index + 1].time, fraction);

                    const Transform expected = sampleLinearly(time);
                    const Transform sampled = track->GetKeyframe(time).transform;

                    const float rotationDifference = MathUtil::Max(
                        MathUtil::Max(MathUtil::Abs(sampled.rotation.x - expected.rotation.x), MathUtil::Abs(sampled.rotation.y - expected.rotation.y)),
                        MathUtil::Max(MathUtil::Abs(sampled.rotation.z - expected.rotation.z), MathUtil::Abs(sampled.rotation.w - expected.rotation.w)));

                    HYP_TEST_CHECK_MSG((sampled.translation - expected.translation).Length() < 1e-5f && rotationDifference < 1e-5f,
                        "{} key {} at {}: sampled {} {} expected {} {}", track->GetName(), index, fraction,
                        sampled.translation, sampled.rotation, expected.translation, expected.rotation);
                }
            }
        }
    }
}

HYP_ENGINE_TEST(AnimationPose, MeasureFlatPoseCost)
{
    static constexpr uint32 NumBones = 64;
    static constexpr uint32 BranchLength = 8;
    static constexpr uint32 NumKeys = 30;
    static constexpr uint32 NumFrames = 300;

    Handle<Bone> rootBone = MakeHandle<Bone>(NAME("Root"));
    Handle<Animation> animation = MakeHandle<Animation>(NAME("Synthetic"));

    Bone* parentBone = rootBone.Get();

    for (uint32 boneIndex = 0; boneIndex < NumBones; ++boneIndex)
    {
        const Name boneName = NAME_FMT("Bone{}", boneIndex);
        const Transform restTransform(Vec3f(0.0f, 0.1f, 0.0f), Vec3f::One(), Quat4f::Identity());

        Handle<Bone> bone = MakeHandle<Bone>(boneName);
        bone->SetBindingTransform(restTransform);
        bone->SetLocalTransform(restTransform);

        (boneIndex % BranchLength == 0 ? rootBone.Get() : parentBone)->AddChild(bone);
        parentBone = bone.Get();

        Array<Keyframe> keyframes;

        for (uint32 keyIndex = 0; keyIndex < NumKeys; ++keyIndex)
        {
            const float angle = MathUtil::Sin(float(keyIndex) * 0.2f + float(boneIndex)) * 0.3f;
            keyframes.PushBack(Keyframe(float(keyIndex) / 30.0f, Transform(restTransform.GetTranslation(), Vec3f::One(), Quat4f(Vec3f::UnitX(), angle))));
        }

        Handle<AnimationTrack> track = MakeHandle<AnimationTrack>(NAME_FMT("Synthetic_{}", boneName), boneName);
        track->SetKeyframes(keyframes.ToSpan());

        animation->AddTrack(track);
    }

    Handle<Skeleton> skeleton = MakeHandle<Skeleton>();
    skeleton->SetRootBone(rootBone);
    InitObject(skeleton);

    const SkeletonBindData& bindData = skeleton->GetBindData();
    const ClipBinding binding = PoseOps::BindClip(*animation, bindData);

    Array<Transform> pose = PoseOps::MakeRestPose(bindData);
    Array<Mat4f> modelMatrices;
    Array<Mat4f> skinningMatrices;
    modelMatrices.Resize(bindData.NumSlots());
    skinningMatrices.Resize(bindData.NumSlots());

    Array<Mat4f> nodeMatrices;

    const auto timeFrames = [&](auto&& evaluateFrame)
    {
        const auto start = std::chrono::high_resolution_clock::now();

        for (uint32 frame = 0; frame < NumFrames; ++frame)
        {
            ApplyAnimParams params {};
            params.time = float(frame % NumKeys) / 30.0f;
            params.blend = 0.5f;

            evaluateFrame(params);
        }

        return std::chrono::duration<double, std::micro>(std::chrono::high_resolution_clock::now() - start).count() / double(NumFrames);
    };

    const double nodeMicroseconds = timeFrames([&](const ApplyAnimParams& params)
        {
            animation->ApplyBlended(*skeleton, params);
            nodeMatrices = NodeSkinningMatrices(*skeleton);
        });

    const double flatMicroseconds = timeFrames([&](const ApplyAnimParams& params)
        {
            PoseOps::ApplyBlended(*animation, binding, bindData, params, pose.ToSpan());
            PoseOps::ComputeMatrices(bindData, pose.ToSpan(), modelMatrices.ToSpan(), skinningMatrices.ToSpan());
        });

    HYP_LOG(Animation, Info, "Pose evaluation for {} bones: nodes {} us/frame, flat {} us/frame", NumBones, nodeMicroseconds, flatMicroseconds);

    float largest = 0.0f;

    for (uint32 slot = 0; slot < bindData.NumSlots(); ++slot)
    {
        largest = MathUtil::Max(largest, LargestDifference(nodeMatrices[slot], skinningMatrices[slot]));
    }

    HYP_TEST_CHECK_MSG(largest < 1e-3f, "synthetic rig: flat pose differs from the node pose by {}", largest);
}

HYP_ENGINE_TEST(AnimationPose, CrossFadeReachesClipLinearly)
{
    Handle<Skeleton> skeleton = LoadRigSkeleton(context, "TwoTakeRig.glb");
    HYP_TEST_REQUIRE(skeleton.IsValid());

    const SkeletonBindData& bindData = skeleton->GetBindData();
    Animation* bend = FindRigAnimation(*skeleton, NAME("Bend"));
    HYP_TEST_REQUIRE(bend != nullptr);

    const ClipBinding binding = PoseOps::BindClip(*bend, bindData);

    ApplyAnimParams params {};
    params.time = bend->GetLength() * 0.5f;

    const auto poseAfter = [&](float blend)
    {
        Array<Transform> pose = PoseOps::MakeRestPose(bindData);
        params.blend = blend;
        PoseOps::ApplyBlended(*bend, binding, bindData, params, pose.ToSpan());
        return pose;
    };

    constexpr float FadeDuration = 0.5f;
    constexpr float Delta = 1.0f / 60.0f;
    constexpr uint32 NumSteps = 30;

    Array<Transform> fadingPose = PoseOps::MakeRestPose(bindData);
    float remaining = FadeDuration;

    for (uint32 step = 0; step < NumSteps; ++step)
    {
        if (step == NumSteps / 2)
        {
            const Array<Transform> halfway = poseAfter(0.5f);

            for (uint32 slot = 0; slot < bindData.NumSlots(); ++slot)
            {
                HYP_TEST_CHECK(LargestDifference(fadingPose[slot].GetMatrix(), halfway[slot].GetMatrix()) < 1e-3f);
            }
        }

        params.blend = PoseOps::AdvanceCrossFade(Delta, remaining, 0.5f);
        PoseOps::ApplyBlended(*bend, binding, bindData, params, fadingPose.ToSpan());
    }

    HYP_TEST_CHECK(remaining == 0.0f);

    const Array<Transform> target = poseAfter(1.0f);

    for (uint32 slot = 0; slot < bindData.NumSlots(); ++slot)
    {
        HYP_TEST_CHECK(LargestDifference(fadingPose[slot].GetMatrix(), target[slot].GetMatrix()) < 1e-4f);
    }

    float noFade = 0.0f;
    HYP_TEST_CHECK(PoseOps::AdvanceCrossFade(Delta, noFade, 0.5f) == 0.5f);
}

HYP_ENGINE_TEST(AnimationPose, BoneMaskMatchesExcludedBone)
{
    Handle<Skeleton> skeleton = LoadRigSkeleton(context, "TwoTakeRig.glb");
    HYP_TEST_REQUIRE(skeleton.IsValid());

    const SkeletonBindData& bindData = skeleton->GetBindData();
    Animation* wave = FindRigAnimation(*skeleton, NAME("Wave"));
    Animation* bend = FindRigAnimation(*skeleton, NAME("Bend"));
    HYP_TEST_REQUIRE(wave != nullptr && bend != nullptr);

    const ClipBinding waveBinding = PoseOps::BindClip(*wave, bindData);
    const ClipBinding bendBinding = PoseOps::BindClip(*bend, bindData);

    ApplyAnimParams params {};
    params.time = wave->GetLength() * 0.5f;
    params.blend = 1.0f;
    params.layerAnimation = bend;
    params.layerTime = bend->GetLength() * 0.5f;
    params.layerWeight = 0.8f;
    params.layerExcludedBone = NAME("Mid");

    Array<Transform> excludedPose = PoseOps::MakeRestPose(bindData);
    PoseOps::ApplyLayered(*wave, waveBinding, &bendBinding, nullptr, bindData, params, excludedPose.ToSpan());

    const Array<float> maskWeights = BoneMask::BuildExcludingSubtree(bindData, NAME("Mid"));
    params.layerExcludedBone = Name::Invalid();
    params.layerBoneWeights = maskWeights.ToSpan();

    Array<Transform> maskedPose = PoseOps::MakeRestPose(bindData);
    PoseOps::ApplyLayered(*wave, waveBinding, &bendBinding, nullptr, bindData, params, maskedPose.ToSpan());

    for (uint32 slot = 0; slot < bindData.NumSlots(); ++slot)
    {
        HYP_TEST_CHECK(LargestDifference(excludedPose[slot].GetMatrix(), maskedPose[slot].GetMatrix()) < 1e-6f);
    }
}

HYP_ENGINE_TEST(AnimationPose, BoneMaskWeightsEachBone)
{
    Handle<Skeleton> skeleton = LoadRigSkeleton(context, "TwoTakeRig.glb");
    HYP_TEST_REQUIRE(skeleton.IsValid());

    const SkeletonBindData& bindData = skeleton->GetBindData();
    Animation* wave = FindRigAnimation(*skeleton, NAME("Wave"));
    Animation* bend = FindRigAnimation(*skeleton, NAME("Bend"));
    HYP_TEST_REQUIRE(wave != nullptr && bend != nullptr);

    const uint32 midSlot = bindData.FindSlot(NAME("Mid"));
    HYP_TEST_REQUIRE(midSlot != SkeletonBindData::InvalidSlot);

    const ClipBinding waveBinding = PoseOps::BindClip(*wave, bindData);
    const ClipBinding bendBinding = PoseOps::BindClip(*bend, bindData);

    Handle<BoneMask> mask = MakeHandle<BoneMask>(NAME("HalfMid"));
    mask->SetDefaultWeight(1.0f);
    mask->SetEntries({ BoneMaskEntry { NAME("Mid"), 0.5f, false } });

    const Array<float> maskWeights = mask->BuildSlotWeights(bindData);
    HYP_TEST_REQUIRE(maskWeights.Size() == bindData.NumSlots());

    for (uint32 slot = 0; slot < bindData.NumSlots(); ++slot)
    {
        HYP_TEST_CHECK(maskWeights[slot] == (slot == midSlot ? 0.5f : 1.0f));
    }

    const auto layeredPose = [&](float layerWeight, Span<const float> boneWeights)
    {
        ApplyAnimParams params {};
        params.time = wave->GetLength() * 0.5f;
        params.blend = 1.0f;
        params.layerAnimation = bend;
        params.layerTime = bend->GetLength() * 0.5f;
        params.layerWeight = layerWeight;
        params.layerBoneWeights = boneWeights;

        Array<Transform> pose = PoseOps::MakeRestPose(bindData);
        PoseOps::ApplyLayered(*wave, waveBinding, &bendBinding, nullptr, bindData, params, pose.ToSpan());

        return pose;
    };

    const Array<Transform> maskedPose = layeredPose(1.0f, maskWeights.ToSpan());
    const Array<Transform> halfPose = layeredPose(0.5f, {});
    const Array<Transform> fullPose = layeredPose(1.0f, {});

    for (uint32 slot = 0; slot < bindData.NumSlots(); ++slot)
    {
        const Transform& expected = slot == midSlot ? halfPose[slot] : fullPose[slot];

        HYP_TEST_CHECK_MSG(LargestDifference(maskedPose[slot].GetMatrix(), expected.GetMatrix()) < 1e-5f, "slot {} differs", slot);
    }

    mask->SetEntries({ BoneMaskEntry { NAME("Mid"), 0.25f, true } });

    const Array<float> subtreeWeights = mask->BuildSlotWeights(bindData);

    for (uint32 slot = 0; slot < bindData.NumSlots(); ++slot)
    {
        HYP_TEST_CHECK(subtreeWeights[slot] == (bindData.IsSlotWithin(slot, NAME("Mid")) ? 0.25f : 1.0f));
    }
}

HYP_ENGINE_TEST(AnimationPose, MeshComponentLayoutMatchesManaged)
{
#ifndef HYP_WEB
    HYP_TEST_CHECK_MSG(HYP_OFFSET_OF(MeshComponent, skeletonInstance) == 168, "skeletonInstance at {}, MeshComponent.cs expects 168", HYP_OFFSET_OF(MeshComponent, skeletonInstance));
    HYP_TEST_CHECK_MSG(sizeof(MeshComponent) == 176, "MeshComponent is {} bytes, MeshComponent.cs expects 176", sizeof(MeshComponent));
#endif
}

HYP_ENGINE_TEST(AnimationPose, AnimationComponentLayoutMatchesManaged)
{
    HYP_TEST_CHECK_MSG(sizeof(AnimationPlaybackState) == 72, "AnimationPlaybackState is {} bytes, AnimationComponent.cs expects 72", sizeof(AnimationPlaybackState));
    HYP_TEST_CHECK_MSG(HYP_OFFSET_OF(AnimationComponent, animation) == 72, "animation at {}, AnimationComponent.cs expects 72", HYP_OFFSET_OF(AnimationComponent, animation));
    HYP_TEST_CHECK_MSG(HYP_OFFSET_OF(AnimationComponent, layerMask) == 80, "layerMask at {}, AnimationComponent.cs expects 80", HYP_OFFSET_OF(AnimationComponent, layerMask));
    HYP_TEST_CHECK_MSG(HYP_OFFSET_OF(AnimationComponent, graph) == 88, "graph at {}, AnimationComponent.cs expects 88", HYP_OFFSET_OF(AnimationComponent, graph));
    HYP_TEST_CHECK_MSG(HYP_OFFSET_OF(AnimationComponent, graphInstance) == 96, "graphInstance at {}, AnimationComponent.cs expects 96", HYP_OFFSET_OF(AnimationComponent, graphInstance));
    HYP_TEST_CHECK_MSG(sizeof(AnimationComponent) == 112, "AnimationComponent is {} bytes, AnimationComponent.cs expects 112", sizeof(AnimationComponent));
}

} // namespace scene
} // namespace tests
} // namespace Hyperion

#endif // HYP_TESTS
