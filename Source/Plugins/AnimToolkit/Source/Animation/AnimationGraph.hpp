/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Animation/AnimationMemory.hpp>
#include <Asset/AssetObject.hpp>

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/String.hpp>

#include <Core/Math/Vector2.hpp>
#include <Core/Math/Transform.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Utilities/Span.hpp>

#include <Core/Functional/Proc.hpp>
#include <Core/Utilities/Result.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

class Animation;
class Skeleton;
class BoneMask;
class AnimationGraphContext;
struct AnimationGraphNodeState;

static constexpr uint32 InvalidAnimationGraphNode = ~0u;

HYP_ENUM()
enum class AnimationGraphParameterType : uint8
{
    Float = 0,
    Bool,
    Trigger,
    Vector2
};

HYP_STRUCT()
struct AnimationGraphParameter
{
    HYP_STRUCT_BODY(AnimationGraphParameter);

    HYP_FIELD(Property = "Name", Serialize = true, Editor = true)
    Name name;

    HYP_FIELD(Property = "Type", Serialize = true, Editor = true)
    AnimationGraphParameterType type = AnimationGraphParameterType::Float;

    HYP_FIELD(Property = "DefaultValue", Serialize = true, Editor = true)
    Vec2f defaultValue;
};

HYP_STRUCT()
struct AnimationBlendSample
{
    HYP_STRUCT_BODY(AnimationBlendSample);

    HYP_FIELD(Property = "Input", Serialize = true, Editor = false)
    uint32 input = InvalidAnimationGraphNode;

    // a Blend1D sample only uses x
    HYP_FIELD(Property = "Position", Serialize = true, Editor = true)
    Vec2f position;
};

HYP_CLASS(Abstract)
class ANIMATION_API AnimationGraphNode : public ObjectBase
{
    HYP_OBJECT_BODY(AnimationGraphNode);

public:
    static Pool* GetAllocator() { return g_animPool; }

    ~AnimationGraphNode() override = default;

    virtual uint32 NumInputs() const
    {
        return 0;
    }

    virtual Name GetInputName(uint32 slot) const
    {
        return Name::Invalid();
    }

    virtual uint32 GetInput(uint32 slot) const
    {
        return InvalidAnimationGraphNode;
    }

    virtual void SetInput(uint32 slot, uint32 nodeIndex)
    {
    }

    virtual bool CanAddInputs() const
    {
        return false;
    }

    virtual bool AddInput(uint32 nodeIndex)
    {
        return false;
    }

    virtual void RemoveInput(uint32 slot)
    {
        SetInput(slot, InvalidAnimationGraphNode);
    }

    virtual void OnNodeRemoved(uint32 removedIndex);

    virtual void ForEachParameter(const ProcRef<void(Name&)>& visit)
    {
    }

    void GetInputs(Array<uint32>& outInputs) const;

    virtual void Update(AnimationGraphContext& context, uint32 nodeIndex, float weight) const = 0;

    virtual void Evaluate(AnimationGraphContext& context, uint32 nodeIndex, Span<Transform> outPose) const = 0;

    HYP_FIELD(Property = "Name", Serialize = true, Editor = true)
    Name name;

    HYP_FIELD(Property = "EditorPosition", Serialize = true, Editor = false)
    Vec2f editorPosition;
};

HYP_CLASS()
class ANIMATION_API AnimationClipNode final : public AnimationGraphNode
{
    HYP_OBJECT_BODY(AnimationClipNode);

public:
    void Update(AnimationGraphContext& context, uint32 nodeIndex, float weight) const override;
    void Evaluate(AnimationGraphContext& context, uint32 nodeIndex, Span<Transform> outPose) const override;

    void ForEachParameter(const ProcRef<void(Name&)>& visit) override
    {
        visit(timeParameter);
        visit(speedParameter);
    }

    HYP_FIELD(Property = "Clip", Serialize = true, Editor = true)
    Handle<Animation> clip;

    HYP_FIELD(Property = "PlayRate", Serialize = true, Editor = true)
    float playRate = 1.0f;

    HYP_FIELD(Property = "Loop", Serialize = true, Editor = true)
    bool loop = true;

    HYP_FIELD(Property = "StartOffset", Serialize = true, Editor = true)
    float startOffset = 0.0f;

    // when set, the clip's time is this float parameter instead of advancing
    HYP_FIELD(Property = "TimeParameter", Serialize = true, Editor = true, EditorType = "GraphParameter")
    Name timeParameter;

