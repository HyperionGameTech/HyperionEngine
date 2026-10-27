/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <Animation/AnimationGraphInstance.hpp>
#include <Scene/Animation/Animation.hpp>
#include <Animation/BoneMask.hpp>
#include <Scene/Animation/Skeleton.hpp>
#include <Animation/MotionMatching.hpp>

#include <Core/Math/MathUtil.hpp>

#include <cmath>

#include <AnimationGraphInstance.generated.inl>

namespace Hyperion {
    
namespace {
constexpr float NotifyWeightThreshold = 0.5f;

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

bool AnimationGraphContext::IsTriggered(Name parameter) const
{
    auto it = m_instance->m_parameterIndices.Find(parameter);

    return it != m_instance->m_parameterIndices.End() && m_instance->m_triggers[it->second];
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

const AnimationSlotPlayback* AnimationGraphContext::GetSlotPlayback(Name slot) const
{
    auto it = m_instance->m_slots.Find(slot);

    return it != m_instance->m_slots.End() ? &it->second : nullptr;
}

AnimationInertialization& AnimationGraphContext::GetInertialization(uint32 nodeIndex)
{
    return m_instance->m_inertialization[nodeIndex];
}

void AnimationGraphContext::ApplyInertialization(uint32 nodeIndex, Span<Transform> outPose)
{
    AnimationGraphNodeState& state = GetState(nodeIndex);
    AnimationInertialization& inertialization = GetInertialization(nodeIndex);
    const size_t numSlots = outPose.Size();

    if (state.isInertializePending)
    {
        state.isInertializePending = false;

        const bool hasLastPose = inertialization.lastPose.Size() == numSlots;

        inertialization.rotationOffsets.Resize(numSlots);
        inertialization.translationOffsets.Resize(numSlots);

        for (size_t slot = 0; slot < numSlots; ++slot)
        {
            inertialization.rotationOffsets[slot] = Quat4f::Identity();
            inertialization.translationOffsets[slot] = Vec3f::Zero();

            if (hasLastPose)
            {
                inertialization.rotationOffsets[slot] = inertialization.lastPose[slot].GetRotation() * outPose[slot].GetRotation().Inverse();
                inertialization.translationOffsets[slot] = inertialization.lastPose[slot].GetTranslation() - outPose[slot].GetTranslation();
            }
        }
    }

    if (state.isInertializing && inertialization.rotationOffsets.Size() == numSlots && state.transitionDuration > 0.0f)
    {
        // a quintic falloff, which leaves the offset with no speed at either end
        const float progress = MathUtil::Clamp(state.transitionTime / state.transitionDuration, 0.0f, 1.0f);
        const float remaining = 1.0f - progress * progress * progress * (progress * (progress * 6.0f - 15.0f) + 10.0f);

        for (size_t slot = 0; slot < numSlots; ++slot)
        {
            Quat4f offset = Quat4f::Identity();
            offset.Slerp(inertialization.rotationOffsets[slot], remaining);

            outPose[slot].SetRotation(offset * outPose[slot].GetRotation());
            outPose[slot].SetTranslation(outPose[slot].GetTranslation() + inertialization.translationOffsets[slot] * remaining);
        }
    }

    inertialization.lastPose = Array<Transform>(Span<const Transform>(outPose.Data(), outPose.Size()));
}

void AnimationGraphContext::AddRootMotion(const Vec3f& motion, float weight)
{
    const float scaledWeight = weight * m_instance->m_rootMotionScale;

    m_instance->m_rootMotion += motion * scaledWeight;
    m_instance->m_rootMotionWeight += scaledWeight;
}

float AnimationGraphContext::GetRootMotionScale() const
{
    return m_instance->m_rootMotionScale;
}

void AnimationGraphContext::SetRootMotionScale(float scale)
{
    m_instance->m_rootMotionScale = scale;
}

const Skeleton& AnimationGraphContext::GetSkeleton() const
{
    return *m_instance->m_boundSkeleton.GetUnsafe();
}

const Vec3f& AnimationGraphContext::GetModelUp() const
{
    return m_instance->m_modelUp;
}

const AnimationGraph& AnimationGraphContext::GetGraph() const
{
    return *m_instance->m_graph;
}

uint32 AnimationGraphContext::NumActiveClips() const
{
    return uint32(m_instance->m_activeClips.Size());
}

uint32 AnimationGraphContext::GetActiveClip(uint32 index) const
{
    return m_instance->m_activeClips[index];
}

const ClipBinding& AnimationGraphContext::GetClipBinding(const Animation& animation)
{
    const WeakHandle<Animation> key = animation.WeakHandleFromThis();

    auto it = m_instance->m_clipBindings.Find(key);

    if (it != m_instance->m_clipBindings.End())
    {
        if (!PoseOps::IsBindingCurrent(it->second, animation))
        {
            it->second = PoseOps::BindClip(animation, GetSkeleton());
        }

        return it->second;
    }

    for (auto expiredIt = m_instance->m_clipBindings.Begin(); expiredIt != m_instance->m_clipBindings.End();)
    {
        if (expiredIt->first.Expired())
        {
            expiredIt = m_instance->m_clipBindings.Erase(expiredIt);

            continue;
        }

        ++expiredIt;
    }

    return m_instance->m_clipBindings.Insert({ key, PoseOps::BindClip(animation, GetSkeleton()) }).first->second;
}

Span<const float> AnimationGraphContext::GetMaskWeights(const BoneMask& mask)
{
    auto it = m_instance->m_maskWeights.Find(mask.WeakHandleFromThis());

    if (it == m_instance->m_maskWeights.End())
    {
        it = m_instance->m_maskWeights.Insert({
            mask.WeakHandleFromThis(),
            {
                mask.GetRevision(),
                mask.BuildSlotWeights(*m_bindData)
            }
        }).first;
    }
    else if (it->second.first != mask.GetRevision())
    {
        it->second = {
            mask.GetRevision(),
            mask.BuildSlotWeights(*m_bindData)
        };
    }

    return it->second.second.ToSpan();
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

    m_graphRevision = m_graph->GetRevision();

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
    m_boundSkeleton = skeleton.WeakHandleFromThis();
    m_boundSkeletonRevision = skeleton.GetRevision();

    m_clipBindings.Clear();
    m_maskWeights.Clear();

    // a pose pass never nests deeper than the graph has nodes
    m_scratchPoses.Clear();
    m_scratchPoses.Resize(m_nodeStates.Size() + 1);

    m_modelUp = PoseOps::GetModelUp(skeleton);

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

void AnimationGraphInstance::CopyParameterValues(const AnimationGraphInstance& other)
{
    if (&other == this)
    {
        return;
    }

    Mutex::Guard otherGuard(other.m_pendingMutex);
    Mutex::Guard guard(m_pendingMutex);

    for (const auto& it : m_parameterIndices)
    {
        auto otherIt = other.m_parameterIndices.Find(it.first);

        if (otherIt != other.m_parameterIndices.End())
        {
            m_pendingValues[it.second] = other.m_pendingValues[otherIt->second];
        }
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

const AnimationGraphNodeState* AnimationGraphInstance::GetNodeState(uint32 nodeIndex) const
{
    return nodeIndex < m_nodeStates.Size() ? &m_nodeStates[nodeIndex] : nullptr;
}

float AnimationGraphInstance::GetNodeWeight(uint32 nodeIndex) const
{
    return nodeIndex < m_nodeStates.Size() ? m_nodeStates[nodeIndex].weight : 0.0f;
}

float AnimationGraphInstance::GetNodeTime(uint32 nodeIndex) const
{
    return nodeIndex < m_nodeStates.Size() ? m_nodeStates[nodeIndex].time : 0.0f;
}

void AnimationGraphInstance::PlaySlot(Name slot, const Handle<Animation>& clip, float blendTime, float playRate)
{
    if (!slot.IsValid() || !clip.IsValid())
    {
        return;
    }

    Mutex::Guard guard(m_pendingMutex);

    AnimationSlotPlayback playback;
    playback.clip = clip;
    playback.playRate = playRate;
    playback.blendTime = MathUtil::Max(blendTime, 0.0f);

    m_pendingSlotPlays.PushBack({ slot, std::move(playback) });

    if (!m_publishedSlots.Contains(slot))
    {
        m_publishedSlots.PushBack(slot);
    }
}

void AnimationGraphInstance::StopSlot(Name slot, float blendTime)
{
    Mutex::Guard guard(m_pendingMutex);

    m_pendingSlotStops.PushBack({ slot, MathUtil::Max(blendTime, 0.0f) });
}

bool AnimationGraphInstance::IsSlotPlaying(Name slot) const
{
    Mutex::Guard guard(m_pendingMutex);

    return m_publishedSlots.Contains(slot);
}

Vec3f AnimationGraphInstance::GetRootMotion() const
{
    Mutex::Guard guard(m_pendingMutex);

    return m_publishedRootMotion;
}

float AnimationGraphInstance::GetRootMotionWeight() const
{
    Mutex::Guard guard(m_pendingMutex);

    return MathUtil::Min(m_publishedRootMotionWeight, 1.0f);
}

void AnimationGraphInstance::ApplySlotRequests()
{
    for (Pair<Name, AnimationSlotPlayback>& request : m_pendingSlotPlays)
    {
        auto it = m_slots.Find(request.first);

        // a clip replacing a running one starts from the weight that one had reached
        const float weight = it != m_slots.End() ? it->second.weight : 0.0f;

        AnimationSlotPlayback& playback = m_slots[request.first];
        playback = std::move(request.second);
        playback.weight = weight;

        // the tracks stay loaded for as long as the slot plays the clip
        for (const Handle<AnimationTrack>& track : playback.clip->GetTracks())
        {
            Assert(track.IsValid(), "Slot clip '{}' has an empty track", playback.clip->GetName());

            playback.trackLocks.EmplaceBack(MakeUnique<TSharedResLock<AssetObject>>(*track));
        }
    }

    for (const Pair<Name, float>& request : m_pendingSlotStops)
    {
        auto it = m_slots.Find(request.first);

        if (it != m_slots.End())
        {
            it->second.isStopping = true;
            it->second.blendTime = request.second;
        }
    }

    m_pendingSlotPlays.Clear();
    m_pendingSlotStops.Clear();
}

void AnimationGraphInstance::FireNotifies(const Animation& clip, float previousTime, float time)
{
    for (const AnimationNotify& notify : clip.GetNotifies())
    {
        if (CrossedTime(previousTime, time, notify.time) && !m_firedNotifies.Contains(notify.name))
        {
            m_firedNotifies.PushBack(notify.name);
        }
    }
}

void AnimationGraphInstance::AdvanceSlots(float delta)
{
    Array<Name> finishedSlots;

    for (auto& it : m_slots)
    {
        AnimationSlotPlayback& playback = it.second;

        const float length = playback.clip.IsValid() ? playback.clip->GetLength() : 0.0f;

        playback.previousTime = playback.time;
        playback.time = MathUtil::Min(playback.time + delta * playback.playRate, length);

        // blend out early enough to be gone when the clip ends
        if (playback.playRate > 0.0f && (length - playback.time) / playback.playRate <= playback.blendTime)
        {
            playback.isStopping = true;
        }

        const float target = playback.isStopping ? 0.0f : 1.0f;
        const float step = playback.blendTime > 0.0f ? delta / playback.blendTime : 1.0f;

        if (playback.weight < target)
        {
            playback.weight = MathUtil::Min(playback.weight + step, target);
        }
        else
        {
            playback.weight = MathUtil::Max(playback.weight - step, target);
        }

        if (playback.clip.IsValid() && playback.weight >= NotifyWeightThreshold)
        {
            FireNotifies(*playback.clip, playback.previousTime, playback.time);
        }

        if (playback.isStopping && playback.weight <= 0.0f)
        {
            finishedSlots.PushBack(it.first);
        }
    }

    for (Name finishedSlot : finishedSlots)
    {
        m_slots.Erase(finishedSlot);
    }
}

void AnimationGraphInstance::SetDebugCapture(bool capture)
{
    Mutex::Guard guard(m_pendingMutex);

    m_isDebugCaptured = capture;

    if (!capture)
    {
        m_debugNodeStates.Clear();
        m_debugValues.Clear();
    }
}

bool AnimationGraphInstance::IsDebugCaptured() const
{
    Mutex::Guard guard(m_pendingMutex);

    return m_isDebugCaptured;
}

float AnimationGraphInstance::GetDebugNodeWeight(uint32 nodeIndex) const
{
    Mutex::Guard guard(m_pendingMutex);

    return nodeIndex < m_debugNodeStates.Size() ? m_debugNodeStates[nodeIndex].weight : 0.0f;
}

String AnimationGraphInstance::GetDebugNodeStatus(uint32 nodeIndex) const
{
    const AnimationGraphNode* node = m_graph.IsValid() ? m_graph->GetNode(nodeIndex) : nullptr;

    Mutex::Guard guard(m_pendingMutex);

    if (!node || nodeIndex >= m_debugNodeStates.Size() || m_debugNodeStates[nodeIndex].weight <= 0.0f)
    {
        return String::empty;
    }

    if (const AnimationStateMachineNode* stateMachine = TryCast<AnimationStateMachineNode>(node))
    {
        return stateMachine->DescribeState(m_debugNodeStates[nodeIndex]);
    }

    if (const AnimationMotionMatchingNode* motionMatching = TryCast<AnimationMotionMatchingNode>(node))
    {
        return motionMatching->DescribeState(m_debugNodeStates[nodeIndex]);
    }

    return String::empty;
}

uint32 AnimationGraphInstance::GetDebugCurrentState(uint32 nodeIndex) const
{
    Mutex::Guard guard(m_pendingMutex);

    if (nodeIndex >= m_debugNodeStates.Size() || m_debugNodeStates[nodeIndex].weight <= 0.0f)
    {
        return InvalidAnimationGraphNode;
    }

    return m_debugNodeStates[nodeIndex].currentState;
}

uint32 AnimationGraphInstance::GetDebugPreviousState(uint32 nodeIndex) const
{
    Mutex::Guard guard(m_pendingMutex);

    return nodeIndex < m_debugNodeStates.Size() ? m_debugNodeStates[nodeIndex].previousState : InvalidAnimationGraphNode;
}

float AnimationGraphInstance::GetDebugBlendAlpha(uint32 nodeIndex) const
{
    Mutex::Guard guard(m_pendingMutex);

    return nodeIndex < m_debugNodeStates.Size() ? m_debugNodeStates[nodeIndex].blendAlpha : 1.0f;
}

uint32 AnimationGraphInstance::GetDebugTransition(uint32 nodeIndex) const
{
    Mutex::Guard guard(m_pendingMutex);

    if (nodeIndex >= m_debugNodeStates.Size())
    {
        return InvalidAnimationGraphNode;
    }

    const AnimationGraphNodeState& state = m_debugNodeStates[nodeIndex];

    if (state.previousState == InvalidAnimationGraphNode && !state.isInertializing)
    {
        return InvalidAnimationGraphNode;
    }

    return state.transitionIndex;
}

float AnimationGraphInstance::GetDebugStateProgress(uint32 nodeIndex) const
{
    Mutex::Guard guard(m_pendingMutex);

    if (nodeIndex >= m_debugNodeStates.Size() || !m_graph.IsValid())
    {
        return 0.0f;
    }

    const uint32 stateClip = m_debugNodeStates[nodeIndex].stateClip;
    const AnimationClipNode* clipNode = TryCast<AnimationClipNode>(m_graph->GetNode(stateClip));

    if (!clipNode || !clipNode->clip.IsValid() || clipNode->clip->GetLength() <= 0.0f || stateClip >= m_debugNodeStates.Size())
    {
        return 0.0f;
    }

    return MathUtil::Clamp(m_debugNodeStates[stateClip].time / clipNode->clip->GetLength(), 0.0f, 1.0f);
}

Vec2f AnimationGraphInstance::GetDebugValue(Name parameter) const
{
    Mutex::Guard guard(m_pendingMutex);

    auto it = m_parameterIndices.Find(parameter);

    if (it == m_parameterIndices.End() || it->second >= m_debugValues.Size())
    {
        return Vec2f::Zero();
    }

    return m_debugValues[it->second];
}

void AnimationGraphInstance::Update(float delta, const Skeleton& skeleton)
{
    HYP_SCOPE;

    if (!m_graph.IsValid())
    {
        return;
    }

    if (m_boundSkeleton.GetUnsafe() != &skeleton || m_boundSkeletonRevision != skeleton.GetRevision())
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

        ApplySlotRequests();
    }

    ++m_updateSerial;

    for (AnimationGraphNodeState& state : m_nodeStates)
    {
        state.wasActive = state.weight > 0.0f;
        state.weight = 0.0f;
        state.rootMotionWeight = 0.0f;
        state.isRegistered = false;
    }

    m_activeClips.Clear();
    m_firedNotifies.Clear();
    m_rootMotion = Vec3f::Zero();
    m_rootMotionWeight = 0.0f;
    m_rootMotionScale = 1.0f;

    AdvanceSlots(delta);

    AnimationGraphContext context(*this, skeleton.GetBindData(), delta);
    context.UpdateInput(m_graph->GetRootNode(), 1.0f);

    AdvanceClips(context, delta);

    Mutex::Guard guard(m_pendingMutex);
    m_publishedNotifies = m_firedNotifies;
    m_publishedRootMotion = m_rootMotion;
    m_publishedRootMotionWeight = m_rootMotionWeight;

    m_publishedSlots.Clear();

    for (const auto& it : m_slots)
    {
        m_publishedSlots.PushBack(it.first);
    }

    for (const Pair<Name, AnimationSlotPlayback>& request : m_pendingSlotPlays)
    {
        if (!m_publishedSlots.Contains(request.first))
        {
            m_publishedSlots.PushBack(request.first);
        }
    }

    if (m_isDebugCaptured)
    {
        m_debugNodeStates = m_nodeStates;
        m_debugValues = m_values;
    }
}

void AnimationGraphInstance::AdvanceClip(AnimationGraphContext& context, uint32 nodeIndex, const AnimationClipNode& clipNode, float time, bool wrapped, bool fireNotifies)
{
    AnimationGraphNodeState& state = m_nodeStates[nodeIndex];

    // a clip that just became active starts fresh instead of firing everything since it last played
    state.previousTime = state.wasActive ? state.time : time;
    state.time = time;

    if (clipNode.clip.IsValid() && clipNode.clip->HasRootMotion())
    {
        m_rootMotionWeight += state.rootMotionWeight;
    }

    if (clipNode.clip.IsValid() && clipNode.clip->HasRootMotion() && state.wasActive)
    {
        m_rootMotion += PoseOps::GetRootMotion(*clipNode.clip, context.GetClipBinding(*clipNode.clip), m_modelUp, state.previousTime, state.time, wrapped) * state.rootMotionWeight;
    }

    if (!fireNotifies || !clipNode.clip.IsValid())
    {
        return;
    }

    FireNotifies(*clipNode.clip, state.previousTime, state.time);
}

float AnimationGraphInstance::GetPlayRate(const AnimationClipNode& clipNode) const
{
    if (!clipNode.speedParameter.IsValid() || clipNode.referenceSpeed <= 0.0f)
    {
        return clipNode.playRate;
    }

    auto it = m_parameterIndices.Find(clipNode.speedParameter);
    const float speed = it != m_parameterIndices.End() ? m_values[it->second].x : 0.0f;

    return clipNode.playRate * speed / clipNode.referenceSpeed;
}

float AnimationGraphInstance::GetSyncGroupCycleRate(const Array<uint32>& members) const
{
    float totalWeight = 0.0f;
    float blendedCycleDuration = 0.0f;

    for (uint32 nodeIndex : members)
    {
        const AnimationClipNode& clipNode = static_cast<const AnimationClipNode&>(*m_graph->GetNode(nodeIndex));
        
        const float weight = m_nodeStates[nodeIndex].weight;
        const float length = clipNode.clip.IsValid() ? clipNode.clip->GetLength() : 0.0f;
        const float rate = MathUtil::Abs(GetPlayRate(clipNode));

        if (weight <= 0.0f || length <= 0.0f || rate <= MathUtil::epsilonF)
        {
            continue;
        }

        blendedCycleDuration += weight * length / rate;
        totalWeight += weight;
    }

    return blendedCycleDuration > 0.0f ? totalWeight / blendedCycleDuration : 0.0f;
}

void AnimationGraphInstance::AdvanceClips(AnimationGraphContext& context, float delta)
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

        float time = state.wasActive ? state.time + delta * GetPlayRate(clipNode) : clipNode.startOffset;
        bool wrapped = false;

        if (clipNode.timeParameter.IsValid())
        {
            auto it = m_parameterIndices.Find(clipNode.timeParameter);
            time = it != m_parameterIndices.End() ? m_values[it->second].x : 0.0f;

            if (clipNode.normalizedTime)
            {
                time *= length;
            }

            // a driven phase only reports where it is, so it went the short way round the loop
            const float wrappedTime = WrapClipTime(time, length, clipNode.loop);
            wrapped = state.wasActive && clipNode.loop && length > 0.0f && MathUtil::Abs(wrappedTime - state.time) > length * 0.5f;
        }
        else
        {
            wrapped = state.wasActive && clipNode.loop && length > 0.0f && (time >= length || time < 0.0f);
        }

        AdvanceClip(context, nodeIndex, clipNode, WrapClipTime(time, length, clipNode.loop), wrapped, state.weight >= NotifyWeightThreshold);
    }

    // one clip leads each group; the others follow its normalized phase so their cycles stay in step
    for (const auto& groupIt : syncGroups)
    {
        const Array<uint32>& members = groupIt.second;

        uint32 leader = members[0];

        // where the group's cycle was last update, from any member that played along
        float previousPhase = -1.0f;

        for (uint32 nodeIndex : members)
        {
            const AnimationClipNode& clipNode = static_cast<const AnimationClipNode&>(*m_graph->GetNode(nodeIndex));
            const AnimationClipNode& leaderNode = static_cast<const AnimationClipNode&>(*m_graph->GetNode(leader));
            const AnimationGraphNodeState& state = m_nodeStates[nodeIndex];

            const float length = clipNode.clip.IsValid() ? clipNode.clip->GetLength() : 0.0f;

            if (state.wasActive && length > 0.0f)
            {
                previousPhase = state.time / length;
            }

            bool isLeader = false;

            if (clipNode.syncLeader != leaderNode.syncLeader)
            {
                isLeader = clipNode.syncLeader;
            }
            else
            {
                isLeader = state.weight > m_nodeStates[leader].weight;
            }

            if (isLeader)
            {
                leader = nodeIndex;
            }
        }

        const AnimationClipNode& leaderNode = static_cast<const AnimationClipNode&>(*m_graph->GetNode(leader));
        const AnimationGraphNodeState& leaderState = m_nodeStates[leader];
        const float leaderLength = leaderNode.clip.IsValid() ? leaderNode.clip->GetLength() : 0.0f;

        // a leader that just joined picks the cycle up where the group had it, so the followers don't jump
        const bool continuesCycle = leaderState.wasActive || previousPhase >= 0.0f;
        const float leaderFromTime = leaderState.wasActive ? leaderState.time : previousPhase * leaderLength;
        const float leaderDirection = GetPlayRate(leaderNode) < 0.0f ? -1.0f : 1.0f;
        const float leaderRate = leaderDirection * GetSyncGroupCycleRate(members) * leaderLength;
        const float unwrappedLeaderTime = continuesCycle ? leaderFromTime + delta * leaderRate : leaderNode.startOffset;

        const bool wrapped = continuesCycle && leaderNode.loop && leaderLength > 0.0f && (unwrappedLeaderTime >= leaderLength || unwrappedLeaderTime < 0.0f);

        const float leaderTime = WrapClipTime(unwrappedLeaderTime, leaderLength, leaderNode.loop);
        const float phase = leaderLength > 0.0f ? leaderTime / leaderLength : 0.0f;

        for (uint32 nodeIndex : members)
        {
            const AnimationClipNode& clipNode = static_cast<const AnimationClipNode&>(*m_graph->GetNode(nodeIndex));
            const float length = clipNode.clip.IsValid() ? clipNode.clip->GetLength() : 0.0f;

            AdvanceClip(context, nodeIndex, clipNode, nodeIndex == leader ? leaderTime : phase * length, wrapped, nodeIndex == leader);
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

    if (m_boundSkeleton.GetUnsafe() != &skeleton || m_boundSkeletonRevision != skeleton.GetRevision())
    {
        ResetBindings(skeleton);
    }

    m_scratchDepth = 0;

    AnimationGraphContext context(*this, bindData, 0.0f);
    context.EvaluateInput(m_graph->GetRootNode(), outPose);
}

#pragma endregion AnimationGraphInstance

} // namespace Hyperion
