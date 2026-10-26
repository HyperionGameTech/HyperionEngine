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
#include <Scene/Animation/AnimationPose.hpp>
#include <Scene/Animation/AnimationGraph.hpp>
#include <Scene/Animation/AnimationGraphInstance.hpp>
#include <Scene/Animation/BoneMask.hpp>

#include <Core/FileSystem/FilePath.hpp>

#include <Core/DataProcessing/HMF/HMF.hpp>

#include <Core/Utilities/GlobalContext.hpp>

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

    auto loadResult = g_assetManager->Load<Prefab>(FilePath(__FILE__).BasePath() / ".." / "Data" / "Animation" / "TwoTakeRig.glb", String::empty, AssetLoadHint::Transient);

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
    Array<Transform> pose = PoseOps::MakeRestPose(bindData);
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

    Array<Transform> expected = PoseOps::MakeRestPose(bindData);
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

    // the heavier clip leads, so it moves at its own rate
    const float expectedWaveTime = std::fmod(0.07f * 12.0f, rig.wave->GetLength());

    HYP_TEST_CHECK(MathUtil::Abs(instance->GetNodeTime(waveNode) - expectedWaveTime) < 1e-4f);
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

} // namespace scene
} // namespace tests
} // namespace Hyperion

#endif // HYP_TESTS
