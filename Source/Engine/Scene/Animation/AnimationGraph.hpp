/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Asset/AssetObject.hpp>

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/String.hpp>

#include <Core/Math/Vector2.hpp>
#include <Core/Math/Transform.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Utilities/Span.hpp>
#include <Core/Utilities/Result.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

class Animation;
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

    // a float's value is x, a bool's is x != 0
    HYP_FIELD(Property = "DefaultValue", Serialize = true, Editor = true)
    Vec2f defaultValue;
};

HYP_STRUCT()
struct AnimationBlendSample
{
    HYP_STRUCT_BODY(AnimationBlendSample);

    HYP_FIELD(Property = "Input", Serialize = true, Editor = true)
    uint32 input = InvalidAnimationGraphNode;

    // a Blend1D sample only uses x
    HYP_FIELD(Property = "Position", Serialize = true, Editor = true)
    Vec2f position;
};

HYP_CLASS(Abstract)
class ENGINE_API AnimationGraphNode : public ObjectBase
{
    HYP_OBJECT_BODY(AnimationGraphNode);

public:
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

    // nodes with a variable number of inputs (blend samples) take a new one here
    virtual bool AddInput(uint32 nodeIndex)
    {
        return false;
    }

    virtual void RemoveInput(uint32 slot)
    {
        SetInput(slot, InvalidAnimationGraphNode);
    }

    // renumbers inputs after removedIndex leaves the graph, dropping inputs that pointed at it
    virtual void OnNodeRemoved(uint32 removedIndex);

    void GetInputs(Array<uint32>& outInputs) const;

    // weight is this node's share of the output; the update pass runs before any pose is built
    virtual void Update(AnimationGraphContext& context, uint32 nodeIndex, float weight) const = 0;

    virtual void Evaluate(AnimationGraphContext& context, uint32 nodeIndex, Span<Transform> outPose) const = 0;

    HYP_FIELD(Property = "Name", Serialize = true, Editor = true)
    Name name;

    HYP_FIELD(Property = "EditorPosition", Serialize = true, Editor = false)
    Vec2f editorPosition;
};

HYP_CLASS()
class ENGINE_API AnimationClipNode final : public AnimationGraphNode
{
    HYP_OBJECT_BODY(AnimationClipNode);

public:
    void Update(AnimationGraphContext& context, uint32 nodeIndex, float weight) const override;
    void Evaluate(AnimationGraphContext& context, uint32 nodeIndex, Span<Transform> outPose) const override;

    HYP_FIELD(Property = "Clip", Serialize = true, Editor = true)
    Handle<Animation> clip;

    HYP_FIELD(Property = "PlayRate", Serialize = true, Editor = true)
    float playRate = 1.0f;

    HYP_FIELD(Property = "Loop", Serialize = true, Editor = true)
    bool loop = true;

    HYP_FIELD(Property = "StartOffset", Serialize = true, Editor = true)
    float startOffset = 0.0f;

    // when set, the clip's time is this float parameter instead of advancing
    HYP_FIELD(Property = "TimeParameter", Serialize = true, Editor = true)
    Name timeParameter;

    HYP_FIELD(Property = "SyncGroup", Serialize = true, Editor = true)
    Name syncGroup;

    HYP_FIELD(Property = "SyncLeader", Serialize = true, Editor = true)
    bool syncLeader = false;
};

HYP_CLASS()
class ENGINE_API AnimationBlend1DNode final : public AnimationGraphNode
{
    HYP_OBJECT_BODY(AnimationBlend1DNode);

public:
    uint32 NumInputs() const override;
    Name GetInputName(uint32 slot) const override;
    uint32 GetInput(uint32 slot) const override;
    void SetInput(uint32 slot, uint32 nodeIndex) override;
    bool AddInput(uint32 nodeIndex) override;
    void RemoveInput(uint32 slot) override;
    void OnNodeRemoved(uint32 removedIndex) override;

    void Update(AnimationGraphContext& context, uint32 nodeIndex, float weight) const override;
    void Evaluate(AnimationGraphContext& context, uint32 nodeIndex, Span<Transform> outPose) const override;