    HYP_FIELD(Property = "SyncGroup", Serialize = true, Editor = true)
    Name syncGroup;

    HYP_FIELD(Property = "SyncLeader", Serialize = true, Editor = true)
    bool syncLeader = false;

    HYP_FIELD(Property = "SpeedParameter", Serialize = true, Editor = true, EditorType = "GraphParameter")
    Name speedParameter;

    HYP_FIELD(Property = "ReferenceSpeed", Serialize = true, Editor = true)
    float referenceSpeed = 1.0f;

    HYP_FIELD(Property = "NormalizedTime", Serialize = true, Editor = true)
    bool normalizedTime = false;
};

HYP_CLASS()
class ANIMATION_API AnimationBlend1DNode final : public AnimationGraphNode
{
    HYP_OBJECT_BODY(AnimationBlend1DNode);

public:
    uint32 NumInputs() const override;
    Name GetInputName(uint32 slot) const override;
    uint32 GetInput(uint32 slot) const override;
    void SetInput(uint32 slot, uint32 nodeIndex) override;
    bool CanAddInputs() const override
    {
        return true;
    }

    bool AddInput(uint32 nodeIndex) override;
    void RemoveInput(uint32 slot) override;
    void OnNodeRemoved(uint32 removedIndex) override;

    void Update(AnimationGraphContext& context, uint32 nodeIndex, float weight) const override;
    void Evaluate(AnimationGraphContext& context, uint32 nodeIndex, Span<Transform> outPose) const override;

    void ForEachParameter(const ProcRef<void(Name&)>& visit) override
    {
        visit(parameter);
    }

    void ComputeWeights(float value, Array<float>& outWeights) const;

    HYP_FIELD(Property = "Parameter", Serialize = true, Editor = true, EditorType = "GraphParameter")
    Name parameter;

    HYP_FIELD(Property = "Samples", Serialize = true, Editor = false)
    Array<AnimationBlendSample> samples;

    HYP_FIELD(Property = "Smooth", Serialize = true, Editor = true)
    bool smooth = false;
};

HYP_CLASS()
class ANIMATION_API AnimationBlend2DNode final : public AnimationGraphNode
{
    HYP_OBJECT_BODY(AnimationBlend2DNode);

public:
    uint32 NumInputs() const override;
    Name GetInputName(uint32 slot) const override;
    uint32 GetInput(uint32 slot) const override;
    void SetInput(uint32 slot, uint32 nodeIndex) override;
    bool CanAddInputs() const override
    {
        return true;
    }

    bool AddInput(uint32 nodeIndex) override;
    void RemoveInput(uint32 slot) override;
    void OnNodeRemoved(uint32 removedIndex) override;

    void Update(AnimationGraphContext& context, uint32 nodeIndex, float weight) const override;
    void Evaluate(AnimationGraphContext& context, uint32 nodeIndex, Span<Transform> outPose) const override;

    void ForEachParameter(const ProcRef<void(Name&)>& visit) override
    {
        visit(parameter);
    }

    void ComputeWeights(const Vec2f& value, Array<float>& outWeights) const;

    HYP_FIELD(Property = "Parameter", Serialize = true, Editor = true, EditorType = "GraphParameter")
    Name parameter;

    HYP_FIELD(Property = "Samples", Serialize = true, Editor = false)
    Array<AnimationBlendSample> samples;
};

HYP_CLASS()
class ANIMATION_API AnimationLayerNode final : public AnimationGraphNode
{
    HYP_OBJECT_BODY(AnimationLayerNode);

public:
    uint32 NumInputs() const override;
    Name GetInputName(uint32 slot) const override;
    uint32 GetInput(uint32 slot) const override;
    void SetInput(uint32 slot, uint32 nodeIndex) override;

    void Update(AnimationGraphContext& context, uint32 nodeIndex, float weight) const override;
    void Evaluate(AnimationGraphContext& context, uint32 nodeIndex, Span<Transform> outPose) const override;

    void ForEachParameter(const ProcRef<void(Name&)>& visit) override
    {
        visit(weightParameter);
    }

    HYP_FIELD(Property = "Base", Serialize = true, Editor = false)
    uint32 base = InvalidAnimationGraphNode;

    HYP_FIELD(Property = "Layer", Serialize = true, Editor = false)
    uint32 layer = InvalidAnimationGraphNode;

    HYP_FIELD(Property = "Mask", Serialize = true, Editor = true)
    Handle<BoneMask> mask;

