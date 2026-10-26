/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/Animation/AnimationGraphInstance.hpp>
#include <Scene/Animation/Animation.hpp>
#include <Scene/Animation/BoneMask.hpp>
#include <Scene/Animation/Skeleton.hpp>

#include <Core/Math/MathUtil.hpp>

#include <cmath>

#include <AnimationGraphInstance.generated.inl>

namespace Hyperion {

static constexpr float NotifyWeightThreshold = 0.5f;

namespace {

float WrapClipTime(float time, float length, bool loop)
{
    if (length <= 0.0f)
    {
        return 0.0f;
    }

    if (!loop)
    {
        return MathUtil::Clamp(time, 0.0f, length);
    }

    time = std::fmod(time, length);

    return time < 0.0f ? time + length : time;
}

bool CrossedTime(float previousTime, float time, float markerTime)
{
    if (time == previousTime)
    {
        return false;
    }

    // a loop wrapped between the two times
    if (time < previousTime)
    {
        return markerTime > previousTime || markerTime <= time;
    }

    return markerTime > previousTime && markerTime <= time;
}

} // namespace

#pragma region AnimationGraphContext

AnimationGraphContext::AnimationGraphContext(AnimationGraphInstance& instance, const SkeletonBindData& bindData, float delta)
    : m_instance(&instance),
      m_bindData(&bindData),
      m_delta(delta)
{
}

float AnimationGraphContext::GetFloat(Name parameter, float fallback) const
{
    auto it = m_instance->m_parameterIndices.Find(parameter);

    return it != m_instance->m_parameterIndices.End() ? m_instance->m_values[it->second].x : fallback;
}

bool AnimationGraphContext::GetBool(Name parameter) const
{
    auto it = m_instance->m_parameterIndices.Find(parameter);

    if (it == m_instance->m_parameterIndices.End())
    {
        return false;
    }

    return m_instance->m_triggers[it->second] || m_instance->m_values[it->second].x != 0.0f;
}

Vec2f AnimationGraphContext::GetVector2(Name parameter) const
{
    auto it = m_instance->m_parameterIndices.Find(parameter);

    return it != m_instance->m_parameterIndices.End() ? m_instance->m_values[it->second] : Vec2f::Zero();
}

AnimationGraphNodeState& AnimationGraphContext::GetState(uint32 nodeIndex)
{
    return m_instance->m_nodeStates[nodeIndex];
}

bool AnimationGraphContext::BeginNodeUpdate(uint32 nodeIndex)
{
    AnimationGraphNodeState& state = GetState(nodeIndex);

    if (state.lastUpdateSerial == m_instance->m_updateSerial)
    {
        return false;
    }

    state.lastUpdateSerial = m_instance->m_updateSerial;

    return true;
}

void AnimationGraphContext::UpdateInput(uint32 nodeIndex, float weight)
{
    if (weight <= 0.0f || nodeIndex >= m_instance->m_nodeValid.Size() || !m_instance->m_nodeValid[nodeIndex])
    {
        return;
    }

    m_instance->m_graph->GetNode(nodeIndex)->Update(*this, nodeIndex, weight);
}

void AnimationGraphContext::EvaluateInput(uint32 nodeIndex, Span<Transform> outPose)
{
    if (nodeIndex >= m_instance->m_nodeValid.Size() || !m_instance->m_nodeValid[nodeIndex] || GetState(nodeIndex).weight <= 0.0f)
    {
        for (uint32 slot = 0; slot < m_bindData->NumSlots(); ++slot)
        {
            outPose[slot] = m_bindData->restLocals[slot];
        }

        return;
    }

    m_instance->m_graph->GetNode(nodeIndex)->Evaluate(*this, nodeIndex, outPose);
}

void AnimationGraphContext::RegisterActiveClip(uint32 nodeIndex)
{
    AnimationGraphNodeState& state = GetState(nodeIndex);

    if (!state.isRegistered)
    {
        state.isRegistered = true;

        m_instance->m_activeClips.PushBack(nodeIndex);
    }
}

const ClipBinding& AnimationGraphContext::GetClipBinding(const Animation& animation)
{
    auto it = m_instance->m_clipBindings.Find(&animation);

    if (it != m_instance->m_clipBindings.End() && it->second.animationRevision == animation.GetRevision())
    {
        return it->second;
    }

    if (it != m_instance->m_clipBindings.End())
    {
        it->second = PoseOps::BindClip(animation, *m_bindData);

        return it->second;
    }

    return m_instance->m_clipBindings.Insert({ &animation, PoseOps::BindClip(animation, *m_bindData) }).first->second;
}

Span<const float> AnimationGraphContext::GetMaskWeights(const BoneMask& mask)
{
    auto it = m_instance->m_maskWeights.Find(&mask);

    if (it == m_instance->m_maskWeights.End())
    {
        it = m_instance->m_maskWeights.Insert({ &mask, mask.BuildSlotWeights(*m_bindData) }).first;
    }

    return it->second.ToSpan();
}

Span<Transform> AnimationGraphContext::AcquireScratchPose()
{
    AssertDebug(m_instance->m_scratchDepth < m_instance->m_scratchPoses.Size());

    return m_instance->m_scratchPoses[m_instance->m_scratchDepth++].ToSpan();
}

void AnimationGraphContext::ReleaseScratchPose()
{
    AssertDebug(m_instance->m_scratchDepth > 0);

    --m_instance->m_scratchDepth;
}

#pragma endregion AnimationGraphContext

#pragma region AnimationGraphInstance

AnimationGraphInstance::AnimationGraphInstance() = default;

AnimationGraphInstance::AnimationGraphInstance(const Handle<AnimationGraph>& graph)
    : m_graph(graph)
{
    if (!m_graph.IsValid())
    {
        return;
    }

    for (const AnimationGraphParameter& parameter : m_graph->GetParameters())
    {
        if (m_parameterIndices.Contains(parameter.name))
        {
            continue;
        }

        m_parameterIndices[parameter.name] = uint32(m_values.Size());

        m_values.PushBack(parameter.defaultValue);
        m_triggers.PushBack(false);
    }

    m_pendingValues = m_values;
    m_pendingTriggers = m_triggers;

    m_nodeStates.Resize(m_graph->NumNodes());

    ValidateGraph();
}

AnimationGraphInstance::~AnimationGraphInstance() = default;

void AnimationGraphInstance::ValidateGraph()
{
    const uint32 numNodes = m_graph->NumNodes();

    enum class VisitState : uint8
    {
        Unvisited,
        Visiting,
        Done
    };

    Array<VisitState> visitStates;
    visitStates.Resize(numNodes);

    for (VisitState& visitState : visitStates)
    {
        visitState = VisitState::Unvisited;
    }

    m_nodeValid.Resize(numNodes);

    for (uint32 nodeIndex = 0; nodeIndex < numNodes; ++nodeIndex)
    {
        m_nodeValid[nodeIndex] = m_graph->GetNode(nodeIndex) != nullptr;
    }

    // a node that feeds into itself can't be evaluated, so it's dropped (its output becomes the rest pose)
    const auto visit = [&](auto&& self, uint32 nodeIndex) -> void
    {
        if (nodeIndex >= numNodes || visitStates[nodeIndex] == VisitState::Done || !m_nodeValid[nodeIndex])
        {
            return;
        }

        visitStates[nodeIndex] = VisitState::Visiting;

        Array<uint32> inputs;
        m_graph->GetNode(nodeIndex)->GetInputs(inputs);

        for (uint32 input : inputs)
        {
            if (input < numNodes && visitStates[input] == VisitState::Visiting)
            {
                m_nodeValid[nodeIndex] = false;

                HYP_LOG(Animation, Warning, "Animation graph '{}': node {} feeds into itself and is ignored", m_graph->GetName(), nodeIndex);

                continue;
            }

            self(self, input);
        }

        visitStates[nodeIndex] = VisitState::Done;
    };

    for (uint32 nodeIndex = 0; nodeIndex < numNodes; ++nodeIndex)
    {
        visit(visit, nodeIndex);
    }
}

void AnimationGraphInstance::ResetBindings(const Skeleton& skeleton)
{
    m_boundSkeleton = &skeleton;
    m_boundSkeletonRevision = skeleton.GetRevision();

    m_clipBindings.Clear();
    m_maskWeights.Clear();

    // a pose pass never nests deeper than the graph has nodes
    m_scratchPoses.Clear();
    m_scratchPoses.Resize(m_nodeStates.Size() + 1);

    for (Array<Transform>& scratchPose : m_scratchPoses)
    {
        scratchPose.Resize(skeleton.GetBindData().NumSlots());
    }
}

void AnimationGraphInstance::SetValue(Name parameter, const Vec2f& value, bool isTrigger)
{
    Mutex::Guard guard(m_pendingMutex);

    auto it = m_parameterIndices.Find(parameter);

    if (it == m_parameterIndices.End())
    {
        return;
    }

    if (isTrigger)
    {
        m_pendingTriggers[it->second] = true;
    }
    else
    {
        m_pendingValues[it->second] = value;
    }
}

void AnimationGraphInstance::SetFloat(Name parameter, float value)
{
    SetValue(parameter, Vec2f(value, 0.0f), false);
}

void AnimationGraphInstance::SetBool(Name parameter, bool value)
{
    SetValue(parameter, Vec2f(value ? 1.0f : 0.0f, 0.0f), false);
}

void AnimationGraphInstance::SetTrigger(Name parameter)
{
    SetValue(parameter, Vec2f::Zero(), true);
}

void AnimationGraphInstance::SetVector2(Name parameter, Vec2f value)
{
    SetValue(parameter, value, false);
}

float AnimationGraphInstance::GetFloat(Name parameter) const
{
    return GetVector2(parameter).x;
}

bool AnimationGraphInstance::GetBool(Name parameter) const
{
    return GetVector2(parameter).x != 0.0f;
}

Vec2f AnimationGraphInstance::GetVector2(Name parameter) const
{
    Mutex::Guard guard(m_pendingMutex);

    auto it = m_parameterIndices.Find(parameter);

    return it != m_parameterIndices.End() ? m_pendingValues[it->second] : Vec2f::Zero();
}

Array<Name> AnimationGraphInstance::GetFiredNotifies() const
{
    Mutex::Guard guard(m_pendingMutex);

    return m_publishedNotifies;
}

float AnimationGraphInstance::GetNodeWeight(uint32 nodeIndex) const
{
    return nodeIndex < m_nodeStates.Size() ? m_nodeStates[nodeIndex].weight : 0.0f;
}

float AnimationGraphInstance::GetNodeTime(uint32 nodeIndex) const
{
    return nodeIndex < m_nodeStates.Size() ? m_nodeStates[nodeIndex].time : 0.0f;
}

void AnimationGraphInstance::Update(float delta, const Skeleton& skeleton)
{
    HYP_SCOPE;

    if (!m_graph.IsValid())
    {
        return;
    }

    if (m_boundSkeleton != &skeleton || m_boundSkeletonRevision != skeleton.GetRevision())
    {
        ResetBindings(skeleton);
    }

    {
        Mutex::Guard guard(m_pendingMutex);

        m_values = m_pendingValues;
        m_triggers = m_pendingTriggers;

        for (bool& pendingTrigger : m_pendingTriggers)
        {
            pendingTrigger = false;
        }
    }

    ++m_updateSerial;

    for (AnimationGraphNodeState& state : m_nodeStates)
    {
        state.wasActive = state.weight > 0.0f;
        state.weight = 0.0f;
        state.isRegistered = false;
    }

    m_activeClips.Clear();
    m_firedNotifies.Clear();

    AnimationGraphContext context(*this, skeleton.GetBindData(), delta);
    context.UpdateInput(m_graph->GetRootNode(), 1.0f);

    AdvanceClips(delta);

    Mutex::Guard guard(m_pendingMutex);
    m_publishedNotifies = m_firedNotifies;
}

void AnimationGraphInstance::AdvanceClip(uint32 nodeIndex, const AnimationClipNode& clipNode, float time, bool fireNotifies)
{
    AnimationGraphNodeState& state = m_nodeStates[nodeIndex];

    // a clip that just became active starts fresh instead of firing everything since it last played
    state.previousTime = state.wasActive ? state.time : time;
    state.time = time;

    if (!fireNotifies || !clipNode.clip.IsValid())
    {
        return;
    }

    for (const AnimationNotify& notify : clipNode.clip->GetNotifies())
    {
        if (CrossedTime(state.previousTime, state.time, notify.time) && !m_firedNotifies.Contains(notify.name))
        {
            m_firedNotifies.PushBack(notify.name);
        }
    }
}

void AnimationGraphInstance::AdvanceClips(float delta)
{
    Map<Name, Array<uint32>> syncGroups;

    for (uint32 nodeIndex : m_activeClips)
    {
        const AnimationClipNode& clipNode = static_cast<const AnimationClipNode&>(*m_graph->GetNode(nodeIndex));
        const AnimationGraphNodeState& state = m_nodeStates[nodeIndex];
        const float length = clipNode.clip.IsValid() ? clipNode.clip->GetLength() : 0.0f;

        if (clipNode.syncGroup.IsValid() && !clipNode.timeParameter.IsValid())
        {
            syncGroups[clipNode.syncGroup].PushBack(nodeIndex);

            continue;
        }

        float time = state.wasActive ? state.time + delta * clipNode.playRate : clipNode.startOffset;

        if (clipNode.timeParameter.IsValid())
        {
            auto it = m_parameterIndices.Find(clipNode.timeParameter);
            time = it != m_parameterIndices.End() ? m_values[it->second].x : 0.0f;
        }

        AdvanceClip(nodeIndex, clipNode, WrapClipTime(time, length, clipNode.loop), state.weight >= NotifyWeightThreshold);
    }

    // one clip leads each group; the others follow its normalized phase so their cycles stay in step
    for (const auto& groupIt : syncGroups)
    {
        const Array<uint32>& members = groupIt.second;

        uint32 leader = members[0];

        for (uint32 nodeIndex : members)
        {
            const AnimationClipNode& clipNode = static_cast<const AnimationClipNode&>(*m_graph->GetNode(nodeIndex));
            const AnimationClipNode& leaderNode = static_cast<const AnimationClipNode&>(*m_graph->GetNode(leader));

            if (clipNode.syncLeader != leaderNode.syncLeader
                    ? clipNode.syncLeader
                    : m_nodeStates[nodeIndex].weight > m_nodeStates[leader].weight)
            {
                leader = nodeIndex;
            }
        }

        const AnimationClipNode& leaderNode = static_cast<const AnimationClipNode&>(*m_graph->GetNode(leader));
        const AnimationGraphNodeState& leaderState = m_nodeStates[leader];
        const float leaderLength = leaderNode.clip.IsValid() ? leaderNode.clip->GetLength() : 0.0f;

        const float leaderTime = WrapClipTime(
            leaderState.wasActive ? leaderState.time + delta * leaderNode.playRate : leaderNode.startOffset,
            leaderLength,
            leaderNode.loop);

        const float phase = leaderLength > 0.0f ? leaderTime / leaderLength : 0.0f;

        for (uint32 nodeIndex : members)
        {
            const AnimationClipNode& clipNode = static_cast<const AnimationClipNode&>(*m_graph->GetNode(nodeIndex));
            const float length = clipNode.clip.IsValid() ? clipNode.clip->GetLength() : 0.0f;

            AdvanceClip(nodeIndex, clipNode, nodeIndex == leader ? leaderTime : phase * length, nodeIndex == leader);
        }
    }
}

void AnimationGraphInstance::Evaluate(const Skeleton& skeleton, Span<Transform> outPose)
{
    HYP_SCOPE;

    const SkeletonBindData& bindData = skeleton.GetBindData();

    if (!m_graph.IsValid())
    {
        for (uint32 slot = 0; slot < bindData.NumSlots(); ++slot)
        {
            outPose[slot] = bindData.restLocals[slot];
        }

        return;
    }

    if (m_boundSkeleton != &skeleton || m_boundSkeletonRevision != skeleton.GetRevision())
    {
        ResetBindings(skeleton);
    }

    m_scratchDepth = 0;

    AnimationGraphContext context(*this, bindData, 0.0f);
    context.EvaluateInput(m_graph->GetRootNode(), outPose);
}

#pragma endregion AnimationGraphInstance

} // namespace Hyperion
