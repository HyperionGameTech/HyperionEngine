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

#include <Asset/SerializationUtils.hpp>

#include <Core/DataProcessing/HMF/HMF.hpp>

#include <Core/Reflection/Class.hpp>
#include <Core/Reflection/ClassRegistry.hpp>

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

uint32 RenumberAfterRemoval(uint32 input, uint32 removedIndex)
{
    if (input == removedIndex)
    {
        return InvalidAnimationGraphNode;
    }

    return input != InvalidAnimationGraphNode && input > removedIndex ? input - 1 : input;
}

void RemoveSamplesOfNode(Array<AnimationBlendSample>& samples, uint32 removedIndex)
{
    for (auto it = samples.Begin(); it != samples.End();)
    {
        if (it->input == removedIndex)
        {
            it = samples.Erase(it);

            continue;
        }

        it->input = RenumberAfterRemoval(it->input, removedIndex);

        ++it;
    }
}

float ResolveWeight(const AnimationGraphContext& context, Name weightParameter, float weight)
{
    return MathUtil::Clamp(weightParameter.IsValid() ? context.GetFloat(weightParameter, weight) : weight, 0.0f, 1.0f);
}

} // namespace

#pragma region AnimationGraphNode

void AnimationGraphNode::OnNodeRemoved(uint32 removedIndex)
{
    for (uint32 slot = 0; slot < NumInputs(); ++slot)
    {
        SetInput(slot, RenumberAfterRemoval(GetInput(slot), removedIndex));
    }
}

void AnimationGraphNode::GetInputs(Array<uint32>& outInputs) const
{
    for (uint32 slot = 0; slot < NumInputs(); ++slot)
    {
        outInputs.PushBack(GetInput(slot));
    }
}

#pragma endregion AnimationGraphNode

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

uint32 AnimationBlend1DNode::NumInputs() const
{
    return uint32(samples.Size());
}

Name AnimationBlend1DNode::GetInputName(uint32 slot) const
{
    return slot < samples.Size() ? NAME_FMT("{}", samples[slot].position.x) : Name::Invalid();
}

uint32 AnimationBlend1DNode::GetInput(uint32 slot) const
{
    return slot < samples.Size() ? samples[slot].input : InvalidAnimationGraphNode;
}

void AnimationBlend1DNode::SetInput(uint32 slot, uint32 nodeIndex)
{
    if (slot < samples.Size())
    {
        samples[slot].input = nodeIndex;
    }
}

bool AnimationBlend1DNode::AddInput(uint32 nodeIndex)
{
    float position = 0.0f;

    for (const AnimationBlendSample& sample : samples)
    {
        position = MathUtil::Max(position, sample.position.x + 1.0f);
    }

    samples.PushBack(AnimationBlendSample { nodeIndex, Vec2f(position, 0.0f) });

    return true;
}

void AnimationBlend1DNode::RemoveInput(uint32 slot)
{
    if (slot < samples.Size())
    {
        samples.EraseAt(slot);
    }
}

