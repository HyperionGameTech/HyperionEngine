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
#include <Scene/Animation/Skeleton.hpp>
#include <Scene/Animation/Animation.hpp>

#include <Core/FileSystem/FilePath.hpp>

#include <Core/Utilities/GlobalContext.hpp>

#include <Core/Math/MathUtil.hpp>

namespace Hyperion {
namespace tests {
namespace scene {

namespace {

AssetImportInfo RigImportInfo(const char* fileName)
{
    AssetImportInfo importInfo;
    importInfo.sourcePath = (FilePath(__FILE__).BasePath() / ".." / "Data" / "Animation" / fileName);

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
        }
    }

    HYP_TEST_CHECK(foundSkinnedEntity);
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