    HYP_FIELD(Property = "Weight", Serialize = true, Editor = true)
    float weight = 1.0f;

    // when set, this float parameter replaces weight
    HYP_FIELD(Property = "WeightParameter", Serialize = true, Editor = true, EditorType = "GraphParameter")
    Name weightParameter;
};

HYP_CLASS()
class ANIMATION_API AnimationAdditiveNode final : public AnimationGraphNode
{
    HYP_OBJECT_BODY(AnimationAdditiveNode);

public:
    uint32 NumInputs() const override;
    Name GetInputName(uint32 slot) const override;
    uint32 GetInput(uint32 slot) const override;
    void SetInput(uint32 slot, uint32 nodeIndex) override;

    void Update(AnimationGraphContext& context, uint32 nodeIndex, float weight) const override;
    void Evaluate(AnimationGraphContext& context, uint32 nodeIndex, Span<Transform> outPose) const override;

    void ForEachParameter(const ProcRef<void(Name&)>& visit) override
    {
        visit(weightParameter);
    }

    HYP_FIELD(Property = "Base", Serialize = true, Editor = false)
    uint32 base = InvalidAnimationGraphNode;

    // its difference from the rest pose is added onto the base
    HYP_FIELD(Property = "Additive", Serialize = true, Editor = false)
    uint32 additive = InvalidAnimationGraphNode;

    HYP_FIELD(Property = "Weight", Serialize = true, Editor = true)
    float weight = 1.0f;

    HYP_FIELD(Property = "WeightParameter", Serialize = true, Editor = true, EditorType = "GraphParameter")
    Name weightParameter;
};

HYP_CLASS()
class ANIMATION_API AnimationChainTwistNode final : public AnimationGraphNode
{
    HYP_OBJECT_BODY(AnimationChainTwistNode);

public:
    uint32 NumInputs() const override;
    Name GetInputName(uint32 slot) const override;
    uint32 GetInput(uint32 slot) const override;
    void SetInput(uint32 slot, uint32 nodeIndex) override;

    void Update(AnimationGraphContext& context, uint32 nodeIndex, float weight) const override;
    void Evaluate(AnimationGraphContext& context, uint32 nodeIndex, Span<Transform> outPose) const override;

    void ForEachParameter(const ProcRef<void(Name&)>& visit) override
    {
        visit(angleParameter);
    }

    HYP_FIELD(Property = "Input", Serialize = true, Editor = false)
    uint32 input = InvalidAnimationGraphNode;

    HYP_FIELD(Property = "RootBone", Serialize = true, Editor = true, EditorType = "GraphBone")
    Name rootBone;

    HYP_FIELD(Property = "EndBone", Serialize = true, Editor = true, EditorType = "GraphBone")
    Name endBone;

    HYP_FIELD(Property = "Angle", Serialize = true, Editor = true)
    float angle = 0.0f;

    HYP_FIELD(Property = "AngleParameter", Serialize = true, Editor = true, EditorType = "GraphParameter")
    Name angleParameter;

    HYP_FIELD(Property = "SmoothingTime", Serialize = true, Editor = true)
    float smoothingTime = 0.0f;
};

HYP_CLASS()
class ANIMATION_API AnimationSlotNode final : public AnimationGraphNode
{
    HYP_OBJECT_BODY(AnimationSlotNode);

public:
    uint32 NumInputs() const override;
    Name GetInputName(uint32 slot) const override;
    uint32 GetInput(uint32 slot) const override;
    void SetInput(uint32 slot, uint32 nodeIndex) override;

    void Update(AnimationGraphContext& context, uint32 nodeIndex, float weight) const override;
    void Evaluate(AnimationGraphContext& context, uint32 nodeIndex, Span<Transform> outPose) const override;

    HYP_FIELD(Property = "Slot", Serialize = true, Editor = true)
    Name slot;

    HYP_FIELD(Property = "Base", Serialize = true, Editor = false)
    uint32 base = InvalidAnimationGraphNode;

    HYP_FIELD(Property = "Mask", Serialize = true, Editor = true)
    Handle<BoneMask> mask;
};

HYP_CLASS()
class ANIMATION_API AnimationFootIKNode final : public AnimationGraphNode
{
    HYP_OBJECT_BODY(AnimationFootIKNode);

public:
    uint32 NumInputs() const override;
    Name GetInputName(uint32 slot) const override;
    uint32 GetInput(uint32 slot) const override;
    void SetInput(uint32 slot, uint32 nodeIndex) override;

