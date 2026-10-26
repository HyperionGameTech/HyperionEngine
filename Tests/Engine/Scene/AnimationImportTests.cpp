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
#include <Scene/Animation/Skeleton.hpp>
#include <Scene/Animation/Bone.hpp>
#include <Scene/Animation/Animation.hpp>

#include <Rendering/Mesh.hpp>
#include <Rendering/Vertex.hpp>

#include <Core/FileSystem/FilePath.hpp>

#include <Core/Utilities/GlobalContext.hpp>

#include <Core/Math/MathUtil.hpp>
#include <Core/Math/Quat4f.hpp>
#include <Core/Math/BoundingBox.hpp>

namespace Hyperion {
namespace tests {
namespace scene {

namespace {

FilePath AnimationDataPath(const char* fileName)
{
    return FilePath(__FILE__).BasePath() / ".." / "Data" / "Animation" / fileName;
}

const Entity* FindSkinnedEntity(const Handle<Prefab>& prefab)
{
    for (Node* descendant : prefab->GetRoot()->GetDescendants())
    {
        Entity* entity = TryCast<Entity>(descendant);

        if (!entity || !entity->GetEntityManager())
        {
            continue;
        }

        if (const MeshComponent* meshComponent = entity->TryGetComponent<MeshComponent>(); meshComponent && meshComponent->skeleton.IsValid())
        {
            return entity;
        }
    }

    return nullptr;
}

const Animation* FindAnimation(const Skeleton& skeleton, Name name)
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

float RotationAngleBetween(const Quat4f& a, const Quat4f& b)
{
    const float dot = MathUtil::Abs(a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w);

    return 2.0f * MathUtil::Arccos(MathUtil::Min(dot, 1.0f));
}

void CheckTwoTakeRig(EngineTestContext& context, const char* fileName)
{
    // imported assets persist their blobs into the current registry, so keep them out of the engine content
    Handle<AssetRegistry> scratchRegistry = MakeHandle<AssetRegistry>(AssetRegistryId::Game, EngineGlobals::GetTempDirectory() / "AnimationImportTests");
    scratchRegistry->Initialize(nullptr);

    GlobalContextScope registryScope { AssetRegistryContext { scratchRegistry } };

    auto loadResult = g_assetManager->Load<Prefab>(AnimationDataPath(fileName), String::empty, AssetLoadHint::Transient);
    HYP_TEST_REQUIRE(loadResult.HasValue());

    Handle<Prefab> prefab = loadResult->Result();
    HYP_TEST_REQUIRE(prefab.IsValid());

    const Entity* skinnedEntity = FindSkinnedEntity(prefab);
    HYP_TEST_REQUIRE(skinnedEntity != nullptr);

    const Skeleton* skeleton = skinnedEntity->GetComponent<MeshComponent>().skeleton.Get();

    for (const Name boneName : { NAME("Root"), NAME("Mid"), NAME("Tip") })
    {
        HYP_TEST_CHECK_MSG(skeleton->FindBone(boneName) != nullptr, "{}: bone {} missing", fileName, boneName);
        HYP_TEST_CHECK_MSG(skeleton->FindBoneIndex(boneName) != uint32(-1), "{}: no index for bone {}", fileName, boneName);
    }

    const Bone* rootBone = skeleton->FindBone(NAME("Root"));
    const Bone* midBone = skeleton->FindBone(NAME("Mid"));
    const Bone* tipBone = skeleton->FindBone(NAME("Tip"));
    HYP_TEST_REQUIRE(rootBone && midBone && tipBone);

    // the bones are a straight column of equal lengths, whatever the file's units
    const float rootToMid = (midBone->GetWorldTranslation() - rootBone->GetWorldTranslation()).Length();
    const float rootToTip = (tipBone->GetWorldTranslation() - rootBone->GetWorldTranslation()).Length();
    HYP_TEST_CHECK_MSG(rootToMid > 0.0f && MathUtil::Abs(rootToTip / rootToMid - 2.0f) < 0.01f, "{}: bind pose joints Root-Mid {}, Root-Tip {}", fileName, rootToMid, rootToTip);

    // the skeleton lives in the skinned mesh's space: the Mid joint sits inside the column it bends
    const BoundingBox meshWorldBounds = skinnedEntity->GetWorldMatrix() * skinnedEntity->GetComponent<MeshComponent>().mesh->GetAABB();
    const Vec3f midJointWorld = (skinnedEntity->GetWorldMatrix() * midBone->GetWorldMatrix()).ExtractTranslation();
    HYP_TEST_CHECK_MSG(meshWorldBounds.ContainsPoint(midJointWorld), "{}: Mid joint {} is outside the mesh bounds {} - {}", fileName, midJointWorld, meshWorldBounds.min, meshWorldBounds.max);

    HYP_TEST_CHECK_MSG(skeleton->GetAnimations().Size() == 2, "{}: expected 2 animations, got {}", fileName, skeleton->GetAnimations().Size());

    // both takes start at rest, so their first keys must land every bone on its bind pose
    for (const Handle<Animation>& animation : skeleton->GetAnimations())
    {
        for (const Bone* bone : { rootBone, midBone, tipBone })
        {
            const AnimationTrack* track = animation->FindTrack(bone->GetBoneName());

            if (!track || track->GetKeyframes().Size() == 0)
            {
                continue;
            }

            const Transform& restKey = track->GetKeyframes()[0].transform;
            const Transform& bindTransform = bone->GetBindingTransform();

            const float rotationError = RotationAngleBetween(restKey.GetRotation(), bindTransform.GetRotation());
            const float translationError = (restKey.GetTranslation() - bindTransform.GetTranslation()).Length();

            HYP_TEST_CHECK_MSG(rotationError < 0.02f && translationError < rootToMid * 0.01f,
                "{}: {} first key of {} is off its bind pose (rotation {} rad, translation {})",
                fileName, animation->GetName(), bone->GetBoneName(), rotationError, translationError);
        }
    }

    const Animation* wave = FindAnimation(*skeleton, NAME("Wave"));
    const Animation* bend = FindAnimation(*skeleton, NAME("Bend"));
    HYP_TEST_REQUIRE(wave != nullptr && bend != nullptr);

    for (const Animation* animation : { wave, bend })
    {
        for (const Handle<AnimationTrack>& track : animation->GetTracks())
        {
            HYP_TEST_CHECK_MSG(skeleton->FindBone(track->GetBoneName()) != nullptr, "{}: track {} doesn't bind to a bone", fileName, track->GetBoneName());
        }
    }

    // glTF keeps Blender's frame 1 offset as a leading hold, FBX takes start at LocalStart, so measure from the first key
    const auto keyedDuration = [](const AnimationTrack& track)
    {
        return track.GetKeyframes()[track.GetKeyframes().Size() - 1].time - track.GetKeyframes()[0].time;
    };

    const AnimationTrack* tipTrack = wave->FindTrack(NAME("Tip"));
    const AnimationTrack* midTrack = bend->FindTrack(NAME("Mid"));
    HYP_TEST_REQUIRE(tipTrack != nullptr && midTrack != nullptr);
    HYP_TEST_REQUIRE(tipTrack->GetKeyframes().Size() > 0 && midTrack->GetKeyframes().Size() > 0);

    HYP_TEST_CHECK_MSG(MathUtil::Abs(keyedDuration(*tipTrack) - 23.0f / 24.0f) < 0.02f, "{}: Wave keyed for {}", fileName, keyedDuration(*tipTrack));
    HYP_TEST_CHECK_MSG(MathUtil::Abs(keyedDuration(*midTrack) - 47.0f / 24.0f) < 0.02f, "{}: Bend keyed for {}", fileName, keyedDuration(*midTrack));

    const float firstKeyTime = tipTrack->GetKeyframes()[0].time;

    const Quat4f restRotation = tipTrack->GetKeyframe(firstKeyTime).transform.GetRotation();
    const Quat4f peakRotation = tipTrack->GetKeyframe(firstKeyTime + 11.0f / 24.0f).transform.GetRotation();
    const float peakAngle = RotationAngleBetween(restRotation, peakRotation);

    HYP_TEST_CHECK_MSG(MathUtil::Abs(peakAngle - 1.0f) < 0.05f, "{}: Tip turns {} rad at the Wave peak, expected 1", fileName, peakAngle);

    const MeshComponent& meshComponent = skinnedEntity->GetComponent<MeshComponent>();
    const VertexArrayView vertexData = meshComponent.mesh->GetVertexData(0);
    HYP_TEST_CHECK_MSG(vertexData.layoutDesc.mask & VT_Skeletal, "{}: mesh has no skinning attributes", fileName);

    if (vertexData.layoutDesc.mask == (VT_Simple | VT_Skeletal) && vertexData.floatData != nullptr)
    {
        uint32 numWeightedVertices = 0;

        for (size_t vertexIndex = 0; vertexIndex < vertexData.vertexCount; ++vertexIndex)
        {
            const SkeletalVertex& vertex = reinterpret_cast<const SkeletalVertex*>(vertexData.floatData)[vertexIndex];
            const float* weights = vertex.GetBoneWeights();

            numWeightedVertices += (weights[0] + weights[1] + weights[2] + weights[3]) > 0.5f ? 1 : 0;
        }

        HYP_TEST_CHECK_MSG(numWeightedVertices == vertexData.vertexCount, "{}: {} of {} vertices carry bone weights", fileName, numWeightedVertices, vertexData.vertexCount);
    }

    // posing the Bend peak has to move Mid off its bind pose, or skinning sees nothing
    ApplyAnimParams params;
    params.time = midTrack->GetKeyframes()[0].time + 35.0f / 24.0f;
    params.blend = 1.0f;

    const_cast<Animation*>(bend)->Apply(*meshComponent.skeleton, params);

    const Mat4f& midSkinningMatrix = midBone->GetBoneMatrix();
    float largestDeviation = 0.0f;

    for (uint32 element = 0; element < 16; ++element)
    {
        largestDeviation = MathUtil::Max(largestDeviation, MathUtil::Abs(midSkinningMatrix.values[element] - Mat4f::Identity().values[element]));
    }

    HYP_TEST_CHECK_MSG(largestDeviation > 0.1f, "{}: Mid skinning matrix stays at identity when posed (largest deviation {})", fileName, largestDeviation);
}

} // namespace

HYP_ENGINE_TEST(AnimationImport, FbxTakesBindToBones)
{
    CheckTwoTakeRig(context, "TwoTakeRig.fbx");
}

HYP_ENGINE_TEST(AnimationImport, GltfAnimationsBindToBones)
{
    CheckTwoTakeRig(context, "TwoTakeRig.glb");
}

} // namespace scene
} // namespace tests
} // namespace Hyperion

#endif // HYP_TESTS
