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
#include <Asset/ModelImport.hpp>

#include <Scene/Node.hpp>
#include <Scene/Entity.hpp>
#include <Scene/EntityManager.hpp>
#include <Scene/Prefab.hpp>
#include <Scene/Components/MeshComponent.hpp>
#include <Animation/AnimationComponent.hpp>
#include <Scene/Animation/Skeleton.hpp>
#include <Scene/Animation/Animation.hpp>
#include <Animation/AnimationModelImport.hpp>
#include <Animation/AnimationPose.hpp>

#include <Core/FileSystem/FilePath.hpp>

#include <Core/Utilities/GlobalContext.hpp>
#include <Core/Utilities/Optional.hpp>

#include <Core/Math/MathUtil.hpp>

namespace Hyperion {
namespace tests {
namespace scene {

namespace {

AssetImportInfo RigImportInfo(const char* fileName)
{
    AssetImportInfo importInfo;
    importInfo.sourcePath = (FilePath(__FILE__).BasePath() / ".." / ".." / ".." / ".." / "Tests" / "Engine" / "Data" / "Animation" / fileName);

    return importInfo;
}

Handle<AssetRegistry> MakeScratchRegistry()
{
    // imported assets persist their blobs into the current registry, so keep them out of the engine content
    Handle<AssetRegistry> scratchRegistry = MakeHandle<AssetRegistry>(AssetRegistryId::Game, EngineGlobals::GetTempDirectory() / "ModelImportTests");
    scratchRegistry->Initialize(nullptr);

    return scratchRegistry;
}

float LargestTransformDifference(const Transform& a, const Transform& b)
{
    const Mat4f matrixA = a.GetMatrix();
    const Mat4f matrixB = b.GetMatrix();

    float largest = 0.0f;

    for (uint32 element = 0; element < 16; ++element)
    {
        largest = MathUtil::Max(largest, MathUtil::Abs(matrixA.values[element] - matrixB.values[element]));
    }

    return largest;
}

struct SkinnedEntityCounts
{
    uint32 skinned = 0;
    uint32 animated = 0;
    uint32 playingFirstClip = 0;
};

SkinnedEntityCounts CountSkinnedEntities(const Prefab& prefab, const Handle<Animation>& firstClip)
{
    SkinnedEntityCounts counts;

    for (Node* descendant : prefab.GetRoot()->GetDescendants())
    {
        const Entity* entity = TryCast<Entity>(descendant);
        const MeshComponent* meshComponent = entity && entity->GetEntityManager() ? entity->TryGetComponent<MeshComponent>() : nullptr;

        if (!meshComponent || !meshComponent->skeleton.IsValid())
        {
            continue;
        }

        counts.skinned++;

        if (const AnimationComponent* animationComponent = entity->TryGetComponent<AnimationComponent>())
        {
            counts.animated++;

            if (animationComponent->animation.IsValid() && animationComponent->animation == firstClip)
            {
                counts.playingFirstClip++;
            }
        }
    }

    return counts;
}

} // namespace

HYP_ENGINE_TEST(ModelImport, ReimportKeepsAssetIdentities)
{
    Handle<AssetRegistry> registry = MakeScratchRegistry();
    GlobalContextScope registryScope { AssetRegistryContext { registry } };

    const AssetImportInfo importInfo = RigImportInfo("TwoTakeRig.fbx");

    auto firstImport = LoadModelForImport(importInfo, Handle<Skeleton>::empty);
    HYP_TEST_REQUIRE(firstImport.HasValue());

    ModelImportContents imported = firstImport.GetValue();
    HYP_TEST_REQUIRE(imported.prefab.IsValid() && imported.skeleton.IsValid());

    RegisterModelImport(*registry, imported);

    const Handle<Prefab> prefab = imported.prefab;
    const Handle<Skeleton> skeleton = imported.skeleton;
    const Handle<Animation> wave = skeleton->FindAnimation(NAME("Wave"));
    HYP_TEST_REQUIRE(wave.IsValid());

    HYP_TEST_CHECK(prefab->GetImportInfo().sourcePath == importInfo.sourcePath);
    HYP_TEST_CHECK(skeleton->GetImportInfo().sourcePath == importInfo.sourcePath);
    HYP_TEST_CHECK(wave->GetImportInfo().sourcePath == importInfo.sourcePath);

    const UUID prefabUUID = prefab->GetUUID();
    const UUID skeletonUUID = skeleton->GetUUID();
    const UUID waveUUID = wave->GetUUID();
    const uint32 prefabRevision = prefab->GetRevision();
    const uint32 skeletonRevision = skeleton->GetRevision();
    const uint32 waveRevision = wave->GetRevision();
    const uint32 numClips = uint32(skeleton->GetAnimations().Size());

    skeleton->SetSockets({ SkeletonSocket { NAME("Hand"), NAME("Tip"), Transform() } });
    wave->SetNotifies({ AnimationNotify { NAME("Step"), 0.25f, 0.0f } });

    auto secondImport = LoadModelForImport(importInfo, Handle<Skeleton>::empty);
    HYP_TEST_REQUIRE(secondImport.HasValue());

    ModelImportContents fresh = secondImport.GetValue();
    HYP_TEST_REQUIRE(fresh.skeleton.IsValid() && fresh.skeleton != skeleton);

    MergeModelReimport(*registry, fresh, prefab, skeleton);

    HYP_TEST_CHECK(fresh.prefab == prefab);
    HYP_TEST_CHECK(fresh.skeleton == skeleton);
    HYP_TEST_CHECK(prefab->GetUUID() == prefabUUID);
    HYP_TEST_CHECK(skeleton->GetUUID() == skeletonUUID);
    HYP_TEST_CHECK(prefab->GetRevision() == prefabRevision + 1);
    HYP_TEST_CHECK(skeleton->GetRevision() != skeletonRevision);

    HYP_TEST_CHECK(skeleton->GetAnimations().Size() == numClips);
    HYP_TEST_CHECK(skeleton->FindAnimation(NAME("Wave")) == wave);
    HYP_TEST_CHECK(wave->GetUUID() == waveUUID);
    HYP_TEST_CHECK(wave->GetRevision() != waveRevision);
    HYP_TEST_CHECK(wave->NumTracks() > 0 && skeleton->CountBoundTracks(wave) == wave->NumTracks());

    HYP_TEST_CHECK(skeleton->GetSockets().Size() == 1);
    HYP_TEST_CHECK(wave->GetNotifies().Size() == 1);

    HYP_TEST_CHECK(registry->GetAsset<Skeleton>(AssetBuckets::Skeletons, skeleton->GetName()) == skeleton);
    HYP_TEST_CHECK(registry->GetAsset<Prefab>(AssetBuckets::Prefabs, prefab->GetName()) == prefab);

    bool foundSkinnedEntity = false;

    for (Node* descendant : prefab->GetRoot()->GetDescendants())
    {
        const Entity* entity = TryCast<Entity>(descendant);
        const MeshComponent* meshComponent = entity && entity->GetEntityManager() ? entity->TryGetComponent<MeshComponent>() : nullptr;

        if (meshComponent && meshComponent->skeleton.IsValid())
        {
            foundSkinnedEntity = true;

            HYP_TEST_CHECK_MSG(meshComponent->skeleton == skeleton, "the reimported mesh should use the original skeleton");

            const AnimationComponent* animationComponent = entity->TryGetComponent<AnimationComponent>();
            HYP_TEST_CHECK_MSG(animationComponent && skeleton->GetAnimations().Contains(animationComponent->animation), "the reimported entity should play one of the original clips");
        }
    }

    HYP_TEST_CHECK(foundSkinnedEntity);
}

HYP_ENGINE_TEST(ModelImport, AnimationProcessorAddsComponents)
{
    Handle<AssetRegistry> registry = MakeScratchRegistry();
    GlobalContextScope registryScope { AssetRegistryContext { registry } };

    const AssetImportInfo importInfo = RigImportInfo("TwoTakeRig.glb");

    auto loadResult = g_assetManager->Load<Prefab>(importInfo.sourcePath, String::empty, AssetLoadHint::Transient);
    HYP_TEST_REQUIRE(loadResult.HasValue());

    const Handle<Prefab> loaded = loadResult->Result();
    HYP_TEST_REQUIRE(loaded.IsValid() && loaded->GetRoot().IsValid());

    const SkinnedEntityCounts loadedCounts = CountSkinnedEntities(*loaded, Handle<Animation>::empty);
    HYP_TEST_REQUIRE(loadedCounts.skinned > 0);
    HYP_TEST_CHECK_MSG(loadedCounts.animated == 0, "the loader added {} AnimationComponent(s); processors add them now", loadedCounts.animated);

    auto importResult = LoadModelForImport(importInfo, Handle<Skeleton>::empty);
    HYP_TEST_REQUIRE(importResult.HasValue());

    const ModelImportContents& contents = importResult.GetValue();
    HYP_TEST_REQUIRE(contents.prefab.IsValid() && contents.animations.Any());

    const SkinnedEntityCounts importedCounts = CountSkinnedEntities(*contents.prefab, contents.animations[0]);
    HYP_TEST_CHECK(importedCounts.skinned > 0);
    HYP_TEST_CHECK_MSG(importedCounts.playingFirstClip == importedCounts.skinned, "{} of {} skinned entities play the first clip", importedCounts.playingFirstClip, importedCounts.skinned);
}

HYP_ENGINE_TEST(ModelImport, AnimationProcessorFollowsSettings)
{
    Handle<AssetRegistry> registry = MakeScratchRegistry();
    GlobalContextScope registryScope { AssetRegistryContext { registry } };

    Handle<AnimationModelImportSettings> animationSettings = MakeHandle<AnimationModelImportSettings>();
    animationSettings->addAnimationComponent = false;

    AssetImportInfo withoutComponent = RigImportInfo("TwoTakeRig.glb");
    withoutComponent.settings.processorSettings = { animationSettings };

    auto withoutComponentResult = LoadModelForImport(withoutComponent, Handle<Skeleton>::empty);
    HYP_TEST_REQUIRE(withoutComponentResult.HasValue());
    HYP_TEST_REQUIRE(withoutComponentResult.GetValue().prefab.IsValid());

    const SkinnedEntityCounts withoutComponentCounts = CountSkinnedEntities(*withoutComponentResult.GetValue().prefab, Handle<Animation>::empty);
    HYP_TEST_CHECK(withoutComponentCounts.skinned > 0);
    HYP_TEST_CHECK(withoutComponentCounts.animated == 0);

    AssetImportInfo noRig = RigImportInfo("TwoTakeRig.glb");
    noRig.settings.rigMode = ImportRigMode::None;

    auto noRigResult = LoadModelForImport(noRig, Handle<Skeleton>::empty);
    HYP_TEST_REQUIRE(noRigResult.HasValue());
    HYP_TEST_REQUIRE(noRigResult.GetValue().prefab.IsValid());

    for (Node* descendant : noRigResult.GetValue().prefab->GetRoot()->GetDescendants())
    {
        const Entity* entity = TryCast<Entity>(descendant);

        if (entity && entity->GetEntityManager())
        {
            HYP_TEST_CHECK_MSG(!entity->HasComponent<AnimationComponent>(), "an import without a rig shouldn't animate '{}'", entity->GetName());
        }
    }
}

// the same rig exported both ways has to come in the same way round: FBX and glTF are right handed, and both loaders mirror into the engine's space
HYP_ENGINE_TEST(ModelImport, FbxAndGltfBendTheSameWay)
{
    Handle<AssetRegistry> registry = MakeScratchRegistry();
    GlobalContextScope registryScope { AssetRegistryContext { registry } };

    const auto bendTip = [](const char* fileName) -> Optional<Vec3f>
    {
        auto importResult = LoadModelForImport(RigImportInfo(fileName), Handle<Skeleton>::empty);

        if (!importResult.HasValue() || !importResult.GetValue().skeleton.IsValid())
        {
            return {};
        }

        const ModelImportContents& contents = importResult.GetValue();
        const SkeletonBindData& bindData = contents.skeleton->GetBindData();

        auto bend = contents.animations.FindIf([](const Handle<Animation>& clip)
            {
                return clip->GetName() == NAME("Bend");
            });

        const uint32 tipSlot = bindData.FindSlot(NAME("Tip"));

        if (bend == contents.animations.End() || tipSlot == SkeletonBindData::InvalidSlot)
        {
            return {};
        }

        Array<Transform> pose = Array<Transform>(PoseOps::GetRestPose(bindData));
        PoseOps::SampleClip(**bend, PoseOps::BindClip(**bend, bindData), bindData, (*bend)->GetLength() * 0.5f, pose.ToSpan());

        Array<Transform> model;
        model.Resize(bindData.NumSlots());

        for (uint32 slot = 0; slot < bindData.NumSlots(); ++slot)
        {
            const uint32 parent = bindData.parentSlots[slot];
            model[slot] = parent != SkeletonBindData::InvalidSlot ? model[parent] * pose[slot] : pose[slot];
        }

        return model[tipSlot].GetTranslation();
    };

    const Optional<Vec3f> gltfTip = bendTip("TwoTakeRig.glb");
    const Optional<Vec3f> fbxTip = bendTip("TwoTakeRig.fbx");
    HYP_TEST_REQUIRE(gltfTip.HasValue() && fbxTip.HasValue());

    // the FBX skeleton is in centimetres
    const Vec3f fbxTipInMetres = *fbxTip * 0.01f;

    HYP_TEST_CHECK_MSG(MathUtil::Abs(gltfTip->z) > 0.1f, "the glTF Bend should swing the tip off its axis, got ({}, {}, {})", gltfTip->x, gltfTip->y, gltfTip->z);
    HYP_TEST_CHECK_MSG((fbxTipInMetres - *gltfTip).Length() < 0.02f, "FBX Bend tip ({}, {}, {}) vs glTF ({}, {}, {})",
        fbxTipInMetres.x, fbxTipInMetres.y, fbxTipInMetres.z, gltfTip->x, gltfTip->y, gltfTip->z);
}

HYP_ENGINE_TEST(ModelImport, SplitTakeMakesClipLengths)
{
    Handle<AssetRegistry> registry = MakeScratchRegistry();
    GlobalContextScope registryScope { AssetRegistryContext { registry } };

    AssetImportInfo importInfo = RigImportInfo("TwoTakeRig.glb");
    importInfo.settings.frameRate = 30.0f;
    importInfo.settings.excludedTakes = { NAME("Bend") };
    importInfo.settings.splits = { AnimationClipSplit { NAME("WaveStart"), NAME("Wave"), 3, 9, false, true } };

    auto result = LoadModelForImport(importInfo, Handle<Skeleton>::empty);
    HYP_TEST_REQUIRE(result.HasValue());

    const ModelImportContents& contents = result.GetValue();
    HYP_TEST_REQUIRE(contents.skeleton.IsValid());

    HYP_TEST_CHECK(!contents.skeleton->FindAnimation(NAME("Bend")).IsValid());

    const Handle<Animation>& wave = contents.skeleton->FindAnimation(NAME("Wave"));
    const Handle<Animation>& waveStart = contents.skeleton->FindAnimation(NAME("WaveStart"));
    HYP_TEST_REQUIRE(wave.IsValid() && waveStart.IsValid());

    HYP_TEST_CHECK(contents.skeleton->GetAnimations().Size() == 2);
    HYP_TEST_CHECK_MSG(MathUtil::Abs(waveStart->GetLength() - 0.2f) < 1e-4f, "split length {}, expected 0.2", waveStart->GetLength());
    HYP_TEST_CHECK(!waveStart->IsLooping());
    HYP_TEST_CHECK(waveStart->HasRootMotion());
    HYP_TEST_CHECK(waveStart->NumTracks() == wave->NumTracks());

    for (uint32 trackIndex = 0; trackIndex < waveStart->NumTracks(); ++trackIndex)
    {
        const AnimationTrack& sourceTrack = *wave->GetTrack(trackIndex);
        const AnimationTrack& splitTrack = *waveStart->GetTrack(trackIndex);

        HYP_TEST_CHECK(splitTrack.GetBoneName() == sourceTrack.GetBoneName());

        for (const float splitTime : { 0.0f, 0.1f, 0.2f })
        {
            const float difference = LargestTransformDifference(splitTrack.GetKeyframe(splitTime).transform, sourceTrack.GetKeyframe(0.1f + splitTime).transform);

            HYP_TEST_CHECK_MSG(difference < 1e-3f, "track {} at {}: off by {}", splitTrack.GetBoneName(), splitTime, difference);
        }
    }
}

HYP_ENGINE_TEST(ModelImport, AnimationOnlyImportUsesExistingSkeleton)
{
    Handle<AssetRegistry> registry = MakeScratchRegistry();
    GlobalContextScope registryScope { AssetRegistryContext { registry } };

    auto rigImport = LoadModelForImport(RigImportInfo("TwoTakeRig.fbx"), Handle<Skeleton>::empty);
    HYP_TEST_REQUIRE(rigImport.HasValue());

    ModelImportContents rig = rigImport.GetValue();
    RegisterModelImport(*registry, rig);

    const Handle<Skeleton> skeleton = rig.skeleton;
    const uint32 numRigClips = uint32(skeleton->GetAnimations().Size());

    AssetImportInfo clipsInfo = RigImportInfo("TwoTakeRig.glb");
    clipsInfo.settings.rigMode = ImportRigMode::UseExisting;
    clipsInfo.settings.targetSkeleton = skeleton->GetName();

    auto clipsImport = LoadModelForImport(clipsInfo, skeleton);
    HYP_TEST_REQUIRE(clipsImport.HasValue());

    ModelImportContents clips = clipsImport.GetValue();

    HYP_TEST_CHECK(clips.IsAnimationOnly());
    HYP_TEST_CHECK(clips.skeleton == skeleton);
    HYP_TEST_CHECK_MSG(clips.numUnmatchedTracks == 0, "{} tracks found no bone on the target skeleton", clips.numUnmatchedTracks);

    RegisterModelImport(*registry, clips);

    HYP_TEST_CHECK(skeleton->GetAnimations().Size() == numRigClips + clips.animations.Size());

    for (const Handle<Animation>& clip : clips.animations)
    {
        HYP_TEST_CHECK(clip->IsRegistered());
        HYP_TEST_CHECK(skeleton->GetAnimations().Contains(clip));
        HYP_TEST_CHECK(skeleton->CountBoundTracks(clip) == clip->NumTracks());
    }
}

} // namespace scene
} // namespace tests
} // namespace Hyperion

#endif // HYP_TESTS