    // one weight per sample, two at most non-zero
    void ComputeWeights(float value, Array<float>& outWeights) const;

    HYP_FIELD(Property = "Parameter", Serialize = true, Editor = true)
    Name parameter;

    HYP_FIELD(Property = "Samples", Serialize = true, Editor = true)
    Array<AnimationBlendSample> samples;
};

// gradient band interpolation over freely placed samples, exact at each sample
HYP_CLASS()
class ENGINE_API AnimationBlend2DNode final : public AnimationGraphNode
{
    HYP_OBJECT_BODY(AnimationBlend2DNode);

public:
    uint32 NumInputs() const override;
    Name GetInputName(uint32 slot) const override;
    uint32 GetInput(uint32 slot) const override;
    void SetInput(uint32 slot, uint32 nodeIndex) override;
    bool AddInput(uint32 nodeIndex) override;
    void RemoveInput(uint32 slot) override;
    void OnNodeRemoved(uint32 removedIndex) override;

    void Update(AnimationGraphContext& context, uint32 nodeIndex, float weight) const override;
    void Evaluate(AnimationGraphContext& context, uint32 nodeIndex, Span<Transform> outPose) const override;

    void ComputeWeights(const Vec2f& value, Array<float>& outWeights) const;

    HYP_FIELD(Property = "Parameter", Serialize = true, Editor = true)
    Name parameter;

    HYP_FIELD(Property = "Samples", Serialize = true, Editor = true)
    Array<AnimationBlendSample> samples;
};

HYP_CLASS()
class ENGINE_API AnimationLayerNode final : public AnimationGraphNode
{
    HYP_OBJECT_BODY(AnimationLayerNode);

public:
    uint32 NumInputs() const override;
    Name GetInputName(uint32 slot) const override;
    uint32 GetInput(uint32 slot) const override;
    void SetInput(uint32 slot, uint32 nodeIndex) override;

    void Update(AnimationGraphContext& context, uint32 nodeIndex, float weight) const override;
    void Evaluate(AnimationGraphContext& context, uint32 nodeIndex, Span<Transform> outPose) const override;

    HYP_FIELD(Property = "Base", Serialize = true, Editor = true)
    uint32 base = InvalidAnimationGraphNode;

    HYP_FIELD(Property = "Layer", Serialize = true, Editor = true)
    uint32 layer = InvalidAnimationGraphNode;

    HYP_FIELD(Property = "Mask", Serialize = true, Editor = true)
    Handle<BoneMask> mask;

    HYP_FIELD(Property = "Weight", Serialize = true, Editor = true)
    float weight = 1.0f;

    // when set, this float parameter replaces weight
    HYP_FIELD(Property = "WeightParameter", Serialize = true, Editor = true)
    Name weightParameter;
};

HYP_CLASS()
class ENGINE_API AnimationAdditiveNode final : public AnimationGraphNode
{
    HYP_OBJECT_BODY(AnimationAdditiveNode);

public:
    uint32 NumInputs() const override;
    Name GetInputName(uint32 slot) const override;
    uint32 GetInput(uint32 slot) const override;
    void SetInput(uint32 slot, uint32 nodeIndex) override;

    void Update(AnimationGraphContext& context, uint32 nodeIndex, float weight) const override;
    void Evaluate(AnimationGraphContext& context, uint32 nodeIndex, Span<Transform> outPose) const override;

    HYP_FIELD(Property = "Base", Serialize = true, Editor = true)
    uint32 base = InvalidAnimationGraphNode;

    // its difference from the rest pose is added onto the base
    HYP_FIELD(Property = "Additive", Serialize = true, Editor = true)
    uint32 additive = InvalidAnimationGraphNode;

    HYP_FIELD(Property = "Weight", Serialize = true, Editor = true)
    float weight = 1.0f;

    HYP_FIELD(Property = "WeightParameter", Serialize = true, Editor = true)
    Name weightParameter;
};