    void Update(AnimationGraphContext& context, uint32 nodeIndex, float weight) const override;
    void Evaluate(AnimationGraphContext& context, uint32 nodeIndex, Span<Transform> outPose) const override;

    void ForEachParameter(const ProcRef<void(Name&)>& visit) override
    {
        visit(leftOffsetParameter);
        visit(rightOffsetParameter);
        visit(weightParameter);
    }

    HYP_FIELD(Property = "Input", Serialize = true, Editor = false)
    uint32 input = InvalidAnimationGraphNode;

    HYP_FIELD(Property = "LeftOffsetParameter", Serialize = true, Editor = true, EditorType = "GraphParameter")
    Name leftOffsetParameter = NAME("LeftFootOffset");

    HYP_FIELD(Property = "RightOffsetParameter", Serialize = true, Editor = true, EditorType = "GraphParameter")
    Name rightOffsetParameter = NAME("RightFootOffset");

    HYP_FIELD(Property = "Weight", Serialize = true, Editor = true)
    float weight = 1.0f;

    HYP_FIELD(Property = "WeightParameter", Serialize = true, Editor = true, EditorType = "GraphParameter")
    Name weightParameter;
};

HYP_ENUM()
enum class AnimationConditionOp : uint8
{
    Greater = 0,
    Less,
    Equal,
    NotEqual,
    IsTrue,
    IsFalse,
    Triggered,
    StateTimeAtLeast,
    StateTimeRemainingBelow,
    StateNormalizedTimeAtLeast
};

HYP_ENUM()
enum class AnimationTransitionMode : uint8
{
    Crossfade = 0,
    Inertialize
};

HYP_ENUM()
enum class AnimationBlendCurve : uint8
{
    Linear = 0,
    SmoothStep,
    EaseIn,
    EaseOut
};

HYP_STRUCT()
struct AnimationTransitionCondition
{
    HYP_STRUCT_BODY(AnimationTransitionCondition);

    // the State* ops read the current state's time and ignore this
    HYP_FIELD(Property = "Parameter", Serialize = true, Editor = true, EditorType = "GraphParameter")
    Name parameter;

    HYP_FIELD(Property = "Op", Serialize = true, Editor = true)
    AnimationConditionOp op = AnimationConditionOp::Greater;

    HYP_FIELD(Property = "Value", Serialize = true, Editor = true)
    float value = 0.0f;
};

HYP_STRUCT()
struct AnimationStateTransition
{
    HYP_STRUCT_BODY(AnimationStateTransition);

    HYP_FIELD(Property = "From", Serialize = true, Editor = true)
    uint32 from = 0;

    // leaves whatever state is current, even mid-transition
    HYP_FIELD(Property = "FromAnyState", Serialize = true, Editor = true)
    bool fromAnyState = false;

    HYP_FIELD(Property = "To", Serialize = true, Editor = true)
    uint32 to = 0;

    // all must hold; none means the transition is taken straight away
    HYP_FIELD(Property = "Conditions", Serialize = true, Editor = true)
    Array<AnimationTransitionCondition> conditions;

    HYP_FIELD(Property = "Duration", Serialize = true, Editor = true)
    float duration = 0.2f;

    HYP_FIELD(Property = "Curve", Serialize = true, Editor = true)
    AnimationBlendCurve curve = AnimationBlendCurve::Linear;

    // the highest priority wins when several transitions are ready
    HYP_FIELD(Property = "Priority", Serialize = true, Editor = true)
    int32 priority = 0;

    HYP_FIELD(Property = "Interruptible", Serialize = true, Editor = true)
    bool interruptible = false;

    HYP_FIELD(Property = "Mode", Serialize = true, Editor = true)
    AnimationTransitionMode mode = AnimationTransitionMode::Crossfade;
};

HYP_STRUCT()
struct AnimationState
{
    HYP_STRUCT_BODY(AnimationState);

    HYP_FIELD(Property = "Name", Serialize = true, Editor = true)
    Name name;

    HYP_FIELD(Property = "Input", Serialize = true, Editor = false)
    uint32 input = InvalidAnimationGraphNode;

    HYP_FIELD(Property = "EditorPosition", Serialize = true, Editor = false)
    Vec2f editorPosition;
};

