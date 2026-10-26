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

#include <Core/Math/Vector2.hpp>
#include <Core/Math/Transform.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Utilities/Span.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

class Animation;
class BoneMask;
class AnimationGraphContext;

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

HYP_CLASS(Abstract, NoScriptBindings)
class ENGINE_API AnimationGraphNode : public ObjectBase
{
    HYP_OBJECT_BODY(AnimationGraphNode);

public:
    ~AnimationGraphNode() override = default;

    virtual void GetInputs(Array<uint32>& outInputs) const
    {
    }

    // weight is this node's share of the output; the update pass runs before any pose is built
    virtual void Update(AnimationGraphContext& context, uint32 nodeIndex, float weight) const = 0;

    virtual void Evaluate(AnimationGraphContext& context, uint32 nodeIndex, Span<Transform> outPose) const = 0;

    HYP_FIELD(Property = "Name", Serialize = true, Editor = true)
    Name name;
};

HYP_CLASS(NoScriptBindings)
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

HYP_CLASS(NoScriptBindings)
class ENGINE_API AnimationBlend1DNode final : public AnimationGraphNode
{
    HYP_OBJECT_BODY(AnimationBlend1DNode);

public:
    void GetInputs(Array<uint32>& outInputs) const override;
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
HYP_CLASS(NoScriptBindings)
class ENGINE_API AnimationBlend2DNode final : public AnimationGraphNode
{
    HYP_OBJECT_BODY(AnimationBlend2DNode);

public:
    void GetInputs(Array<uint32>& outInputs) const override;
    void Update(AnimationGraphContext& context, uint32 nodeIndex, float weight) const override;
    void Evaluate(AnimationGraphContext& context, uint32 nodeIndex, Span<Transform> outPose) const override;

    void ComputeWeights(const Vec2f& value, Array<float>& outWeights) const;

    HYP_FIELD(Property = "Parameter", Serialize = true, Editor = true)
    Name parameter;

    HYP_FIELD(Property = "Samples", Serialize = true, Editor = true)
    Array<AnimationBlendSample> samples;
};

HYP_CLASS(NoScriptBindings)
class ENGINE_API AnimationLayerNode final : public AnimationGraphNode
{
    HYP_OBJECT_BODY(AnimationLayerNode);

public:
    void GetInputs(Array<uint32>& outInputs) const override;
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

HYP_CLASS(NoScriptBindings)
class ENGINE_API AnimationAdditiveNode final : public AnimationGraphNode
{
    HYP_OBJECT_BODY(AnimationAdditiveNode);

public:
    void GetInputs(Array<uint32>& outInputs) const override;
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

HYP_CLASS(NoScriptBindings)
class ENGINE_API AnimationChainTwistNode final : public AnimationGraphNode
{
    HYP_OBJECT_BODY(AnimationChainTwistNode);

public:
    void GetInputs(Array<uint32>& outInputs) const override;
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
    }

    HYP_FORCE_INLINE const Array<AnimationGraphParameter>& GetParameters() const
    {
        return m_parameters;
    }

    void AddParameter(Name name, AnimationGraphParameterType type, const Vec2f& defaultValue = Vec2f::Zero());

    HYP_METHOD()
    Array<Name> GetParameterNames() const;

    // the clips any node plays, so their track data can be kept resident
    Array<Handle<Animation>> CollectClips() const;

private:
    HYP_FIELD(Property = "Nodes", Serialize = true)
    Array<Handle<AnimationGraphNode>> m_nodes;

    HYP_FIELD(Property = "RootNode", Serialize = true)
    uint32 m_rootNode = 0;

    HYP_FIELD(Property = "Parameters", Serialize = true)
    Array<AnimationGraphParameter> m_parameters;
};

} // namespace Hyperion
