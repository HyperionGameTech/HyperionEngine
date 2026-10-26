/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/Animation/AnimationGraph.hpp>
#include <Scene/Animation/AnimationGraphInstance.hpp>
#include <Scene/Animation/AnimationPose.hpp>
#include <Scene/Animation/Animation.hpp>
#include <Scene/Animation/BoneMask.hpp>
#include <Scene/Animation/Skeleton.hpp>

#include <Core/Math/MathUtil.hpp>

#include <cmath>

#include <AnimationGraph.generated.inl>

namespace Hyperion {

namespace {

// blends each active input into outPose, keeping a running normalized weight
void EvaluateWeightedInputs(AnimationGraphContext& context, const Array<AnimationBlendSample>& samples, const Array<float>& weights, Span<Transform> outPose)
{
    float accumulatedWeight = 0.0f;

    for (size_t sampleIndex = 0; sampleIndex < samples.Size(); ++sampleIndex)
    {
        const float sampleWeight = weights[sampleIndex];

        if (sampleWeight <= 0.0f)
        {
            continue;
        }

        if (accumulatedWeight <= 0.0f)
        {
            context.EvaluateInput(samples[sampleIndex].input, outPose);
        }
        else
        {
            Span<Transform> samplePose = context.AcquireScratchPose();
            context.EvaluateInput(samples[sampleIndex].input, samplePose);

            PoseOps::BlendPoses(outPose, samplePose, sampleWeight / (accumulatedWeight + sampleWeight), {}, outPose);

            context.ReleaseScratchPose();
        }

        accumulatedWeight += sampleWeight;
    }

    if (accumulatedWeight <= 0.0f)
    {
        context.EvaluateInput(InvalidAnimationGraphNode, outPose);
    }
}

void UpdateWeightedInputs(AnimationGraphContext& context, const Array<AnimationBlendSample>& samples, const Array<float>& weights, float weight)
{
    for (size_t sampleIndex = 0; sampleIndex < samples.Size(); ++sampleIndex)
    {
        if (weights[sampleIndex] > 0.0f)
        {
            context.UpdateInput(samples[sampleIndex].input, weight * weights[sampleIndex]);
        }
    }
}

void GetSampleInputs(const Array<AnimationBlendSample>& samples, Array<uint32>& outInputs)
{
    for (const AnimationBlendSample& sample : samples)
    {
        outInputs.PushBack(sample.input);
    }
}

float ResolveWeight(const AnimationGraphContext& context, Name weightParameter, float weight)
{
    return MathUtil::Clamp(weightParameter.IsValid() ? context.GetFloat(weightParameter, weight) : weight, 0.0f, 1.0f);
}

} // namespace

#pragma region AnimationClipNode

void AnimationClipNode::Update(AnimationGraphContext& context, uint32 nodeIndex, float weight) const
{
    context.GetState(nodeIndex).weight += weight;
    context.RegisterActiveClip(nodeIndex);
}

void AnimationClipNode::Evaluate(AnimationGraphContext& context, uint32 nodeIndex, Span<Transform> outPose) const
{
    if (!clip.IsValid())
    {
        context.EvaluateInput(InvalidAnimationGraphNode, outPose);

        return;
    }

    PoseOps::SampleClip(*clip, context.GetClipBinding(*clip), context.GetBindData(), context.GetState(nodeIndex).time, outPose);
}

#pragma endregion AnimationClipNode

#pragma region AnimationBlend1DNode

void AnimationBlend1DNode::GetInputs(Array<uint32>& outInputs) const
{
    GetSampleInputs(samples, outInputs);
}

void AnimationBlend1DNode::ComputeWeights(float value, Array<float>& outWeights) const
{
    outWeights.Resize(samples.Size());

    for (float& sampleWeight : outWeights)
    {
        sampleWeight = 0.0f;
    }

    if (samples.Empty())
    {
        return;
    }

    // the nearest samples below and above value, so samples needn't be sorted
    uint32 below = InvalidAnimationGraphNode;
    uint32 above = InvalidAnimationGraphNode;

    for (uint32 sampleIndex = 0; sampleIndex < uint32(samples.Size()); ++sampleIndex)
    {
        const float position = samples[sampleIndex].position.x;

        if (position <= value && (below == InvalidAnimationGraphNode || position > samples[below].position.x))
        {
            below = sampleIndex;
        }

        if (position >= value && (above == InvalidAnimationGraphNode || position < samples[above].position.x))
        {
            above = sampleIndex;
        }
    }

    if (below == InvalidAnimationGraphNode || above == InvalidAnimationGraphNode || below == above)
    {
        outWeights[below != InvalidAnimationGraphNode ? below : above] = 1.0f;

        return;
    }

    const float span = samples[above].position.x - samples[below].position.x;
    const float alpha = span > 0.0f ? (value - samples[below].position.x) / span : 0.0f;

    outWeights[below] = 1.0f - alpha;
    outWeights[above] = alpha;
}

void AnimationBlend1DNode::Update(AnimationGraphContext& context, uint32 nodeIndex, float weight) const
{
    context.GetState(nodeIndex).weight += weight;

    Array<float> weights;
    ComputeWeights(context.GetFloat(parameter), weights);

    UpdateWeightedInputs(context, samples, weights, weight);
}

void AnimationBlend1DNode::Evaluate(AnimationGraphContext& context, uint32 nodeIndex, Span<Transform> outPose) const
{
    Array<float> weights;
    ComputeWeights(context.GetFloat(parameter), weights);

    EvaluateWeightedInputs(context, samples, weights, outPose);
}

#pragma endregion AnimationBlend1DNode

#pragma region AnimationBlend2DNode

void AnimationBlend2DNode::GetInputs(Array<uint32>& outInputs) const
{
    GetSampleInputs(samples, outInputs);
}

void AnimationBlend2DNode::ComputeWeights(const Vec2f& value, Array<float>& outWeights) const
{
    outWeights.Resize(samples.Size());

    float totalWeight = 0.0f;

    for (size_t sampleIndex = 0; sampleIndex < samples.Size(); ++sampleIndex)
    {
        const Vec2f& position = samples[sampleIndex].position;
        const Vec2f toValue = value - position;

        float sampleWeight = 1.0f;

        for (size_t otherIndex = 0; otherIndex < samples.Size() && sampleWeight > 0.0f; ++otherIndex)
        {
            if (otherIndex == sampleIndex)
            {
                continue;
            }

            const Vec2f toOther = samples[otherIndex].position - position;
            const float lengthSquared = toOther.Dot(toOther);

            if (lengthSquared <= MathUtil::epsilonF)
            {
                continue;
            }

            sampleWeight = MathUtil::Min(sampleWeight, MathUtil::Clamp(1.0f - toValue.Dot(toOther) / lengthSquared, 0.0f, 1.0f));
        }

        outWeights[sampleIndex] = sampleWeight;
        totalWeight += sampleWeight;
    }

    for (float& sampleWeight : outWeights)
    {
        sampleWeight = totalWeight > 0.0f ? sampleWeight / totalWeight : 0.0f;
    }
}

void AnimationBlend2DNode::Update(AnimationGraphContext& context, uint32 nodeIndex, float weight) const
{
    context.GetState(nodeIndex).weight += weight;

    Array<float> weights;
    ComputeWeights(context.GetVector2(parameter), weights);

    UpdateWeightedInputs(context, samples, weights, weight);
}

void AnimationBlend2DNode::Evaluate(AnimationGraphContext& context, uint32 nodeIndex, Span<Transform> outPose) const
{
    Array<float> weights;
    ComputeWeights(context.GetVector2(parameter), weights);

    EvaluateWeightedInputs(context, samples, weights, outPose);
}

#pragma endregion AnimationBlend2DNode

#pragma region AnimationLayerNode

void AnimationLayerNode::GetInputs(Array<uint32>& outInputs) const
{
    outInputs.PushBack(base);
    outInputs.PushBack(layer);
}

void AnimationLayerNode::Update(AnimationGraphContext& context, uint32 nodeIndex, float weight) const
{
    context.GetState(nodeIndex).weight += weight;
    context.UpdateInput(base, weight);

    if (const float layerWeight = ResolveWeight(context, weightParameter, this->weight); layerWeight > 0.0f)
    {
        context.UpdateInput(layer, weight * layerWeight);
    }
}

void AnimationLayerNode::Evaluate(AnimationGraphContext& context, uint32 nodeIndex, Span<Transform> outPose) const
{
    context.EvaluateInput(base, outPose);

    const float layerWeight = ResolveWeight(context, weightParameter, weight);

    if (layerWeight <= 0.0f)
    {
        return;
    }

    Span<Transform> layerPose = context.AcquireScratchPose();
    context.EvaluateInput(layer, layerPose);

    PoseOps::BlendPoses(outPose, layerPose, layerWeight, mask.IsValid() ? context.GetMaskWeights(*mask) : Span<const float>(), outPose);

    context.ReleaseScratchPose();
}

#pragma endregion AnimationLayerNode

#pragma region AnimationAdditiveNode

void AnimationAdditiveNode::GetInputs(Array<uint32>& outInputs) const
{
    outInputs.PushBack(base);
    outInputs.PushBack(additive);
}

void AnimationAdditiveNode::Update(AnimationGraphContext& context, uint32 nodeIndex, float weight) const
{
    context.GetState(nodeIndex).weight += weight;
    context.UpdateInput(base, weight);

    if (ResolveWeight(context, weightParameter, this->weight) > 0.0f)
    {
        // an additive pose doesn't replace anything, so it runs at full rate whatever its share
        context.UpdateInput(additive, weight);
    }
}

void AnimationAdditiveNode::Evaluate(AnimationGraphContext& context, uint32 nodeIndex, Span<Transform> outPose) const
{
    context.EvaluateInput(base, outPose);

    const float additiveWeight = ResolveWeight(context, weightParameter, weight);

    if (additiveWeight <= 0.0f)
    {
        return;
    }

    Span<Transform> additivePose = context.AcquireScratchPose();
    context.EvaluateInput(additive, additivePose);

    PoseOps::ApplyAdditive(context.GetBindData(), additivePose, additiveWeight, outPose);

    context.ReleaseScratchPose();
}

#pragma endregion AnimationAdditiveNode

#pragma region AnimationChainTwistNode

void AnimationChainTwistNode::GetInputs(Array<uint32>& outInputs) const
{
    outInputs.PushBack(input);
}

void AnimationChainTwistNode::Update(AnimationGraphContext& context, uint32 nodeIndex, float weight) const
{
    AnimationGraphNodeState& state = context.GetState(nodeIndex);
    state.weight += weight;

    if (context.BeginNodeUpdate(nodeIndex))
    {
        const float targetAngle = angleParameter.IsValid() ? context.GetFloat(angleParameter, angle) : angle;

        state.smoothedValue = smoothingTime > 0.0f && state.wasActive
            ? state.smoothedValue + (targetAngle - state.smoothedValue) * (1.0f - std::exp(-context.GetDelta() / smoothingTime))
            : targetAngle;
    }

    context.UpdateInput(input, weight);
}

void AnimationChainTwistNode::Evaluate(AnimationGraphContext& context, uint32 nodeIndex, Span<Transform> outPose) const
{
    context.EvaluateInput(input, outPose);

    const float twistAngle = context.GetState(nodeIndex).smoothedValue;

    if (twistAngle != 0.0f && rootBone.IsValid() && endBone.IsValid())
    {
        PoseOps::ApplyTwist(context.GetBindData(), rootBone, endBone, twistAngle, outPose);
    }
}

#pragma endregion AnimationChainTwistNode

#pragma region AnimationGraph

AnimationGraph::AnimationGraph() = default;

AnimationGraph::AnimationGraph(Name name)
{
    SetName(name);
}

AnimationGraph::~AnimationGraph() = default;

uint32 AnimationGraph::AddNode(const Handle<AnimationGraphNode>& node)
{
    m_nodes.PushBack(node);

    return uint32(m_nodes.Size() - 1);
}

void AnimationGraph::AddParameter(Name name, AnimationGraphParameterType type, const Vec2f& defaultValue)
{
    m_parameters.PushBack(AnimationGraphParameter { name, type, defaultValue });
}

Array<Name> AnimationGraph::GetParameterNames() const
{
    Array<Name> names;

    for (const AnimationGraphParameter& parameter : m_parameters)
    {
        names.PushBack(parameter.name);
    }

    return names;
}

Array<Handle<Animation>> AnimationGraph::CollectClips() const
{
    Array<Handle<Animation>> clips;

    for (const Handle<AnimationGraphNode>& node : m_nodes)
    {
        const AnimationClipNode* clipNode = node.IsValid() ? TryCast<AnimationClipNode>(node.Get()) : nullptr;

        if (clipNode && clipNode->clip.IsValid() && !clips.Contains(clipNode->clip))
        {
            clips.PushBack(clipNode->clip);
        }
    }

    return clips;
}

#pragma endregion AnimationGraph

} // namespace Hyperion