void AnimationBlend1DNode::OnNodeRemoved(uint32 removedIndex)
{
    RemoveSamplesOfNode(samples, removedIndex);
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

uint32 AnimationBlend2DNode::NumInputs() const
{
    return uint32(samples.Size());
}

Name AnimationBlend2DNode::GetInputName(uint32 slot) const
{
    return slot < samples.Size() ? NAME_FMT("{}, {}", samples[slot].position.x, samples[slot].position.y) : Name::Invalid();
}

uint32 AnimationBlend2DNode::GetInput(uint32 slot) const
{
    return slot < samples.Size() ? samples[slot].input : InvalidAnimationGraphNode;
}

void AnimationBlend2DNode::SetInput(uint32 slot, uint32 nodeIndex)
{
    if (slot < samples.Size())
    {
        samples[slot].input = nodeIndex;
    }
}

bool AnimationBlend2DNode::AddInput(uint32 nodeIndex)
{
    // spread new samples along x so they don't start on top of each other
    samples.PushBack(AnimationBlendSample { nodeIndex, Vec2f(float(samples.Size()), 0.0f) });

    return true;
}

void AnimationBlend2DNode::RemoveInput(uint32 slot)
{
    if (slot < samples.Size())
    {
        samples.EraseAt(slot);
    }
}

void AnimationBlend2DNode::OnNodeRemoved(uint32 removedIndex)
{
    RemoveSamplesOfNode(samples, removedIndex);
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

uint32 AnimationLayerNode::NumInputs() const
{
    return 2;
}

Name AnimationLayerNode::GetInputName(uint32 slot) const
{
    return slot == 0 ? NAME("Base") : NAME("Layer");
}

uint32 AnimationLayerNode::GetInput(uint32 slot) const
{
    return slot == 0 ? base : layer;
}

void AnimationLayerNode::SetInput(uint32 slot, uint32 nodeIndex)
{
    if (slot == 0)
    {
        base = nodeIndex;
    }
    else
    {
        layer = nodeIndex;
    }
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

uint32 AnimationAdditiveNode::NumInputs() const
{
    return 2;
}

Name AnimationAdditiveNode::GetInputName(uint32 slot) const
{
    return slot == 0 ? NAME("Base") : NAME("Additive");
}

uint32 AnimationAdditiveNode::GetInput(uint32 slot) const
{
    return slot == 0 ? base : additive;
}

void AnimationAdditiveNode::SetInput(uint32 slot, uint32 nodeIndex)
{
    if (slot == 0)
    {
        base = nodeIndex;
    }
    else
    {
        additive = nodeIndex;
    }
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

uint32 AnimationChainTwistNode::NumInputs() const
{
    return 1;
}

Name AnimationChainTwistNode::GetInputName(uint32 slot) const
{
    return NAME("Input");
}

uint32 AnimationChainTwistNode::GetInput(uint32 slot) const
{
    return input;
}

void AnimationChainTwistNode::SetInput(uint32 slot, uint32 nodeIndex)
{
    input = nodeIndex;
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

#pragma region AnimationStateMachineNode

namespace {

float ApplyBlendCurve(AnimationBlendCurve curve, float alpha)
{
    alpha = MathUtil::Clamp(alpha, 0.0f, 1.0f);

    switch (curve)
    {
    case AnimationBlendCurve::SmoothStep:
        return alpha * alpha * (3.0f - 2.0f * alpha);
    case AnimationBlendCurve::EaseIn:
        return alpha * alpha;
    case AnimationBlendCurve::EaseOut:
        return 1.0f - (1.0f - alpha) * (1.0f - alpha);
    default:
        return alpha;
    }
}

} // namespace

uint32 AnimationStateMachineNode::NumInputs() const
{
    return uint32(states.Size());
}

Name AnimationStateMachineNode::GetInputName(uint32 slot) const
{
    return GetStateName(slot);
}

uint32 AnimationStateMachineNode::GetInput(uint32 slot) const
{
    return slot < states.Size() ? states[slot].input : InvalidAnimationGraphNode;
}

void AnimationStateMachineNode::SetInput(uint32 slot, uint32 nodeIndex)
{
    if (slot < states.Size())
    {
        states[slot].input = nodeIndex;
    }
}

bool AnimationStateMachineNode::AddInput(uint32 nodeIndex)
{
    states.PushBack(AnimationState { NAME_FMT("State{}", states.Size()), nodeIndex });

    return true;
}

void AnimationStateMachineNode::RemoveInput(uint32 slot)
{
    if (slot >= states.Size())
    {
        return;
    }

    states.EraseAt(slot);

    for (auto it = transitions.Begin(); it != transitions.End();)
    {
        if (it->to == slot || (!it->fromAnyState && it->from == slot))
        {
            it = transitions.Erase(it);

            continue;
        }

        if (it->to > slot)
        {
            --it->to;
        }

        if (it->from > slot)
        {
            --it->from;
        }

        ++it;
    }

    if (entryState == slot)
    {
        entryState = 0;
    }
    else if (entryState > slot)
    {
        --entryState;
    }
}

Name AnimationStateMachineNode::GetStateName(uint32 stateIndex) const
{
    if (stateIndex >= states.Size())
    {
        return Name::Invalid();
    }

    if (states[stateIndex].name.IsValid())
    {
        return states[stateIndex].name;
    }

    return NAME_FMT("State{}", stateIndex);
}

String AnimationStateMachineNode::DescribeState(const AnimationGraphNodeState& state) const
{
    if (state.currentState >= states.Size())
    {
        return String::empty;
    }

    if (state.previousState == InvalidAnimationGraphNode || state.previousState >= states.Size())
    {
        return HYP_FORMAT("{}", GetStateName(state.currentState));
    }

    return HYP_FORMAT("{} -> {} {}%", GetStateName(state.previousState), GetStateName(state.currentState), int(state.blendAlpha * 100.0f + 0.5f));
}

bool AnimationStateMachineNode::MeetsCondition(AnimationGraphContext& context, const AnimationTransitionCondition& condition, const AnimationGraphNodeState& state) const
{
    // the heaviest clip the state played last update times it
    const AnimationClipNode* stateClipNode = TryCast<AnimationClipNode>(context.GetGraph().GetNode(state.stateClip));
    const float clipLength = stateClipNode && stateClipNode->clip.IsValid() ? stateClipNode->clip->GetLength() : 0.0f;
    const float clipTime = stateClipNode ? context.GetState(state.stateClip).time : 0.0f;

    switch (condition.op)
    {
    case AnimationConditionOp::Greater:
        return context.GetFloat(condition.parameter) > condition.value;
    case AnimationConditionOp::Less:
        return context.GetFloat(condition.parameter) < condition.value;
    case AnimationConditionOp::Equal:
        return context.GetFloat(condition.parameter) == condition.value;
    case AnimationConditionOp::NotEqual:
        return context.GetFloat(condition.parameter) != condition.value;
    case AnimationConditionOp::IsTrue:
        return context.GetBool(condition.parameter);
    case AnimationConditionOp::IsFalse:
        return !context.GetBool(condition.parameter);
    case AnimationConditionOp::Triggered:
        return context.IsTriggered(condition.parameter);
    case AnimationConditionOp::StateTimeAtLeast:
        return state.stateTime >= condition.value;
    case AnimationConditionOp::StateTimeRemainingBelow:
        if (clipLength <= 0.0f || stateClipNode->playRate <= 0.0f)
        {
            return false;
        }

        return (clipLength - clipTime) / stateClipNode->playRate < condition.value;
    case AnimationConditionOp::StateNormalizedTimeAtLeast:
        return clipLength > 0.0f && clipTime / clipLength >= condition.value;
    default:
        return false;
    }
}

uint32 AnimationStateMachineNode::ChooseTransition(AnimationGraphContext& context, const AnimationGraphNodeState& state) const
{
    const bool isTransitioning = state.previousState != InvalidAnimationGraphNode;

    uint32 chosen = InvalidAnimationGraphNode;

    for (uint32 transitionIndex = 0; transitionIndex < uint32(transitions.Size()); ++transitionIndex)
    {
        const AnimationStateTransition& transition = transitions[transitionIndex];

        if (transition.to >= states.Size() || transition.to == state.currentState)
        {
            continue;
        }

        // only any-state transitions interrupt a running one
        if (!transition.fromAnyState && (isTransitioning || transition.from != state.currentState))
        {
            continue;
        }

        if (chosen != InvalidAnimationGraphNode && transition.priority <= transitions[chosen].priority)
        {
            continue;
        }

        bool meetsConditions = true;

        for (const AnimationTransitionCondition& condition : transition.conditions)
        {
            if (!MeetsCondition(context, condition, state))
            {
                meetsConditions = false;

                break;
            }
        }

        if (meetsConditions)
        {
            chosen = transitionIndex;
        }
    }

    return chosen;
}

void AnimationStateMachineNode::StartTransition(AnimationGraphNodeState& state, uint32 transitionIndex) const
{
    const AnimationStateTransition& transition = transitions[transitionIndex];

    // an interrupted transition fades out from whichever side it was closer to
    uint32 outgoingState = state.currentState;

    if (state.previousState != InvalidAnimationGraphNode && state.blendAlpha < 0.5f)
    {
        outgoingState = state.previousState;
    }

    state.currentState = transition.to;
    state.stateClip = InvalidAnimationGraphNode;
    state.stateTime = 0.0f;
    state.transitionIndex = transitionIndex;
    state.transitionTime = 0.0f;
    state.transitionDuration = transition.duration;
    state.previousState = InvalidAnimationGraphNode;

    if (transition.duration > 0.0f && outgoingState != transition.to)
    {
        state.previousState = outgoingState;
    }
}

void AnimationStateMachineNode::Update(AnimationGraphContext& context, uint32 nodeIndex, float weight) const
{
    AnimationGraphNodeState& state = context.GetState(nodeIndex);
    state.weight += weight;

    if (states.Empty())
    {
        return;
    }

    const bool isFirstUpdate = context.BeginNodeUpdate(nodeIndex);

    if (isFirstUpdate)
    {
        if (!state.wasActive || state.currentState >= states.Size())
        {
            state.currentState = entryState < states.Size() ? entryState : 0;
            state.previousState = InvalidAnimationGraphNode;
            state.stateClip = InvalidAnimationGraphNode;
            state.stateTime = 0.0f;
            state.transitionIndex = InvalidAnimationGraphNode;
        }
        else
        {
            state.stateTime += context.GetDelta();
            state.transitionTime += context.GetDelta();

            if (state.previousState != InvalidAnimationGraphNode && (state.transitionTime >= state.transitionDuration || state.previousState >= states.Size()))
            {
                state.previousState = InvalidAnimationGraphNode;
            }
        }

        if (const uint32 transitionIndex = ChooseTransition(context, state); transitionIndex != InvalidAnimationGraphNode)
        {
            StartTransition(state, transitionIndex);
        }

        state.blendAlpha = 1.0f;

        if (state.previousState != InvalidAnimationGraphNode)
        {
            state.blendAlpha = ApplyBlendCurve(transitions[state.transitionIndex].curve, state.transitionTime / state.transitionDuration);
        }
    }

    if (state.previousState != InvalidAnimationGraphNode && state.blendAlpha < 1.0f)
    {
        context.UpdateInput(states[state.previousState].input, weight * (1.0f - state.blendAlpha));
    }

    const uint32 firstClip = context.NumActiveClips();

    context.UpdateInput(states[state.currentState].input, weight * state.blendAlpha);

    if (!isFirstUpdate)
    {
        return;
    }

    state.stateClip = InvalidAnimationGraphNode;

    for (uint32 clipIndex = firstClip; clipIndex < context.NumActiveClips(); ++clipIndex)
    {
        const uint32 clipNode = context.GetActiveClip(clipIndex);

        if (state.stateClip == InvalidAnimationGraphNode || context.GetState(clipNode).weight > context.GetState(state.stateClip).weight)
        {
            state.stateClip = clipNode;
        }
    }
}

void AnimationStateMachineNode::Evaluate(AnimationGraphContext& context, uint32 nodeIndex, Span<Transform> outPose) const
{
    const AnimationGraphNodeState& state = context.GetState(nodeIndex);

    if (state.currentState >= states.Size())
    {
        context.EvaluateInput(InvalidAnimationGraphNode, outPose);

        return;
    }

    const uint32 currentInput = states[state.currentState].input;

    if (state.previousState == InvalidAnimationGraphNode || state.blendAlpha >= 1.0f)
    {
        context.EvaluateInput(currentInput, outPose);

        return;
    }

    context.EvaluateInput(states[state.previousState].input, outPose);

    Span<Transform> currentPose = context.AcquireScratchPose();
    context.EvaluateInput(currentInput, currentPose);

    PoseOps::BlendPoses(outPose, currentPose, state.blendAlpha, {}, outPose);

    context.ReleaseScratchPose();
}

#pragma endregion AnimationStateMachineNode

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

    MarkChanged();

    return uint32(m_nodes.Size() - 1);
}

void AnimationGraph::RemoveNode(uint32 nodeIndex)
{
    if (nodeIndex >= m_nodes.Size())
    {
        return;
    }

    m_nodes.EraseAt(nodeIndex);

    for (const Handle<AnimationGraphNode>& node : m_nodes)
    {
        if (node.IsValid())
        {
            node->OnNodeRemoved(nodeIndex);
        }
    }

    m_rootNode = RenumberAfterRemoval(m_rootNode, nodeIndex);

    MarkChanged();
}

Handle<AnimationGraphNode> AnimationGraph::CreateNode(Name className)
{
    const Class* nodeClass = ClassRegistry::GetInstance().GetClass(className);

    if (!nodeClass || !nodeClass->IsDerivedFrom(AnimationGraphNode::StaticClass()) || nodeClass->IsAbstract() || !nodeClass->CanCreateInstance())
    {
        return Handle<AnimationGraphNode>::empty;
    }

    BoxedValue boxed;

    if (!nodeClass->CreateInstance(boxed) || !boxed.Is<Handle<ObjectBase>>())
    {
        return Handle<AnimationGraphNode>::empty;
    }

    Handle<AnimationGraphNode> node = TryCast<AnimationGraphNode>(boxed.Get<Handle<ObjectBase>>());
    InitObject(node);

    return node;
}

Array<Name> AnimationGraph::GetNodeClassNames()
{
    return {
        AnimationClipNode::StaticClass()->GetName(),
        AnimationBlend1DNode::StaticClass()->GetName(),
        AnimationBlend2DNode::StaticClass()->GetName(),
        AnimationLayerNode::StaticClass()->GetName(),
        AnimationAdditiveNode::StaticClass()->GetName(),
        AnimationChainTwistNode::StaticClass()->GetName(),
        AnimationStateMachineNode::StaticClass()->GetName()
    };
}

void AnimationGraph::AddParameter(Name name, AnimationGraphParameterType type, const Vec2f& defaultValue)
{
    m_parameters.PushBack(AnimationGraphParameter { name, type, defaultValue });

    MarkChanged();
}

void AnimationGraph::RemoveParameter(Name name)
{
    for (auto it = m_parameters.Begin(); it != m_parameters.End(); ++it)
    {
        if (it->name == name)
        {
            m_parameters.Erase(it);

            MarkChanged();

            return;
        }
    }
}

Result AnimationGraph::SaveContents(String& outText) const
{
    return ObjectToHMF(StaticClass(), BoxedValue(MakeStrongRef(const_cast<AnimationGraph*>(this))), outText);
}

Result AnimationGraph::LoadContents(const String& text)
{
    HMF::ParseResult parsed = HMF::Parse(text);

    if (parsed.HasError() || !parsed.GetValue().Is<Handle<ObjectBase>>())
    {
        return HYP_MAKE_ERROR(Error, "Animation graph '{}': couldn't read its contents back", GetName());
    }

    Handle<AnimationGraph> loaded = TryCast<AnimationGraph>(parsed.GetValue().Get<Handle<ObjectBase>>());

    if (!loaded.IsValid())
    {
        return HYP_MAKE_ERROR(Error, "Animation graph '{}': the text isn't an animation graph", GetName());
    }

    m_nodes = loaded->m_nodes;
    m_rootNode = loaded->m_rootNode;
    m_parameters = loaded->m_parameters;

    MarkChanged();

    return {};
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
