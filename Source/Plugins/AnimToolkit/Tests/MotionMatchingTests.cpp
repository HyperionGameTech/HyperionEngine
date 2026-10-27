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
#include <Scene/Animation/Animation.hpp>
#include <Animation/AnimationPose.hpp>
#include <Animation/AnimationGraph.hpp>
#include <Animation/AnimationGraphInstance.hpp>
#include <Animation/HumanoidMapping.hpp>
#include <Animation/MotionMatching.hpp>

#include <Core/FileSystem/FilePath.hpp>

#include <Plugins/PluginManager.hpp>

#include <Core/Utilities/GlobalContext.hpp>

#include <Core/Resource/ResLock.hpp>

#include <Core/Math/MathUtil.hpp>

#include <cmath>

namespace Hyperion {
namespace tests {
namespace scene {

namespace {

Handle<AssetRegistry> GetTestEngineRegistry()
{
    if (!GetEngineAssetRegistry().IsValid())
    {
        Handle<AssetRegistry> engineRegistry = MakeHandle<AssetRegistry>(AssetRegistryId::Engine, EngineGlobals::GetContentDirectory<HYP_STATIC_STRING("Engine")>());
        engineRegistry->Initialize(nullptr);

        SetEngineAssetRegistry(engineRegistry);
    }

    // the test host doesn't enable plugins, so the motion database's plugin content is mounted here
    for (const FilePath& contentDirectory : PluginManager::GetInstance().GetPluginContentDirectories({ NAME("AnimToolkit") }))
    {
        GetEngineAssetRegistry()->AddContentRoot(contentDirectory);
    }

    return GetEngineAssetRegistry();
}

Handle<Skeleton> LoadTwoTakeRig(Handle<Animation>& outWave, Handle<Animation>& outBend)
{
    auto loadResult = g_assetManager->Load<Prefab>(FilePath(__FILE__).BasePath() / ".." / ".." / ".." / ".." / "Tests" / "Engine" / "Data" / "Animation" / "TwoTakeRig.glb", String::empty, AssetLoadHint::Transient);

    if (!loadResult.HasValue())
    {
        return Handle<Skeleton>::empty;
    }

    for (Node* descendant : loadResult->Result()->GetRoot()->GetDescendants())
    {
        const Entity* entity = TryCast<Entity>(descendant);
        const MeshComponent* meshComponent = entity && entity->GetEntityManager() ? entity->TryGetComponent<MeshComponent>() : nullptr;

        if (meshComponent && meshComponent->skeleton.IsValid())
        {
            outWave = meshComponent->skeleton->FindAnimation(NAME("Wave"));
            outBend = meshComponent->skeleton->FindAnimation(NAME("Bend"));

            return meshComponent->skeleton;
        }
    }

    return Handle<Skeleton>::empty;
}

MotionDatabaseClip MakeEntry(const Handle<Animation>& clip, const Vec2f& velocity)
{
    MotionDatabaseClip entry;
    entry.clip = clip;
    entry.velocity = velocity;

    return entry;
}

// a query for a character moving at velocity that wants desiredVelocity, its pose part taken from a database frame
Array<float> MakeQuery(const MotionDatabase& database, uint32 poseFrame, const Vec2f& velocity, const Vec2f& desiredVelocity, float acceleration)
{
    Array<float> query;
    query.Resize(MotionDatabase::NumFeatures);

    const Span<const float> poseFeatures = database.GetFrameFeatures(poseFrame);

    for (uint32 feature = 0; feature < uint32(MotionFeature::TrajectoryPosition); ++feature)
    {
        query[feature] = poseFeatures[feature];
    }

    for (uint32 point = 0; point < MotionDatabase::NumTrajectoryPoints; ++point)
    {
        const Vec2f position = MotionDatabase::PredictPosition(velocity, desiredVelocity, acceleration, database.GetTrajectoryTime(point));

        query[uint32(MotionFeature::TrajectoryPosition) + point * 2] = position.x;
        query[uint32(MotionFeature::TrajectoryPosition) + point * 2 + 1] = position.y;
        query[uint32(MotionFeature::TrajectoryFacing) + point * 2] = 0.0f;
        query[uint32(MotionFeature::TrajectoryFacing) + point * 2 + 1] = 1.0f;
    }

    return query;
}

} // namespace

HYP_ENGINE_TEST(MotionMatching, EngineRigFacesWhereItsToesPoint)
{
    const Handle<Skeleton> skeleton = GetTestEngineRegistry()->GetAsset<Skeleton>(AssetBuckets::Skeletons, NAME("BaseHumanRig"));
    HYP_TEST_REQUIRE(skeleton.IsValid() && HumanoidSkeletonData::IsHumanoid(*skeleton));

    const SkeletonBindData& bindData = skeleton->GetBindData();
    const HumanoidMapping& mapping = HumanoidSkeletonData::GetSkeletonMapping(*skeleton);

    HYP_TEST_CHECK(mapping.GetBoneName(HumanoidBone::LeftToes).IsValid());

    Array<Mat4f> modelMatrices;
    Array<Mat4f> skinningMatrices;
    modelMatrices.Resize(bindData.NumSlots());
    skinningMatrices.Resize(bindData.NumSlots());

    PoseOps::ComputeMatrices(bindData, bindData.restLocals.ToSpan(), modelMatrices.ToSpan(), skinningMatrices.ToSpan());

    const auto positionOf = [&](HumanoidBone bone)
    {
        return modelMatrices[bindData.FindSlot(mapping.GetBoneName(bone))].ExtractTranslation();
    };

    const Vec3f up = PoseOps::GetModelUp(*skeleton);
    const Vec3f forward = PoseOps::GetModelForward(*skeleton);

    // the hip-line fallback, right x up, has to agree with the toes for rigs that have none
    Vec3f acrossHips = positionOf(HumanoidBone::RightUpperLeg) - positionOf(HumanoidBone::LeftUpperLeg);
    acrossHips -= up * acrossHips.Dot(up);

    const Vec3f fromHips = acrossHips.Cross(up).Normalized();

    HYP_TEST_CHECK_MSG(forward.Dot(fromHips) > 0.9f, "toes face {}, the hips say {}", forward, fromHips);
}

HYP_ENGINE_TEST(MotionMatching, DatabaseDescribesInPlaceClips)
{
    Handle<AssetRegistry> scratchRegistry = MakeHandle<AssetRegistry>(AssetRegistryId::Game, EngineGlobals::GetTempDirectory() / "MotionMatchingTests");
    scratchRegistry->Initialize(nullptr);

    GlobalContextScope registryScope { AssetRegistryContext { scratchRegistry } };

    Handle<Animation> wave;
    Handle<Animation> bend;
    Handle<Skeleton> skeleton = LoadTwoTakeRig(wave, bend);
    HYP_TEST_REQUIRE(skeleton.IsValid() && wave.IsValid() && bend.IsValid());

    Handle<MotionDatabase> database = MakeHandle<MotionDatabase>(NAME("TestMotion"));
    database->SetSkeleton(skeleton);
    database->SetClips({ MakeEntry(wave, Vec2f::Zero()), MakeEntry(bend, Vec2f(0.0f, 1.5f)) });

    HYP_TEST_REQUIRE(database->Build());

    const uint32 expectedFrames = uint32(std::ceil(wave->GetLength() * 30.0f - 0.5f)) + uint32(std::ceil(bend->GetLength() * 30.0f - 0.5f));
    HYP_TEST_CHECK_MSG(MathUtil::Abs(int32(database->NumFrames()) - int32(expectedFrames)) <= 2, "{} frames, expected about {}", database->NumFrames(), expectedFrames);

    // a walking clip's trajectory runs straight ahead at its speed, facing forward all the way
    const uint32 bendFrame = database->FindFrame(1, 0.0f);
    HYP_TEST_REQUIRE(bendFrame != ~0u);

    const Span<const float> features = database->GetFrameFeatures(bendFrame);

    for (uint32 point = 0; point < MotionDatabase::NumTrajectoryPoints; ++point)
    {
        const float time = database->GetTrajectoryTime(point);

        HYP_TEST_CHECK(MathUtil::Abs(features[uint32(MotionFeature::TrajectoryPosition) + point * 2]) < 1e-4f);
        HYP_TEST_CHECK_MSG(MathUtil::Abs(features[uint32(MotionFeature::TrajectoryPosition) + point * 2 + 1] - 1.5f * time) < 1e-4f,
            "point {} is {} ahead", point, features[uint32(MotionFeature::TrajectoryPosition) + point * 2 + 1]);
        HYP_TEST_CHECK(MathUtil::Abs(features[uint32(MotionFeature::TrajectoryFacing) + point * 2 + 1] - 1.0f) < 1e-4f);
    }

    // a rig with no humanoid mapping only has the trajectory
    HYP_TEST_CHECK(features[uint32(MotionFeature::LeftFootPosition)] == 0.0f && features[uint32(MotionFeature::HipVelocity) + 2] == 0.0f);
}

HYP_ENGINE_TEST(MotionMatching, NodeFollowsDesiredVelocity)
{
    Handle<AssetRegistry> scratchRegistry = MakeHandle<AssetRegistry>(AssetRegistryId::Game, EngineGlobals::GetTempDirectory() / "MotionMatchingTests");
    scratchRegistry->Initialize(nullptr);

    GlobalContextScope registryScope { AssetRegistryContext { scratchRegistry } };

    Handle<Animation> wave;
    Handle<Animation> bend;
    Handle<Skeleton> skeleton = LoadTwoTakeRig(wave, bend);
    HYP_TEST_REQUIRE(skeleton.IsValid() && wave.IsValid() && bend.IsValid());

    Handle<MotionDatabase> database = MakeHandle<MotionDatabase>(NAME("TestMotion"));
    database->SetSkeleton(skeleton);
    database->SetClips({ MakeEntry(wave, Vec2f::Zero()), MakeEntry(bend, Vec2f(0.0f, 1.5f)) });
    HYP_TEST_REQUIRE(database->Build());

    Handle<AnimationGraph> graph = MakeHandle<AnimationGraph>(NAME("MotionGraph"));
    graph->AddParameter(NAME("Velocity"), AnimationGraphParameterType::Vector2);
    graph->AddParameter(NAME("DesiredVelocity"), AnimationGraphParameterType::Vector2);

    Handle<AnimationMotionMatchingNode> motionNode = MakeHandle<AnimationMotionMatchingNode>();
    motionNode->database = database;

    const uint32 motionIndex = graph->AddNode(motionNode);
    graph->SetRootNode(motionIndex);

    HYP_TEST_CHECK(graph->CollectClips().Size() == 2);

    Handle<AnimationGraphInstance> instance = MakeHandle<AnimationGraphInstance>(graph);

    const auto playing = [&]()
    {
        const AnimationGraphNodeState* state = instance->GetNodeState(motionIndex);

        return state ? state->currentState : InvalidAnimationGraphNode;
    };

    // standing still finds the clip that doesn't move
    instance->Update(0.05f, *skeleton);
    HYP_TEST_CHECK_MSG(playing() == 0, "playing {} while standing", playing());

    // wanting to walk jumps to the walking clip at the next search, through inertialization
    instance->SetVector2(NAME("DesiredVelocity"), Vec2f(0.0f, 1.5f));

    for (uint32 update = 0; update < 3; ++update)
    {
        instance->Update(0.05f, *skeleton);
    }

    HYP_TEST_CHECK_MSG(playing() == 1, "playing {} while walking", playing());
    HYP_TEST_CHECK(instance->GetNodeState(motionIndex)->isInertializing);

    Array<Transform> pose;
    pose.Resize(skeleton->GetBindData().NumSlots());
    instance->Evaluate(*skeleton, pose.ToSpan());

    // and stopping goes back
    instance->SetVector2(NAME("Velocity"), Vec2f(0.0f, 1.5f));
    instance->SetVector2(NAME("DesiredVelocity"), Vec2f::Zero());

    for (uint32 update = 0; update < 3; ++update)
    {
        instance->Update(0.05f, *skeleton);
    }

    HYP_TEST_CHECK_MSG(playing() == 0, "playing {} after stopping", playing());
}

HYP_ENGINE_TEST(MotionMatching, EngineDatabasePicksTheWayToMove)
{
    const Handle<MotionDatabase> database = GetTestEngineRegistry()->GetAsset<MotionDatabase>(AssetBuckets::Animations, NAME("HumanoidMotion"));

    HYP_TEST_CHECK_MSG(database.IsValid(), "the engine has no HumanoidMotion database; it is AnimToolkit plugin content, built by BuildAnimationContentCommandlet --parts=motionmatching");
    HYP_TEST_REQUIRE(database.IsValid() && database->IsBuilt());

    const auto clipName = [&](uint32 frame)
    {
        return frame != ~0u ? database->GetClips()[database->GetFrame(frame).clipIndex].clip->GetName() : Name::Invalid();
    };

    const uint32 idleFrame = database->FindFrame(0, 0.0f);
    HYP_TEST_REQUIRE(idleFrame != ~0u);

    // from standing, each way of moving finds its own clip
    const struct
    {
        Vec2f velocity;
        Name expected;
    } cases[] = {
        { Vec2f::Zero(), NAME("Idle") },
        { Vec2f(0.0f, 1.0f), NAME("Walk") },
        { Vec2f(1.0f, 0.0f), NAME("StrafeRight") },
        { Vec2f(-1.0f, 0.0f), NAME("StrafeLeft") },
        { Vec2f(0.0f, -1.0f), NAME("WalkBackward") },
        { Vec2f(0.0f, 8.0f), NAME("Run") }
    };

    for (const auto& testCase : cases)
    {
        const Array<float> query = MakeQuery(*database, idleFrame, testCase.velocity, testCase.velocity, 12.0f);
        const Name found = clipName(database->Search(query.ToSpan(), false));

        HYP_TEST_CHECK_MSG(found == testCase.expected, "moving at {} found {}, expected {}", testCase.velocity, found, testCase.expected);
    }

    // with the pose in the query, a change of mind still has to beat carrying on with the clip already playing
    const auto groupCosts = [&](Span<const float> query, uint32 frame)
    {
        static constexpr uint32 GroupStarts[] = { 0, 6, 12, 15, 21, 27 };

        const Span<const float> features = database->GetFrameFeatures(frame);

        String costs;

        for (uint32 group = 0; group < 5; ++group)
        {
            float cost = 0.0f;

            for (uint32 feature = GroupStarts[group]; feature < GroupStarts[group + 1]; ++feature)
            {
                const float difference = (query[feature] - features[feature]) * database->GetFeatureScale(feature);

                cost += difference * difference;
            }

            costs += HYP_FORMAT("{} ", cost);
        }

        return costs;
    };

    const auto bestFrameOf = [&](Name clip, Span<const float> query)
    {
        uint32 best = ~0u;
        float bestCost = MathUtil::MaxSafeValue<float>();

        for (uint32 frame = 0; frame < database->NumFrames(); ++frame)
        {
            if (clipName(frame) != clip)
            {
                continue;
            }

            float cost = 0.0f;

            for (uint32 feature = 0; feature < MotionDatabase::NumFeatures; ++feature)
            {
                const float difference = (query[feature] - database->GetFrameFeatures(frame)[feature]) * database->GetFeatureScale(feature);

                cost += difference * difference;
            }

            if (cost < bestCost)
            {
                best = frame;
                bestCost = cost;
            }
        }

        return best;
    };

    const auto frameOf = [&](Name clip, float time)
    {
        for (uint32 clipIndex = 0; clipIndex < database->GetClips().Size(); ++clipIndex)
        {
            if (database->GetClips()[clipIndex].clip->GetName() == clip)
            {
                return database->FindFrame(clipIndex, time);
            }
        }

        return ~0u;
    };

    const struct
    {
        Name from;
        Vec2f velocity;
        Vec2f desiredVelocity;
        Name expected;
    } changes[] = {
        { NAME("Idle"), Vec2f::Zero(), Vec2f(0.0f, 1.35f), NAME("Walk") },
        { NAME("Walk"), Vec2f(0.0f, 1.35f), Vec2f::Zero(), NAME("Idle") },
        { NAME("Walk"), Vec2f(0.0f, 1.35f), Vec2f(0.0f, 7.5f), NAME("Run") },
        // a sprint still carries the character over half a metre once it lets go, so it slows through a walk
        { NAME("Run"), Vec2f(0.0f, 7.5f), Vec2f::Zero(), NAME("Walk") },
        { NAME("Walk"), Vec2f::Zero(), Vec2f::Zero(), NAME("Idle") }
    };

    for (const auto& change : changes)
    {
        const uint32 fromFrame = frameOf(change.from, 0.5f);
        HYP_TEST_REQUIRE(fromFrame != ~0u);

        const Array<float> query = MakeQuery(*database, fromFrame, change.velocity, change.desiredVelocity, 12.0f);
        const uint32 found = database->Search(query.ToSpan(), true);
        const uint32 expectedFrame = bestFrameOf(change.expected, query.ToSpan());

        HYP_TEST_CHECK_MSG(clipName(found) == change.expected, "from {} at {} wanting {}: found {} [{}], {} would cost [{}]",
            change.from, change.velocity, change.desiredVelocity, clipName(found), groupCosts(query.ToSpan(), found),
            change.expected, expectedFrame != ~0u ? groupCosts(query.ToSpan(), expectedFrame) : String("-"));
    }
}

} // namespace scene
} // namespace tests
} // namespace Hyperion

#endif // HYP_TESTS