HYP_CLASS()
class ANIMATION_API AnimationStateMachineNode final : public AnimationGraphNode
{
    HYP_OBJECT_BODY(AnimationStateMachineNode);

public:
    uint32 NumInputs() const override;
    Name GetInputName(uint32 slot) const override;
    uint32 GetInput(uint32 slot) const override;
    void SetInput(uint32 slot, uint32 nodeIndex) override;
    bool CanAddInputs() const override
    {
        return true;
    }

    bool AddInput(uint32 nodeIndex) override;
    void RemoveInput(uint32 slot) override;

    void Update(AnimationGraphContext& context, uint32 nodeIndex, float weight) const override;
    void Evaluate(AnimationGraphContext& context, uint32 nodeIndex, Span<Transform> outPose) const override;

    void ForEachParameter(const ProcRef<void(Name&)>& visit) override
    {
        for (AnimationStateTransition& transition : transitions)
        {
            for (AnimationTransitionCondition& condition : transition.conditions)
            {
                visit(condition.parameter);
            }
        }
    }

    HYP_METHOD()
    Name GetStateName(uint32 stateIndex) const;

    // "Walk", "Walk -> Run 40%" mid-crossfade, or "Run (inertializing 40%)"
    String DescribeState(const AnimationGraphNodeState& state) const;

    HYP_METHOD()
    uint32 NumStates() const
    {
        return uint32(states.Size());
    }

    HYP_METHOD()
    uint32 GetStateInput(uint32 stateIndex) const;

    HYP_METHOD()
    Vec2f GetStatePosition(uint32 stateIndex) const;

    HYP_METHOD()
    uint32 GetEntryState() const
    {
        return entryState;
    }

    HYP_METHOD()
    uint32 NumTransitions() const
    {
        return uint32(transitions.Size());
    }

    HYP_METHOD()
    uint32 GetTransitionFrom(uint32 transitionIndex) const;

    HYP_METHOD()
    bool IsTransitionFromAnyState(uint32 transitionIndex) const;

    HYP_METHOD()
    uint32 GetTransitionTo(uint32 transitionIndex) const;

    HYP_METHOD()
    float GetTransitionDuration(uint32 transitionIndex) const;

    HYP_METHOD()
    uint32 GetTransitionCurve(uint32 transitionIndex) const;

    HYP_METHOD()
    int32 GetTransitionPriority(uint32 transitionIndex) const;

    HYP_METHOD()
    bool IsTransitionInterruptible(uint32 transitionIndex) const;

    HYP_METHOD()
    uint32 GetTransitionMode(uint32 transitionIndex) const;

    HYP_METHOD()
    uint32 NumConditions(uint32 transitionIndex) const;

    HYP_METHOD()
    Name GetConditionParameter(uint32 transitionIndex, uint32 conditionIndex) const;

    HYP_METHOD()
    uint32 GetConditionOp(uint32 transitionIndex, uint32 conditionIndex) const;

    HYP_METHOD()
    float GetConditionValue(uint32 transitionIndex, uint32 conditionIndex) const;

    HYP_FIELD(Property = "States", Serialize = true, Editor = false)
    Array<AnimationState> states;

    HYP_FIELD(Property = "Transitions", Serialize = true, Editor = false)
    Array<AnimationStateTransition> transitions;

    HYP_FIELD(Property = "EntryState", Serialize = true, Editor = false)
    uint32 entryState = 0;

private:
    uint32 ChooseTransition(AnimationGraphContext& context, const AnimationGraphNodeState& state) const;
    bool MeetsCondition(AnimationGraphContext& context, const AnimationTransitionCondition& condition, const AnimationGraphNodeState& state) const;
    void StartTransition(AnimationGraphNodeState& state, uint32 transitionIndex) const;
    void EvaluateStates(AnimationGraphContext& context, const AnimationGraphNodeState& state, Span<Transform> outPose) const;
    bool UsesInertialization() const;
};

HYP_CLASS(AssetBucket = "Animations")
class ANIMATION_API AnimationGraph final : public AssetObject
{
    HYP_OBJECT_BODY(AnimationGraph);

public:
    static Pool* GetAllocator() { return g_animPool; }

    AnimationGraph();
    explicit AnimationGraph(Name name);
    ~AnimationGraph() override;

    HYP_FORCE_INLINE const Array<Handle<AnimationGraphNode>>& GetNodes() const
    {
        return m_nodes;
    }

    HYP_FORCE_INLINE const AnimationGraphNode* GetNode(uint32 nodeIndex) const
    {
        return nodeIndex < m_nodes.Size() ? m_nodes[nodeIndex].Get() : nullptr;
    }

    uint32 AddNode(const Handle<AnimationGraphNode>& node);

