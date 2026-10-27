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
#include <Asset/SerializationUtils.hpp>

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
#include <Animation/BoneMask.hpp>

#include <Core/FileSystem/FilePath.hpp>

#include <Plugins/PluginManager.hpp>

#include <Core/DataProcessing/HMF/HMF.hpp>

#include <Core/Utilities/GlobalContext.hpp>

#include <Core/Resource/ResLock.hpp>

#include <Core/Math/MathUtil.hpp>

namespace Hyperion {
namespace tests {
namespace scene {

namespace {

struct GraphTestRig
{
    Handle<Skeleton> skeleton;
    Handle<Animation> wave;
    Handle<Animation> bend;
};

GraphTestRig LoadGraphTestRig(EngineTestContext& context)
{
    Handle<AssetRegistry> scratchRegistry = MakeHandle<AssetRegistry>(AssetRegistryId::Game, EngineGlobals::GetTempDirectory() / "AnimationGraphTests");
    scratchRegistry->Initialize(nullptr);

    GlobalContextScope registryScope { AssetRegistryContext { scratchRegistry } };

    GraphTestRig rig;

    auto loadResult = g_assetManager->Load<Prefab>(FilePath(__FILE__).BasePath() / ".." / ".." / ".." / ".." / "Tests" / "Engine" / "Data" / "Animation" / "TwoTakeRig.glb", String::empty, AssetLoadHint::Transient);

    if (!HYP_TEST_CHECK(loadResult.HasValue()))
    {
        return rig;
    }

    for (Node* descendant : loadResult->Result()->GetRoot()->GetDescendants())
    {
        const Entity* entity = TryCast<Entity>(descendant);
        const MeshComponent* meshComponent = entity && entity->GetEntityManager() ? entity->TryGetComponent<MeshComponent>() : nullptr;

        if (meshComponent && meshComponent->skeleton.IsValid())
        {
            rig.skeleton = meshComponent->skeleton;
            rig.wave = rig.skeleton->FindAnimation(NAME("Wave"));
            rig.bend = rig.skeleton->FindAnimation(NAME("Bend"));

            break;
        }
    }

    return rig;
}

uint32 AddClipNode(AnimationGraph& graph, const Handle<Animation>& clip, Name syncGroup = Name::Invalid())
{
    Handle<AnimationClipNode> clipNode = MakeHandle<AnimationClipNode>();
    clipNode->clip = clip;
    clipNode->syncGroup = syncGroup;

    return graph.AddNode(clipNode);
}

Array<Transform> SampledPose(const SkeletonBindData& bindData, const Animation& clip, float time)
{
    Array<Transform> pose = Array<Transform>(PoseOps::GetRestPose(bindData));
    PoseOps::SampleClip(clip, PoseOps::BindClip(clip, bindData), bindData, time, pose.ToSpan());

    return pose;
}

float LargestPoseDifference(Span<const Transform> a, Span<const Transform> b)
{
    float largest = 0.0f;

    for (size_t slot = 0; slot < a.Size(); ++slot)
    {
        const Mat4f matrixA = a[slot].GetMatrix();
        const Mat4f matrixB = b[slot].GetMatrix();

        for (uint32 element = 0; element < 16; ++element)
        {
            largest = MathUtil::Max(largest, MathUtil::Abs(matrixA.values[element] - matrixB.values[element]));
        }
    }

    return largest;
}

Array<Transform> EvaluatedPose(AnimationGraphInstance& instance, const Skeleton& skeleton)
{
    Array<Transform> pose;
    pose.Resize(skeleton.GetBindData().NumSlots());

    instance.Evaluate(skeleton, pose.ToSpan());

    return pose;
}

} // namespace

HYP_ENGINE_TEST(AnimationGraph, ClipNodeAdvancesAndLoops)
{
    GraphTestRig rig = LoadGraphTestRig(context);
    HYP_TEST_REQUIRE(rig.skeleton.IsValid() && rig.wave.IsValid());

    Handle<AnimationGraph> graph = MakeHandle<AnimationGraph>(NAME("ClipGraph"));
    graph->SetRootNode(AddClipNode(*graph, rig.wave));

    Handle<AnimationGraphInstance> instance = MakeHandle<AnimationGraphInstance>(graph);

    constexpr float Delta = 0.1f;
    constexpr uint32 NumUpdates = 25;

    for (uint32 update = 0; update < NumUpdates; ++update)
    {
        instance->Update(Delta, *rig.skeleton);
    }

    // the first update starts the clip at its offset, each later one advances it
    const float expectedTime = std::fmod(Delta * float(NumUpdates - 1), rig.wave->GetLength());

    HYP_TEST_CHECK_MSG(MathUtil::Abs(instance->GetNodeTime(0) - expectedTime) < 1e-4f, "clip time {}, expected {}", instance->GetNodeTime(0), expectedTime);
    HYP_TEST_CHECK(instance->GetNodeWeight(0) == 1.0f);

    const Array<Transform> pose = EvaluatedPose(*instance, *rig.skeleton);
    const Array<Transform> expected = SampledPose(rig.skeleton->GetBindData(), *rig.wave, instance->GetNodeTime(0));

    HYP_TEST_CHECK(LargestPoseDifference(pose.ToSpan(), expected.ToSpan()) < 1e-5f);
}

HYP_ENGINE_TEST(AnimationGraph, Blend1DWeightsAndPose)
{
    GraphTestRig rig = LoadGraphTestRig(context);
    HYP_TEST_REQUIRE(rig.skeleton.IsValid() && rig.wave.IsValid() && rig.bend.IsValid());

    Handle<AnimationGraph> graph = MakeHandle<AnimationGraph>(NAME("Blend1DGraph"));
    graph->AddParameter(NAME("Speed"), AnimationGraphParameterType::Float);

    const uint32 waveNode = AddClipNode(*graph, rig.wave);
    const uint32 bendNode = AddClipNode(*graph, rig.bend);

    Handle<AnimationBlend1DNode> blendNode = MakeHandle<AnimationBlend1DNode>();
    blendNode->parameter = NAME("Speed");
    blendNode->samples = { AnimationBlendSample { bendNode, Vec2f(1.0f, 0.0f) }, AnimationBlendSample { waveNode, Vec2f(0.0f, 0.0f) } };

    graph->SetRootNode(graph->AddNode(blendNode));

    Handle<AnimationGraphInstance> instance = MakeHandle<AnimationGraphInstance>(graph);

    instance->SetFloat(NAME("Speed"), 0.25f);
    HYP_TEST_CHECK(instance->GetNodeWeight(waveNode) == 0.0f);

    instance->Update(0.0f, *rig.skeleton);

    HYP_TEST_CHECK(MathUtil::Abs(instance->GetNodeWeight(waveNode) - 0.75f) < 1e-6f);
    HYP_TEST_CHECK(MathUtil::Abs(instance->GetNodeWeight(bendNode) - 0.25f) < 1e-6f);

    const SkeletonBindData& bindData = rig.skeleton->GetBindData();
    const Array<Transform> wavePose = SampledPose(bindData, *rig.wave, 0.0f);
    const Array<Transform> bendPose = SampledPose(bindData, *rig.bend, 0.0f);

    Array<Transform> expected = Array<Transform>(PoseOps::GetRestPose(bindData));
    PoseOps::BlendPoses(wavePose.ToSpan(), bendPose.ToSpan(), 0.25f, {}, expected.ToSpan());

    const Array<Transform> pose = EvaluatedPose(*instance, *rig.skeleton);

    HYP_TEST_CHECK(LargestPoseDifference(pose.ToSpan(), expected.ToSpan()) < 1e-5f);

    // out of range clamps to the end sample, and a sample sitting on the value takes all the weight
    instance->SetFloat(NAME("Speed"), 3.0f);
    instance->Update(0.0f, *rig.skeleton);

    HYP_TEST_CHECK(instance->GetNodeWeight(bendNode) == 1.0f && instance->GetNodeWeight(waveNode) == 0.0f);
}

HYP_ENGINE_TEST(AnimationGraph, Blend2DIsExactAtSamples)
{
    Handle<AnimationBlend2DNode> blendNode = MakeHandle<AnimationBlend2DNode>();
    blendNode->samples = {
        AnimationBlendSample { 0, Vec2f(0.0f, 0.0f) },
        AnimationBlendSample { 1, Vec2f(0.0f, 1.0f) },
        AnimationBlendSample { 2, Vec2f(1.0f, 0.0f) },
        AnimationBlendSample { 3, Vec2f(-1.0f, 0.0f) },
        AnimationBlendSample { 4, Vec2f(0.0f, -1.0f) }
    };

    Array<float> weights;

    for (size_t sampleIndex = 0; sampleIndex < blendNode->samples.Size(); ++sampleIndex)
    {
        blendNode->ComputeWeights(blendNode->samples[sampleIndex].position, weights);

        for (size_t weightIndex = 0; weightIndex < weights.Size(); ++weightIndex)
        {
            HYP_TEST_CHECK(MathUtil::Abs(weights[weightIndex] - (weightIndex == sampleIndex ? 1.0f : 0.0f)) < 1e-6f);
        }
    }

    for (const Vec2f& point : { Vec2f(0.3f, 0.4f), Vec2f(-0.7f, 0.2f), Vec2f(2.0f, 2.0f) })
    {
        blendNode->ComputeWeights(point, weights);

        float total = 0.0f;

        for (float weight : weights)
        {
            HYP_TEST_CHECK(weight >= 0.0f);
            total += weight;
        }

        HYP_TEST_CHECK_MSG(MathUtil::Abs(total - 1.0f) < 1e-5f, "weights at ({}, {}) sum to {}", point.x, point.y, total);
    }

    // halfway between the centre and the forward sample, only those two contribute
    blendNode->ComputeWeights(Vec2f(0.0f, 0.5f), weights);

    HYP_TEST_CHECK(MathUtil::Abs(weights[0] - 0.5f) < 1e-5f && MathUtil::Abs(weights[1] - 0.5f) < 1e-5f);
}

HYP_ENGINE_TEST(AnimationGraph, SyncGroupSharesPhase)
{
    GraphTestRig rig = LoadGraphTestRig(context);
    HYP_TEST_REQUIRE(rig.skeleton.IsValid() && rig.wave.IsValid() && rig.bend.IsValid());

    Handle<AnimationGraph> graph = MakeHandle<AnimationGraph>(NAME("SyncGraph"));
    graph->AddParameter(NAME("Speed"), AnimationGraphParameterType::Float, Vec2f(0.4f, 0.0f));

    const uint32 waveNode = AddClipNode(*graph, rig.wave, NAME("Locomotion"));
    const uint32 bendNode = AddClipNode(*graph, rig.bend, NAME("Locomotion"));

    Handle<AnimationBlend1DNode> blendNode = MakeHandle<AnimationBlend1DNode>();
    blendNode->parameter = NAME("Speed");
    blendNode->samples = { AnimationBlendSample { waveNode, Vec2f(0.0f, 0.0f) }, AnimationBlendSample { bendNode, Vec2f(1.0f, 0.0f) } };

    graph->SetRootNode(graph->AddNode(blendNode));

    Handle<AnimationGraphInstance> instance = MakeHandle<AnimationGraphInstance>(graph);

    for (uint32 update = 0; update < 13; ++update)
    {
        instance->Update(0.07f, *rig.skeleton);
    }

    const float wavePhase = instance->GetNodeTime(waveNode) / rig.wave->GetLength();
    const float bendPhase = instance->GetNodeTime(bendNode) / rig.bend->GetLength();

    HYP_TEST_CHECK_MSG(MathUtil::Abs(wavePhase - bendPhase) < 1e-4f, "phases {} and {} differ", wavePhase, bendPhase);

    // the group cycles at the weight-blended cycle duration of its members
    const float cycleDuration = 0.6f * rig.wave->GetLength() + 0.4f * rig.bend->GetLength();
    const float expectedWaveTime = std::fmod(0.07f * 12.0f / cycleDuration * rig.wave->GetLength(), rig.wave->GetLength());

    HYP_TEST_CHECK(MathUtil::Abs(instance->GetNodeTime(waveNode) - expectedWaveTime) < 1e-4f);
}

HYP_ENGINE_TEST(AnimationGraph, SyncGroupBlendsSpeedWarpedPace)
{
    GraphTestRig rig = LoadGraphTestRig(context);
    HYP_TEST_REQUIRE(rig.skeleton.IsValid() && rig.wave.IsValid() && rig.bend.IsValid());

    Handle<AnimationGraph> graph = MakeHandle<AnimationGraph>(NAME("SpeedWarpedSyncGraph"));
    graph->AddParameter(NAME("Blend"), AnimationGraphParameterType::Float, Vec2f(0.5f, 0.0f));
    graph->AddParameter(NAME("Speed"), AnimationGraphParameterType::Float, Vec2f(2.0f, 0.0f));

    const auto addWarpedClip = [&](const Handle<Animation>& clip, float referenceSpeed)
    {
        Handle<AnimationClipNode> clipNode = MakeHandle<AnimationClipNode>();
        clipNode->clip = clip;
        clipNode->syncGroup = NAME("Locomotion");
        clipNode->speedParameter = NAME("Speed");
        clipNode->referenceSpeed = referenceSpeed;

        return graph->AddNode(clipNode);
    };

    // a slow-reference walk and a fast-reference run, like walking into a sprint
    const uint32 walkNode = addWarpedClip(rig.wave, 1.0f);
    const uint32 runNode = addWarpedClip(rig.bend, 8.0f);

    Handle<AnimationBlend1DNode> blendNode = MakeHandle<AnimationBlend1DNode>();
    blendNode->parameter = NAME("Blend");
    blendNode->samples = { AnimationBlendSample { walkNode, Vec2f(0.0f, 0.0f) }, AnimationBlendSample { runNode, Vec2f(1.0f, 0.0f) } };

    graph->SetRootNode(graph->AddNode(blendNode));

    Handle<AnimationGraphInstance> instance = MakeHandle<AnimationGraphInstance>(graph);

    for (uint32 update = 0; update < 13; ++update)
    {
        instance->Update(0.07f, *rig.skeleton);
    }

    // neither the walk's pace stretched to the run speed nor the run's alone
    const float cycleDuration = 0.5f * rig.wave->GetLength() / 2.0f + 0.5f * rig.bend->GetLength() / 0.25f;
    const float expectedWalkTime = std::fmod(0.07f * 12.0f / cycleDuration * rig.wave->GetLength(), rig.wave->GetLength());

    HYP_TEST_CHECK_MSG(MathUtil::Abs(instance->GetNodeTime(walkNode) - expectedWalkTime) < 1e-4f, "walk time {}, expected {}", instance->GetNodeTime(walkNode), expectedWalkTime);
}

HYP_ENGINE_TEST(AnimationGraph, LoneSyncGroupMemberAdvances)
{
    GraphTestRig rig = LoadGraphTestRig(context);
    HYP_TEST_REQUIRE(rig.skeleton.IsValid() && rig.wave.IsValid() && rig.bend.IsValid());

    Handle<AnimationGraph> graph = MakeHandle<AnimationGraph>(NAME("LoneSyncGraph"));
    graph->AddParameter(NAME("Speed"), AnimationGraphParameterType::Float, Vec2f(1.0f, 0.0f));

    const uint32 waveNode = AddClipNode(*graph, rig.wave, NAME("Locomotion"));
    const uint32 bendNode = AddClipNode(*graph, rig.bend, NAME("Locomotion"));

    Handle<AnimationBlend1DNode> blendNode = MakeHandle<AnimationBlend1DNode>();
    blendNode->parameter = NAME("Speed");
    blendNode->samples = { AnimationBlendSample { waveNode, Vec2f(0.0f, 0.0f) }, AnimationBlendSample { bendNode, Vec2f(1.0f, 0.0f) } };

    graph->SetRootNode(graph->AddNode(blendNode));

    Handle<AnimationGraphInstance> instance = MakeHandle<AnimationGraphInstance>(graph);

    for (uint32 update = 0; update < 5; ++update)
    {
        instance->Update(0.1f, *rig.skeleton);
    }

    HYP_TEST_CHECK(instance->GetNodeWeight(bendNode) == 1.0f);
    HYP_TEST_CHECK_MSG(MathUtil::Abs(instance->GetNodeTime(bendNode) - 0.4f) < 1e-4f, "bend time {}", instance->GetNodeTime(bendNode));
}

HYP_ENGINE_TEST(AnimationGraph, NotifiesFireWhenCrossed)
{
    GraphTestRig rig = LoadGraphTestRig(context);
    HYP_TEST_REQUIRE(rig.skeleton.IsValid() && rig.wave.IsValid());

    rig.wave->SetNotifies({ AnimationNotify { NAME("Step"), 0.25f, 0.0f } });

    Handle<AnimationGraph> graph = MakeHandle<AnimationGraph>(NAME("NotifyGraph"));
    graph->SetRootNode(AddClipNode(*graph, rig.wave));

    Handle<AnimationGraphInstance> instance = MakeHandle<AnimationGraphInstance>(graph);

    constexpr float Delta = 0.1f;

    uint32 numFired = 0;
    uint32 firstFiredUpdate = ~0u;

    const uint32 numUpdates = uint32(std::ceil(rig.wave->GetLength() * 2.0f / Delta)) + 1;

    for (uint32 update = 0; update < numUpdates; ++update)
    {
        instance->Update(Delta, *rig.skeleton);

        if (instance->GetFiredNotifies().Contains(NAME("Step")))
        {
            ++numFired;
            firstFiredUpdate = MathUtil::Min(firstFiredUpdate, update);
        }
    }

    HYP_TEST_CHECK_MSG(numFired == 2, "fired {} times over two loops", numFired);
    HYP_TEST_CHECK_MSG(firstFiredUpdate == 3, "first fired on update {}", firstFiredUpdate);
}

HYP_ENGINE_TEST(AnimationGraph, LayerAdditiveAndTwist)
{
    GraphTestRig rig = LoadGraphTestRig(context);
    HYP_TEST_REQUIRE(rig.skeleton.IsValid() && rig.wave.IsValid() && rig.bend.IsValid());

    const SkeletonBindData& bindData = rig.skeleton->GetBindData();
    const uint32 midSlot = bindData.FindSlot(NAME("Mid"));
    HYP_TEST_REQUIRE(midSlot != SkeletonBindData::InvalidSlot);

    Handle<AnimationGraph> graph = MakeHandle<AnimationGraph>(NAME("LayerGraph"));
    graph->AddParameter(NAME("Aim"), AnimationGraphParameterType::Float);

    const uint32 waveNode = AddClipNode(*graph, rig.wave);
    const uint32 bendNode = AddClipNode(*graph, rig.bend);

    Handle<BoneMask> mask = MakeHandle<BoneMask>(NAME("MidOnly"));
    mask->SetEntries({ BoneMaskEntry { NAME("Mid"), 1.0f, false } });

    Handle<AnimationLayerNode> layerNode = MakeHandle<AnimationLayerNode>();
    layerNode->base = waveNode;
    layerNode->layer = bendNode;
    layerNode->mask = mask;

    const uint32 layerIndex = graph->AddNode(layerNode);

    Handle<AnimationChainTwistNode> twistNode = MakeHandle<AnimationChainTwistNode>();
    twistNode->input = layerIndex;
    twistNode->rootBone = NAME("Mid");
    twistNode->endBone = NAME("Tip");
    twistNode->angleParameter = NAME("Aim");

    graph->SetRootNode(graph->AddNode(twistNode));

    Handle<AnimationGraphInstance> instance = MakeHandle<AnimationGraphInstance>(graph);
    instance->SetFloat(NAME("Aim"), 0.6f);
    instance->Update(0.0f, *rig.skeleton);

    const Array<Transform> wavePose = SampledPose(bindData, *rig.wave, 0.0f);
    const Array<Transform> bendPose = SampledPose(bindData, *rig.bend, 0.0f);

    Array<Transform> expected = wavePose;
    expected[midSlot] = bendPose[midSlot];
    PoseOps::ApplyTwist(bindData, NAME("Mid"), NAME("Tip"), 0.6f, expected.ToSpan());

    const Array<Transform> pose = EvaluatedPose(*instance, *rig.skeleton);

    HYP_TEST_CHECK(LargestPoseDifference(pose.ToSpan(), expected.ToSpan()) < 1e-5f);

    Handle<AnimationGraph> additiveGraph = MakeHandle<AnimationGraph>(NAME("AdditiveGraph"));

    Handle<AnimationAdditiveNode> additiveNode = MakeHandle<AnimationAdditiveNode>();
    additiveNode->base = AddClipNode(*additiveGraph, rig.wave);
    additiveNode->additive = AddClipNode(*additiveGraph, rig.bend);
    additiveNode->weight = 0.5f;

    additiveGraph->SetRootNode(additiveGraph->AddNode(additiveNode));

    Handle<AnimationGraphInstance> additiveInstance = MakeHandle<AnimationGraphInstance>(additiveGraph);
    additiveInstance->Update(0.0f, *rig.skeleton);

    Array<Transform> expectedAdditive = wavePose;
    PoseOps::ApplyAdditive(bindData, bendPose.ToSpan(), 0.5f, expectedAdditive.ToSpan());

    const Array<Transform> additivePose = EvaluatedPose(*additiveInstance, *rig.skeleton);

    HYP_TEST_CHECK(LargestPoseDifference(additivePose.ToSpan(), expectedAdditive.ToSpan()) < 1e-5f);
}

HYP_ENGINE_TEST(AnimationGraph, GraphRoundTripsThroughHMF)
{
    Handle<AnimationGraph> graph = MakeHandle<AnimationGraph>(NAME("SavedGraph"));
    graph->AddParameter(NAME("Speed"), AnimationGraphParameterType::Float, Vec2f(0.5f, 0.0f));

    Handle<AnimationClipNode> clipNode = MakeHandle<AnimationClipNode>();
    clipNode->name = NAME("Walk");
    clipNode->playRate = 1.5f;
    clipNode->syncGroup = NAME("Locomotion");

    const uint32 clipIndex = graph->AddNode(clipNode);

    Handle<AnimationBlend1DNode> blendNode = MakeHandle<AnimationBlend1DNode>();
    blendNode->parameter = NAME("Speed");
    blendNode->samples = { AnimationBlendSample { clipIndex, Vec2f(2.0f, 0.0f) } };

    graph->SetRootNode(graph->AddNode(blendNode));

    String text;
    HYP_TEST_REQUIRE(!ObjectToHMF(AnimationGraph::StaticClass(), BoxedValue(graph), text).HasError());

    HMF::ParseResult parsed = HMF::Parse(text);
    HYP_TEST_REQUIRE(parsed.HasValue());

    Handle<AnimationGraph> loaded = parsed.GetValue().Is<Handle<ObjectBase>>()
        ? TryCast<AnimationGraph>(parsed.GetValue().Get<Handle<ObjectBase>>())
        : Handle<AnimationGraph>::empty;

    HYP_TEST_CHECK_MSG(loaded.IsValid(), "didn't read back as an AnimationGraph: {}", text);
    HYP_TEST_REQUIRE(loaded.IsValid());

    HYP_TEST_CHECK(loaded->NumNodes() == 2 && loaded->GetRootNode() == 1);
    HYP_TEST_CHECK(loaded->GetParameters().Size() == 1 && loaded->GetParameters()[0].defaultValue.x == 0.5f);

    const AnimationClipNode* loadedClip = TryCast<AnimationClipNode>(loaded->GetNode(0));
    const AnimationBlend1DNode* loadedBlend = TryCast<AnimationBlend1DNode>(loaded->GetNode(1));
    HYP_TEST_REQUIRE(loadedClip != nullptr && loadedBlend != nullptr);

    HYP_TEST_CHECK(loadedClip->playRate == 1.5f);
    HYP_TEST_CHECK(loadedClip->syncGroup == NAME("Locomotion"));

    // unset names are written as empty strings and must stay unset, or the clip would wait on a time parameter
    HYP_TEST_CHECK_MSG(!loadedClip->timeParameter.IsValid(), "an unset time parameter read back as set: {}", text);

    HYP_TEST_CHECK(loadedBlend->samples.Size() == 1 && loadedBlend->samples[0].input == 0 && loadedBlend->samples[0].position.x == 2.0f);
}

HYP_ENGINE_TEST(AnimationGraph, CyclesAreIgnored)
{
    GraphTestRig rig = LoadGraphTestRig(context);
    HYP_TEST_REQUIRE(rig.skeleton.IsValid());

    Handle<AnimationGraph> graph = MakeHandle<AnimationGraph>(NAME("CycleGraph"));

    Handle<AnimationLayerNode> layerNode = MakeHandle<AnimationLayerNode>();
    layerNode->base = 0;
    layerNode->layer = 0;

    graph->SetRootNode(graph->AddNode(layerNode));

    Handle<AnimationGraphInstance> instance = MakeHandle<AnimationGraphInstance>(graph);
    instance->Update(0.1f, *rig.skeleton);

    const Array<Transform> pose = EvaluatedPose(*instance, *rig.skeleton);

    HYP_TEST_CHECK(LargestPoseDifference(pose.ToSpan(), rig.skeleton->GetBindData().restLocals.ToSpan()) < 1e-6f);
}

namespace {

AnimationStateTransition MakeTransition(uint32 from, uint32 to, float duration, Array<AnimationTransitionCondition> conditions, int32 priority = 0)
{
    AnimationStateTransition transition;
    transition.from = from;
    transition.to = to;
    transition.duration = duration;
    transition.conditions = std::move(conditions);
    transition.priority = priority;

    return transition;
}

String DescribeStateMachine(const AnimationGraph& graph, const AnimationGraphInstance& instance, uint32 nodeIndex)
{
    const AnimationStateMachineNode* stateMachine = TryCast<AnimationStateMachineNode>(graph.GetNode(nodeIndex));
    const AnimationGraphNodeState* nodeState = instance.GetNodeState(nodeIndex);

    return stateMachine && nodeState ? stateMachine->DescribeState(*nodeState) : String::empty;
}

} // namespace

HYP_ENGINE_TEST(AnimationGraph, StateMachineCrossfadesOnCondition)
{
    GraphTestRig rig = LoadGraphTestRig(context);
    HYP_TEST_REQUIRE(rig.skeleton.IsValid() && rig.wave.IsValid() && rig.bend.IsValid());

    Handle<AnimationGraph> graph = MakeHandle<AnimationGraph>(NAME("StateGraph"));
    graph->AddParameter(NAME("Speed"), AnimationGraphParameterType::Float);

    const uint32 idleClip = AddClipNode(*graph, rig.wave);
    const uint32 moveClip = AddClipNode(*graph, rig.bend);

    Handle<AnimationStateMachineNode> stateMachine = MakeHandle<AnimationStateMachineNode>();
    stateMachine->states = { AnimationState { NAME("Idle"), idleClip }, AnimationState { NAME("Move"), moveClip } };
    stateMachine->transitions = {
        MakeTransition(0, 1, 0.2f, { AnimationTransitionCondition { NAME("Speed"), AnimationConditionOp::Greater, 0.5f } }),
        MakeTransition(1, 0, 0.0f, { AnimationTransitionCondition { NAME("Speed"), AnimationConditionOp::Less, 0.5f } })
    };

    const uint32 stateMachineIndex = graph->AddNode(stateMachine);
    graph->SetRootNode(stateMachineIndex);

    Handle<AnimationGraphInstance> instance = MakeHandle<AnimationGraphInstance>(graph);

    instance->Update(0.1f, *rig.skeleton);
    HYP_TEST_CHECK(instance->GetNodeWeight(idleClip) == 1.0f && instance->GetNodeWeight(moveClip) == 0.0f);
    HYP_TEST_CHECK(DescribeStateMachine(*graph, *instance, stateMachineIndex) == "Idle");

    // the transition starts at zero blend, then crossfades over its duration
    instance->SetFloat(NAME("Speed"), 1.0f);
    instance->Update(0.1f, *rig.skeleton);
    instance->Update(0.1f, *rig.skeleton);

    HYP_TEST_CHECK_MSG(MathUtil::Abs(instance->GetNodeWeight(moveClip) - 0.5f) < 1e-4f, "move weight {}", instance->GetNodeWeight(moveClip));
    HYP_TEST_CHECK(MathUtil::Abs(instance->GetNodeWeight(idleClip) - 0.5f) < 1e-4f);
    HYP_TEST_CHECK_MSG(DescribeStateMachine(*graph, *instance, stateMachineIndex) == "Idle -> Move 50%", "described as {}", DescribeStateMachine(*graph, *instance, stateMachineIndex));

    const Array<Transform> blendedPose = EvaluatedPose(*instance, *rig.skeleton);

    Array<Transform> expectedPose = SampledPose(rig.skeleton->GetBindData(), *rig.wave, instance->GetNodeTime(idleClip));
    const Array<Transform> movePose = SampledPose(rig.skeleton->GetBindData(), *rig.bend, instance->GetNodeTime(moveClip));
    PoseOps::BlendPoses(expectedPose.ToSpan(), movePose.ToSpan(), 0.5f, {}, expectedPose.ToSpan());

    HYP_TEST_CHECK(LargestPoseDifference(blendedPose.ToSpan(), expectedPose.ToSpan()) < 1e-4f);

    instance->Update(0.1f, *rig.skeleton);
    HYP_TEST_CHECK(instance->GetNodeWeight(moveClip) == 1.0f && instance->GetNodeWeight(idleClip) == 0.0f);

    // a zero-length transition switches at once
    instance->SetFloat(NAME("Speed"), 0.0f);
    instance->Update(0.1f, *rig.skeleton);
    HYP_TEST_CHECK(instance->GetNodeWeight(idleClip) == 1.0f && instance->GetNodeWeight(moveClip) == 0.0f);
}

HYP_ENGINE_TEST(AnimationGraph, AnyStateTriggerWinsByPriority)
{
    GraphTestRig rig = LoadGraphTestRig(context);
    HYP_TEST_REQUIRE(rig.skeleton.IsValid() && rig.wave.IsValid() && rig.bend.IsValid());

    Handle<AnimationGraph> graph = MakeHandle<AnimationGraph>(NAME("AnyStateGraph"));
    graph->AddParameter(NAME("Hit"), AnimationGraphParameterType::Trigger);

    const uint32 firstClip = AddClipNode(*graph, rig.wave);
    const uint32 secondClip = AddClipNode(*graph, rig.bend);
    const uint32 hitClip = AddClipNode(*graph, rig.wave);

    AnimationStateTransition anyToHit = MakeTransition(0, 2, 0.0f, { AnimationTransitionCondition { NAME("Hit"), AnimationConditionOp::Triggered, 0.0f } }, 1);
    anyToHit.fromAnyState = true;

    Handle<AnimationStateMachineNode> stateMachine = MakeHandle<AnimationStateMachineNode>();
    stateMachine->states = { AnimationState { NAME("First"), firstClip }, AnimationState { NAME("Second"), secondClip }, AnimationState { NAME("Hit"), hitClip } };
    stateMachine->transitions = {
        MakeTransition(0, 1, 0.0f, { AnimationTransitionCondition { Name::Invalid(), AnimationConditionOp::StateTimeAtLeast, 0.25f } }),
        anyToHit
    };

    graph->SetRootNode(graph->AddNode(stateMachine));

    Handle<AnimationGraphInstance> instance = MakeHandle<AnimationGraphInstance>(graph);

    for (uint32 update = 0; update < 3; ++update)
    {
        instance->Update(0.1f, *rig.skeleton);
    }

    HYP_TEST_CHECK(instance->GetNodeWeight(firstClip) == 1.0f);

    // both transitions are ready on this update; the any-state one has the higher priority
    instance->SetTrigger(NAME("Hit"));
    instance->Update(0.1f, *rig.skeleton);

    HYP_TEST_CHECK(instance->GetNodeWeight(hitClip) == 1.0f && instance->GetNodeWeight(secondClip) == 0.0f);

    instance->Update(0.1f, *rig.skeleton);
    HYP_TEST_CHECK(instance->GetNodeWeight(hitClip) == 1.0f);
}

HYP_ENGINE_TEST(AnimationGraph, AnyStateInterruptsTransition)
{
    GraphTestRig rig = LoadGraphTestRig(context);
    HYP_TEST_REQUIRE(rig.skeleton.IsValid() && rig.wave.IsValid() && rig.bend.IsValid());

    Handle<AnimationGraph> graph = MakeHandle<AnimationGraph>(NAME("InterruptGraph"));
    graph->AddParameter(NAME("Go"), AnimationGraphParameterType::Bool);
    graph->AddParameter(NAME("Fall"), AnimationGraphParameterType::Bool);

    const uint32 firstClip = AddClipNode(*graph, rig.wave);
    const uint32 secondClip = AddClipNode(*graph, rig.bend);
    const uint32 fallClip = AddClipNode(*graph, rig.wave);

    AnimationStateTransition anyToFall = MakeTransition(0, 2, 0.2f, { AnimationTransitionCondition { NAME("Fall"), AnimationConditionOp::IsTrue, 0.0f } });
    anyToFall.fromAnyState = true;

    Handle<AnimationStateMachineNode> stateMachine = MakeHandle<AnimationStateMachineNode>();
    stateMachine->states = { AnimationState { NAME("First"), firstClip }, AnimationState { NAME("Second"), secondClip }, AnimationState { NAME("Fall"), fallClip } };
    stateMachine->transitions = {
        MakeTransition(0, 1, 0.4f, { AnimationTransitionCondition { NAME("Go"), AnimationConditionOp::IsTrue, 0.0f } }),
        anyToFall
    };

    const uint32 stateMachineIndex = graph->AddNode(stateMachine);
    graph->SetRootNode(stateMachineIndex);

    Handle<AnimationGraphInstance> instance = MakeHandle<AnimationGraphInstance>(graph);
    instance->Update(0.1f, *rig.skeleton);

    instance->SetBool(NAME("Go"), true);
    instance->Update(0.1f, *rig.skeleton);
    instance->Update(0.1f, *rig.skeleton);

    HYP_TEST_CHECK_MSG(DescribeStateMachine(*graph, *instance, stateMachineIndex) == "First -> Second 25%", "described as {}", DescribeStateMachine(*graph, *instance, stateMachineIndex));

    // a quarter of the way to Second, so the fall fades out of First
    instance->SetBool(NAME("Fall"), true);
    instance->Update(0.1f, *rig.skeleton);
    instance->Update(0.1f, *rig.skeleton);

    HYP_TEST_CHECK_MSG(DescribeStateMachine(*graph, *instance, stateMachineIndex) == "First -> Fall 50%", "described as {}", DescribeStateMachine(*graph, *instance, stateMachineIndex));
    HYP_TEST_CHECK(instance->GetNodeWeight(secondClip) == 0.0f);
    HYP_TEST_CHECK(MathUtil::Abs(instance->GetNodeWeight(firstClip) - 0.5f) < 1e-4f);
}

HYP_ENGINE_TEST(AnimationGraph, TimeRemainingEndsOneShot)
{
    GraphTestRig rig = LoadGraphTestRig(context);
    HYP_TEST_REQUIRE(rig.skeleton.IsValid() && rig.wave.IsValid() && rig.bend.IsValid());

    constexpr float Delta = 0.05f;
    constexpr float Remaining = 0.15f;

    const float length = rig.wave->GetLength();
    HYP_TEST_REQUIRE(length > Remaining + Delta * 2.0f);

    Handle<AnimationGraph> graph = MakeHandle<AnimationGraph>(NAME("OneShotGraph"));

    Handle<AnimationClipNode> shotNode = MakeHandle<AnimationClipNode>();
    shotNode->clip = rig.wave;
    shotNode->loop = false;

    const uint32 shotClip = graph->AddNode(shotNode);
    const uint32 idleClip = AddClipNode(*graph, rig.bend);

    Handle<AnimationStateMachineNode> stateMachine = MakeHandle<AnimationStateMachineNode>();
    stateMachine->states = { AnimationState { NAME("Shot"), shotClip }, AnimationState { NAME("Idle"), idleClip } };
    stateMachine->transitions = {
        MakeTransition(0, 1, 0.0f, { AnimationTransitionCondition { Name::Invalid(), AnimationConditionOp::StateTimeRemainingBelow, Remaining } })
    };

    graph->SetRootNode(graph->AddNode(stateMachine));

    Handle<AnimationGraphInstance> instance = MakeHandle<AnimationGraphInstance>(graph);

    float lastShotTime = 0.0f;
    bool switched = false;

    for (uint32 update = 0; update < 1000 && !switched; ++update)
    {
        lastShotTime = instance->GetNodeTime(shotClip);

        instance->Update(Delta, *rig.skeleton);

        switched = instance->GetNodeWeight(idleClip) == 1.0f;
    }

    HYP_TEST_REQUIRE(switched);

    // it switches on the first update after less than Remaining was left, not before
    HYP_TEST_CHECK_MSG(length - lastShotTime < Remaining, "switched with {} left", length - lastShotTime);
    HYP_TEST_CHECK_MSG(length - (lastShotTime - Delta) >= Remaining - 1e-4f, "should have switched earlier, at {}", lastShotTime - Delta);
}

HYP_ENGINE_TEST(AnimationGraph, RemovingStateFixesTransitions)
{
    Handle<AnimationStateMachineNode> stateMachine = MakeHandle<AnimationStateMachineNode>();
    stateMachine->AddInput(InvalidAnimationGraphNode);
    stateMachine->AddInput(InvalidAnimationGraphNode);
    stateMachine->AddInput(InvalidAnimationGraphNode);
    stateMachine->transitions = { MakeTransition(0, 1, 0.1f, {}), MakeTransition(1, 2, 0.1f, {}), MakeTransition(2, 0, 0.1f, {}) };
    stateMachine->entryState = 2;

    stateMachine->RemoveInput(1);

    HYP_TEST_CHECK(stateMachine->states.Size() == 2 && stateMachine->states[1].name == NAME("State2"));
    HYP_TEST_REQUIRE(stateMachine->transitions.Size() == 1);
    HYP_TEST_CHECK(stateMachine->transitions[0].from == 1 && stateMachine->transitions[0].to == 0);
    HYP_TEST_CHECK(stateMachine->entryState == 1);
}

HYP_ENGINE_TEST(AnimationGraph, StateMachineRoundTripsThroughHMF)
{
    Handle<AnimationGraph> graph = MakeHandle<AnimationGraph>(NAME("SavedStateGraph"));
    graph->AddParameter(NAME("Grounded"), AnimationGraphParameterType::Bool);

    AnimationStateTransition fall = MakeTransition(0, 1, 0.25f, { AnimationTransitionCondition { NAME("Grounded"), AnimationConditionOp::IsFalse, 0.0f } }, 3);
    fall.fromAnyState = true;
    fall.curve = AnimationBlendCurve::SmoothStep;

    Handle<AnimationStateMachineNode> stateMachine = MakeHandle<AnimationStateMachineNode>();
    stateMachine->states = { AnimationState { NAME("Ground"), InvalidAnimationGraphNode }, AnimationState { NAME("Air"), InvalidAnimationGraphNode } };
    stateMachine->transitions = { fall };
    stateMachine->entryState = 1;

    graph->SetRootNode(graph->AddNode(stateMachine));

    String text;
    HYP_TEST_REQUIRE(!graph->SaveContents(text).HasError());

    Handle<AnimationGraph> loaded = MakeHandle<AnimationGraph>(NAME("LoadedStateGraph"));
    HYP_TEST_REQUIRE(!loaded->LoadContents(text).HasError());

    const AnimationStateMachineNode* loadedMachine = TryCast<AnimationStateMachineNode>(loaded->GetNode(0));
    HYP_TEST_CHECK_MSG(loadedMachine != nullptr, "didn't read back a state machine: {}", text);
    HYP_TEST_REQUIRE(loadedMachine != nullptr);

    HYP_TEST_CHECK(loadedMachine->states.Size() == 2 && loadedMachine->states[1].name == NAME("Air"));
    HYP_TEST_CHECK(loadedMachine->entryState == 1);
    HYP_TEST_REQUIRE(loadedMachine->transitions.Size() == 1);

    const AnimationStateTransition& loadedFall = loadedMachine->transitions[0];

    HYP_TEST_CHECK(loadedFall.fromAnyState && loadedFall.to == 1 && loadedFall.duration == 0.25f && loadedFall.priority == 3);
    HYP_TEST_CHECK(loadedFall.curve == AnimationBlendCurve::SmoothStep);
    HYP_TEST_CHECK_MSG(loadedFall.conditions.Size() == 1, "conditions didn't read back: {}", text);
    HYP_TEST_REQUIRE(loadedFall.conditions.Size() == 1);
    HYP_TEST_CHECK(loadedFall.conditions[0].parameter == NAME("Grounded") && loadedFall.conditions[0].op == AnimationConditionOp::IsFalse);
}

HYP_ENGINE_TEST(AnimationGraph, InterruptibleTransitionGivesWay)
{
    GraphTestRig rig = LoadGraphTestRig(context);
    HYP_TEST_REQUIRE(rig.skeleton.IsValid() && rig.wave.IsValid() && rig.bend.IsValid());

    for (const bool interruptible : { false, true })
    {
        Handle<AnimationGraph> graph = MakeHandle<AnimationGraph>(NAME("InterruptibleGraph"));
        graph->AddParameter(NAME("Go"), AnimationGraphParameterType::Bool);
        graph->AddParameter(NAME("Next"), AnimationGraphParameterType::Bool);

        const uint32 firstClip = AddClipNode(*graph, rig.wave);
        const uint32 secondClip = AddClipNode(*graph, rig.bend);
        const uint32 thirdClip = AddClipNode(*graph, rig.wave);

        AnimationStateTransition firstToSecond = MakeTransition(0, 1, 0.4f, { AnimationTransitionCondition { NAME("Go"), AnimationConditionOp::IsTrue, 0.0f } });
        firstToSecond.interruptible = interruptible;

        Handle<AnimationStateMachineNode> stateMachine = MakeHandle<AnimationStateMachineNode>();
        stateMachine->states = { AnimationState { NAME("First"), firstClip }, AnimationState { NAME("Second"), secondClip }, AnimationState { NAME("Third"), thirdClip } };
        stateMachine->transitions = {
            firstToSecond,
            MakeTransition(1, 2, 0.0f, { AnimationTransitionCondition { NAME("Next"), AnimationConditionOp::IsTrue, 0.0f } })
        };

        graph->SetRootNode(graph->AddNode(stateMachine));

        Handle<AnimationGraphInstance> instance = MakeHandle<AnimationGraphInstance>(graph);
        instance->Update(0.1f, *rig.skeleton);

        instance->SetBool(NAME("Go"), true);
        instance->Update(0.1f, *rig.skeleton);

        instance->SetBool(NAME("Next"), true);
        instance->Update(0.1f, *rig.skeleton);

        if (interruptible)
        {
            HYP_TEST_CHECK_MSG(instance->GetNodeWeight(thirdClip) == 1.0f, "an interruptible transition should give way, third weight {}", instance->GetNodeWeight(thirdClip));
        }
        else
        {
            HYP_TEST_CHECK_MSG(instance->GetNodeWeight(thirdClip) == 0.0f, "a running transition was interrupted, third weight {}", instance->GetNodeWeight(thirdClip));

            // once it finishes, the waiting transition goes
            for (uint32 update = 0; update < 5; ++update)
            {
                instance->Update(0.1f, *rig.skeleton);
            }

            HYP_TEST_CHECK(instance->GetNodeWeight(thirdClip) == 1.0f);
        }
    }
}

HYP_ENGINE_TEST(AnimationGraph, DebugSnapshotOnlyWhileCaptured)
{
    GraphTestRig rig = LoadGraphTestRig(context);
    HYP_TEST_REQUIRE(rig.skeleton.IsValid() && rig.wave.IsValid() && rig.bend.IsValid());

    Handle<AnimationGraph> graph = MakeHandle<AnimationGraph>(NAME("DebugGraph"));
    graph->AddParameter(NAME("Speed"), AnimationGraphParameterType::Float);

    const uint32 idleClip = AddClipNode(*graph, rig.wave);
    const uint32 moveClip = AddClipNode(*graph, rig.bend);

    Handle<AnimationStateMachineNode> stateMachine = MakeHandle<AnimationStateMachineNode>();
    stateMachine->states = { AnimationState { NAME("Idle"), idleClip }, AnimationState { NAME("Move"), moveClip } };
    stateMachine->transitions = { MakeTransition(0, 1, 0.2f, { AnimationTransitionCondition { NAME("Speed"), AnimationConditionOp::Greater, 0.5f } }) };

    const uint32 stateMachineIndex = graph->AddNode(stateMachine);
    graph->SetRootNode(stateMachineIndex);

    Handle<AnimationGraphInstance> instance = MakeHandle<AnimationGraphInstance>(graph);
    instance->Update(0.1f, *rig.skeleton);

    HYP_TEST_CHECK(instance->GetDebugNodeWeight(idleClip) == 0.0f);
    HYP_TEST_CHECK(instance->GetDebugCurrentState(stateMachineIndex) == InvalidAnimationGraphNode);

    instance->SetDebugCapture(true);
    instance->SetFloat(NAME("Speed"), 1.0f);
    instance->Update(0.1f, *rig.skeleton);
    instance->Update(0.1f, *rig.skeleton);

    HYP_TEST_CHECK(MathUtil::Abs(instance->GetDebugNodeWeight(moveClip) - 0.5f) < 1e-4f);
    HYP_TEST_CHECK(instance->GetDebugCurrentState(stateMachineIndex) == 1 && instance->GetDebugPreviousState(stateMachineIndex) == 0);
    HYP_TEST_CHECK(instance->GetDebugTransition(stateMachineIndex) == 0);
    HYP_TEST_CHECK_MSG(instance->GetDebugNodeStatus(stateMachineIndex) == "Idle -> Move 50%", "status {}", instance->GetDebugNodeStatus(stateMachineIndex));
    HYP_TEST_CHECK(instance->GetDebugValue(NAME("Speed")).x == 1.0f);

    // the progress bar follows the clip that times the current state
    const float expectedProgress = instance->GetNodeTime(moveClip) / rig.bend->GetLength();
    HYP_TEST_CHECK_MSG(MathUtil::Abs(instance->GetDebugStateProgress(stateMachineIndex) - expectedProgress) < 1e-4f, "progress {}, expected {}", instance->GetDebugStateProgress(stateMachineIndex), expectedProgress);

    instance->SetDebugCapture(false);
    HYP_TEST_CHECK(instance->GetDebugNodeWeight(moveClip) == 0.0f);
}

HYP_ENGINE_TEST(AnimationGraph, InertializedTransitionEasesOutOldPose)
{
    GraphTestRig rig = LoadGraphTestRig(context);
    HYP_TEST_REQUIRE(rig.skeleton.IsValid() && rig.wave.IsValid() && rig.bend.IsValid());

    Handle<AnimationGraph> graph = MakeHandle<AnimationGraph>(NAME("InertializeGraph"));
    graph->AddParameter(NAME("Speed"), AnimationGraphParameterType::Float);

    const uint32 idleClip = AddClipNode(*graph, rig.wave);
    const uint32 moveClip = AddClipNode(*graph, rig.bend);

    AnimationStateTransition toMove = MakeTransition(0, 1, 0.2f, { AnimationTransitionCondition { NAME("Speed"), AnimationConditionOp::Greater, 0.5f } });
    toMove.mode = AnimationTransitionMode::Inertialize;

    Handle<AnimationStateMachineNode> stateMachine = MakeHandle<AnimationStateMachineNode>();
    stateMachine->states = { AnimationState { NAME("Idle"), idleClip }, AnimationState { NAME("Move"), moveClip } };
    stateMachine->transitions = { toMove };

    const uint32 stateMachineIndex = graph->AddNode(stateMachine);
    graph->SetRootNode(stateMachineIndex);

    Handle<AnimationGraphInstance> instance = MakeHandle<AnimationGraphInstance>(graph);

    // far enough into the idle clip that it has moved away from the rest pose
    for (uint32 update = 0; update < 5; ++update)
    {
        instance->Update(0.1f, *rig.skeleton);
    }

    const Array<Transform> idlePose = EvaluatedPose(*instance, *rig.skeleton);

    // only the new state plays, starting from exactly what was last shown
    instance->SetFloat(NAME("Speed"), 1.0f);
    instance->Update(0.1f, *rig.skeleton);

    HYP_TEST_CHECK(instance->GetNodeWeight(moveClip) == 1.0f && instance->GetNodeWeight(idleClip) == 0.0f);
    HYP_TEST_CHECK_MSG(DescribeStateMachine(*graph, *instance, stateMachineIndex) == "Move (inertializing 0%)", "described as {}", DescribeStateMachine(*graph, *instance, stateMachineIndex));

    const Array<Transform> startPose = EvaluatedPose(*instance, *rig.skeleton);
    const Array<Transform> moveStartPose = SampledPose(rig.skeleton->GetBindData(), *rig.bend, instance->GetNodeTime(moveClip));

    const float stateGap = LargestPoseDifference(idlePose.ToSpan(), moveStartPose.ToSpan());
    HYP_TEST_CHECK_MSG(stateGap > 1e-2f, "the states differ by only {}", stateGap);
    HYP_TEST_REQUIRE(stateGap > 1e-2f);

    HYP_TEST_CHECK_MSG(LargestPoseDifference(startPose.ToSpan(), idlePose.ToSpan()) < 1e-3f, "start differs by {}", LargestPoseDifference(startPose.ToSpan(), idlePose.ToSpan()));

    // halfway the old pose's offset is half gone
    instance->Update(0.1f, *rig.skeleton);

    const Array<Transform> midPose = EvaluatedPose(*instance, *rig.skeleton);
    const Array<Transform> moveMidPose = SampledPose(rig.skeleton->GetBindData(), *rig.bend, instance->GetNodeTime(moveClip));

    HYP_TEST_CHECK(DescribeStateMachine(*graph, *instance, stateMachineIndex) == "Move (inertializing 50%)");
    HYP_TEST_CHECK_MSG(LargestPoseDifference(midPose.ToSpan(), moveMidPose.ToSpan()) > stateGap * 0.1f, "mid is {} from the new state", LargestPoseDifference(midPose.ToSpan(), moveMidPose.ToSpan()));

    instance->Update(0.1f, *rig.skeleton);

    const Array<Transform> endPose = EvaluatedPose(*instance, *rig.skeleton);
    const Array<Transform> moveEndPose = SampledPose(rig.skeleton->GetBindData(), *rig.bend, instance->GetNodeTime(moveClip));

    HYP_TEST_CHECK(DescribeStateMachine(*graph, *instance, stateMachineIndex) == "Move");
    HYP_TEST_CHECK_MSG(LargestPoseDifference(endPose.ToSpan(), moveEndPose.ToSpan()) < 1e-4f, "end differs by {}", LargestPoseDifference(endPose.ToSpan(), moveEndPose.ToSpan()));
}

HYP_ENGINE_TEST(AnimationGraph, SlotPlaysOverBaseAndBlendsOut)
{
    GraphTestRig rig = LoadGraphTestRig(context);
    HYP_TEST_REQUIRE(rig.skeleton.IsValid() && rig.wave.IsValid() && rig.bend.IsValid());

    constexpr float Delta = 0.05f;
    constexpr float BlendTime = 0.1f;

    Handle<AnimationGraph> graph = MakeHandle<AnimationGraph>(NAME("SlotGraph"));

    const uint32 baseClip = AddClipNode(*graph, rig.wave);

    Handle<AnimationSlotNode> slotNode = MakeHandle<AnimationSlotNode>();
    slotNode->slot = NAME("Action");
    slotNode->base = baseClip;

    graph->SetRootNode(graph->AddNode(slotNode));

    Handle<AnimationGraphInstance> instance = MakeHandle<AnimationGraphInstance>(graph);
    instance->Update(Delta, *rig.skeleton);

    instance->PlaySlot(NAME("Action"), rig.bend, BlendTime, 1.0f);
    HYP_TEST_CHECK(instance->IsSlotPlaying(NAME("Action")));

    instance->Update(Delta, *rig.skeleton);
    instance->Update(Delta, *rig.skeleton);

    // fully blended in, so the pose is the slot clip alone
    const Array<Transform> slotPose = EvaluatedPose(*instance, *rig.skeleton);
    const Array<Transform> expectedSlotPose = SampledPose(rig.skeleton->GetBindData(), *rig.bend, Delta * 2.0f);

    HYP_TEST_CHECK_MSG(LargestPoseDifference(slotPose.ToSpan(), expectedSlotPose.ToSpan()) < 1e-4f, "slot pose differs by {}", LargestPoseDifference(slotPose.ToSpan(), expectedSlotPose.ToSpan()));

    bool finished = false;

    for (uint32 update = 0; update < 1000 && !finished; ++update)
    {
        instance->Update(Delta, *rig.skeleton);

        finished = !instance->IsSlotPlaying(NAME("Action"));
    }

    HYP_TEST_REQUIRE(finished);

    const Array<Transform> basePose = EvaluatedPose(*instance, *rig.skeleton);
    const Array<Transform> expectedBasePose = SampledPose(rig.skeleton->GetBindData(), *rig.wave, instance->GetNodeTime(baseClip));

    HYP_TEST_CHECK(LargestPoseDifference(basePose.ToSpan(), expectedBasePose.ToSpan()) < 1e-5f);
}

HYP_ENGINE_TEST(AnimationGraph, HumanoidLocomotionFollowsSignals)
{
    if (!GetEngineAssetRegistry().IsValid())
    {
        Handle<AssetRegistry> engineRegistry = MakeHandle<AssetRegistry>(AssetRegistryId::Engine, EngineGlobals::GetContentDirectory<HYP_STATIC_STRING("Engine")>());
        engineRegistry->Initialize(nullptr);

        SetEngineAssetRegistry(engineRegistry);
    }

    // the test host doesn't enable plugins, so the graph's plugin content is mounted here
    for (const FilePath& contentDirectory : PluginManager::GetInstance().GetPluginContentDirectories({ NAME("AnimToolkit") }))
    {
        GetEngineAssetRegistry()->AddContentRoot(contentDirectory);
    }

    Handle<AnimationGraph> graph = GetEngineAssetRegistry()->GetAsset<AnimationGraph>(AssetBuckets::Animations, NAME("HumanoidLocomotion"));
    Handle<Skeleton> skeleton = GetEngineAssetRegistry()->GetAsset<Skeleton>(AssetBuckets::Skeletons, NAME("BaseHumanRig"));

    HYP_TEST_CHECK_MSG(graph.IsValid(), "the engine has no HumanoidLocomotion graph; it is AnimToolkit plugin content (Source/Plugins/AnimToolkit/Content), built by BuildAnimationContentCommandlet");
    HYP_TEST_REQUIRE(graph.IsValid() && skeleton.IsValid());

    // the animation system holds the graph's tracks resident while it plays; clip lengths and poses need them
    Array<UniquePtr<TSharedResLock<AssetObject>>> trackLocks;

    for (const Handle<Animation>& clip : graph->CollectClips())
    {
        for (const Handle<AnimationTrack>& track : clip->GetTracks())
        {
            if (track.IsValid())
            {
                trackLocks.EmplaceBack(MakeUnique<TSharedResLock<AssetObject>>(*track));
            }
        }
    }

    Handle<AnimationGraphInstance> instance = MakeHandle<AnimationGraphInstance>(graph);

    const auto weight = [&](const char* nodeName)
    {
        return instance->GetNodeWeight(graph->FindNode(Name(nodeName)));
    };

    const auto update = [&](uint32 numUpdates = 1)
    {
        for (uint32 updateIndex = 0; updateIndex < numUpdates; ++updateIndex)
        {
            instance->Update(0.05f, *skeleton);
        }
    };

    update();
    HYP_TEST_CHECK(weight("Idle") == 1.0f);

    instance->SetFloat(NAME("Speed"), 1.02f);
    update();
    HYP_TEST_CHECK_MSG(weight("Walk") == 1.0f, "walk weight {}", weight("Walk"));

    instance->SetFloat(NAME("MoveAngle"), 90.0f);
    update();
    HYP_TEST_CHECK(weight("StrafeRight") == 1.0f);

    instance->SetFloat(NAME("MoveAngle"), -135.0f);
    update();
    HYP_TEST_CHECK(MathUtil::Abs(weight("StrafeLeft") - 0.5f) < 1e-4f && MathUtil::Abs(weight("WalkBackward") - 0.5f) < 1e-4f);

    // between walk and run both play, in one phase
    instance->SetFloat(NAME("MoveAngle"), 0.0f);
    instance->SetFloat(NAME("Speed"), 4.8f);
    update(10);

    const uint32 walk = graph->FindNode(NAME("Walk"));
    const uint32 run = graph->FindNode(NAME("Run"));
    const float walkPhase = instance->GetNodeTime(walk) / static_cast<const AnimationClipNode*>(graph->GetNode(walk))->clip->GetLength();
    const float runPhase = instance->GetNodeTime(run) / static_cast<const AnimationClipNode*>(graph->GetNode(run))->clip->GetLength();

    HYP_TEST_CHECK(weight("Walk") > 0.0f && weight("Run") > 0.0f);
    HYP_TEST_CHECK_MSG(MathUtil::Abs(walkPhase - runPhase) < 1e-3f, "walk phase {}, run phase {}", walkPhase, runPhase);

    instance->SetFloat(NAME("Speed"), 0.0f);
    instance->SetFloat(NAME("Windup"), 1.0f);
    update(10);
    HYP_TEST_CHECK_MSG(weight("JumpWindup") == 1.0f, "windup weight {}", weight("JumpWindup"));

    instance->SetBool(NAME("Airborne"), true);
    instance->SetBool(NAME("Jumping"), true);
    update(4);
    HYP_TEST_CHECK_MSG(weight("Jump") == 1.0f, "jump weight {}", weight("Jump"));

    instance->SetFloat(NAME("Windup"), 0.0f);
    update(40);
    HYP_TEST_CHECK_MSG(weight("Fall") == 1.0f, "a long jump should end up falling, fall weight {}", weight("Fall"));

    instance->SetBool(NAME("Airborne"), false);
    instance->SetBool(NAME("Jumping"), false);
    update();
    HYP_TEST_CHECK(weight("Land") == 1.0f);

    update(60);
    HYP_TEST_CHECK_MSG(weight("Idle") == 1.0f, "back to idle after landing, idle weight {}", weight("Idle"));

    // a small left turn step over the idle
    instance->SetFloat(NAME("TurnStep"), 1.0f);
    instance->SetFloat(NAME("TurnStepKind"), 2.0f);
    instance->SetFloat(NAME("TurnStepProgress"), 0.5f);
    update();

    const uint32 turnLeftSmall = graph->FindNode(NAME("TurnLeftSmall"));

    HYP_TEST_CHECK(weight("TurnLeftSmall") == 1.0f);
    HYP_TEST_CHECK(MathUtil::Abs(instance->GetNodeTime(turnLeftSmall) - 0.5f * static_cast<const AnimationClipNode*>(graph->GetNode(turnLeftSmall))->clip->GetLength()) < 1e-4f);
}

} // namespace scene
} // namespace tests
} // namespace Hyperion

#endif // HYP_TESTS