HYP_CLASS()
class ENGINE_API AnimationChainTwistNode final : public AnimationGraphNode
{
    HYP_OBJECT_BODY(AnimationChainTwistNode);

public:
    uint32 NumInputs() const override;
    Name GetInputName(uint32 slot) const override;
    uint32 GetInput(uint32 slot) const override;
    void SetInput(uint32 slot, uint32 nodeIndex) override;

    void Update(AnimationGraphContext& context, uint32 nodeIndex, float weight) const override;
    void Evaluate(AnimationGraphContext& context, uint32 nodeIndex, Span<Transform> outPose) const override;

    HYP_FIELD(Property = "Input", Serialize = true, Editor = true)
    uint32 input = InvalidAnimationGraphNode;

    HYP_FIELD(Property = "RootBone", Serialize = true, Editor = true, EditorType = "Bone")
    Name rootBone;

    HYP_FIELD(Property = "EndBone", Serialize = true, Editor = true, EditorType = "Bone")
    Name endBone;

    // radians, spread evenly over the chain
    HYP_FIELD(Property = "Angle", Serialize = true, Editor = true)
    float angle = 0.0f;

    HYP_FIELD(Property = "AngleParameter", Serialize = true, Editor = true)
    Name angleParameter;

    // seconds to close most of the gap to a new angle; 0 follows it immediately
    HYP_FIELD(Property = "SmoothingTime", Serialize = true, Editor = true)
    float smoothingTime = 0.0f;
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
    HYP_FIELD(Property = "Parameter", Serialize = true, Editor = true)
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
};

HYP_STRUCT()
struct AnimationState
{
    HYP_STRUCT_BODY(AnimationState);

    HYP_FIELD(Property = "Name", Serialize = true, Editor = true)
    Name name;

    HYP_FIELD(Property = "Input", Serialize = true, Editor = true)
    uint32 input = InvalidAnimationGraphNode;
};

// each state is an input; transitions crossfade between them
HYP_CLASS()
class ENGINE_API AnimationStateMachineNode final : public AnimationGraphNode
{
    HYP_OBJECT_BODY(AnimationStateMachineNode);

public:
    uint32 NumInputs() const override;
    Name GetInputName(uint32 slot) const override;
    uint32 GetInput(uint32 slot) const override;
    void SetInput(uint32 slot, uint32 nodeIndex) override;
    bool AddInput(uint32 nodeIndex) override;
    void RemoveInput(uint32 slot) override;

    void Update(AnimationGraphContext& context, uint32 nodeIndex, float weight) const override;
    void Evaluate(AnimationGraphContext& context, uint32 nodeIndex, Span<Transform> outPose) const override;

    Name GetStateName(uint32 stateIndex) const;

    // "Walk", or "Walk -> Run 40%" mid-transition
    String DescribeState(const AnimationGraphNodeState& state) const;

    HYP_FIELD(Property = "States", Serialize = true, Editor = true)
    Array<AnimationState> states;

    HYP_FIELD(Property = "Transitions", Serialize = true, Editor = true)
    Array<AnimationStateTransition> transitions;

    HYP_FIELD(Property = "EntryState", Serialize = true, Editor = true)
    uint32 entryState = 0;

private:
    uint32 ChooseTransition(AnimationGraphContext& context, const AnimationGraphNodeState& state) const;
    bool MeetsCondition(AnimationGraphContext& context, const AnimationTransitionCondition& condition, const AnimationGraphNodeState& state) const;
    void StartTransition(AnimationGraphNodeState& state, uint32 transitionIndex) const;
};

HYP_CLASS(AssetBucket = "Animations")
class ENGINE_API AnimationGraph final : public AssetObject
{
    HYP_OBJECT_BODY(AnimationGraph);

public:
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
    HYP_FIELD(Property = "Nodes", Serialize = true)
    Array<Handle<AnimationGraphNode>> m_nodes;

    HYP_FIELD(Property = "RootNode", Serialize = true)
    uint32 m_rootNode = 0;

    HYP_FIELD(Property = "Parameters", Serialize = true)
    Array<AnimationGraphParameter> m_parameters;

    uint32 m_revision = 0;
};

} // namespace Hyperion