    HYP_FORCE_INLINE AnimationGraphNode* GetMutableNode(uint32 nodeIndex)
    {
        return nodeIndex < m_nodes.Size() ? m_nodes[nodeIndex].Get() : nullptr;
    }

    // drops the node and renumbers every reference to the nodes after it
    void RemoveNode(uint32 nodeIndex);

    // creates a node of a concrete AnimationGraphNode class, or null for any other class
    static Handle<AnimationGraphNode> CreateNode(Name className);

    static Array<Name> GetNodeClassNames();

    HYP_METHOD()
    uint32 NumNodes() const
    {
        return uint32(m_nodes.Size());
    }

    HYP_METHOD()
    const Handle<Skeleton>& GetTargetSkeleton() const
    {
        return m_targetSkeleton;
    }

    void SetTargetSkeleton(const Handle<Skeleton>& skeleton)
    {
        m_targetSkeleton = skeleton;
        MarkChanged();
    }

    HYP_METHOD(Property = "RootNode")
    uint32 GetRootNode() const
    {
        return m_rootNode;
    }

    HYP_METHOD(Property = "RootNode")
    void SetRootNode(uint32 rootNode)
    {
        m_rootNode = rootNode;
        MarkChanged();
    }

    HYP_FORCE_INLINE const Array<AnimationGraphParameter>& GetParameters() const
    {
        return m_parameters;
    }

    void AddParameter(Name name, AnimationGraphParameterType type, const Vec2f& defaultValue = Vec2f::Zero());

    void RemoveParameter(Name name);

    HYP_METHOD()
    Array<Name> GetParameterNames() const;

    // ~0u when there's no such parameter
    HYP_METHOD()
    uint32 GetParameterType(Name name) const;

    HYP_METHOD()
    Vec2f GetParameterDefault(Name name) const;

    void SetParameterDefault(Name name, const Vec2f& value);

    void SetParameterType(Name name, AnimationGraphParameterType type);

    void RenameParameter(Name from, Name to);

    HYP_METHOD()
    Array<Name> GetNodeParameters(uint32 nodeIndex) const;

    HYP_METHOD()
    Name GetNodeSubject(uint32 nodeIndex) const;

    HYP_METHOD()
    float GetNodeClipLength(uint32 nodeIndex) const;

    HYP_METHOD()
    Handle<AnimationGraphNode> GetNodeObject(uint32 nodeIndex) const;

    HYP_METHOD()
    Name GetNodeClassName(uint32 nodeIndex) const;

    HYP_METHOD()
    Name GetNodeName(uint32 nodeIndex) const;

    // the first node with this name, or InvalidAnimationGraphNode
    HYP_METHOD()
    uint32 FindNode(Name name) const;

    HYP_METHOD()
    Vec2f GetNodeEditorPosition(uint32 nodeIndex) const;

    HYP_METHOD()
    uint32 GetNodeInputCount(uint32 nodeIndex) const;

    HYP_METHOD()
    Name GetNodeInputName(uint32 nodeIndex, uint32 slot) const;

    HYP_METHOD()
    uint32 GetNodeInput(uint32 nodeIndex, uint32 slot) const;

    HYP_METHOD()
    bool CanNodeAddInputs(uint32 nodeIndex) const;

    // the clips any node plays, so their track data can be kept resident
    Array<Handle<Animation>> CollectClips() const;

    // bumped on any change to nodes, wiring or parameters, so running instances rebuild
    HYP_FORCE_INLINE uint32 GetRevision() const
    {
        return m_revision;
    }

    HYP_FORCE_INLINE void MarkChanged()
    {
        ++m_revision;
    }

    // the nodes, root and parameters as HMF text; LoadContents replaces them with what SaveContents wrote
    Result SaveContents(String& outText) const;
    Result LoadContents(const String& text);

private:
    HYP_FIELD(Property = "Nodes", Serialize = true, Editor = false)
    Array<Handle<AnimationGraphNode>> m_nodes;

    HYP_FIELD(Property = "RootNode", Serialize = true, Editor = false)
    uint32 m_rootNode = 0;

    HYP_FIELD(Property = "TargetSkeleton", Serialize = true, Editor = false)
    Handle<Skeleton> m_targetSkeleton;

    HYP_FIELD(Property = "Parameters", Serialize = true, Editor = false)
    Array<AnimationGraphParameter> m_parameters;

    uint32 m_revision = 0;
};

} // namespace Hyperion
