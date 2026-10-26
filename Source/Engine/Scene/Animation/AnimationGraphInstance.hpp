/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Scene/Animation/AnimationGraph.hpp>
#include <Scene/Animation/AnimationPose.hpp>

#include <Core/Containers/Map.hpp>

#include <Core/Threading/Mutex.hpp>

namespace Hyperion {

class Skeleton;
class BoneMask;
class AnimationClipNode;
struct SkeletonBindData;

struct AnimationGraphNodeState
{
    float weight = 0.0f;
    float time = 0.0f;
    float previousTime = 0.0f;
    float smoothedValue = 0.0f;
    uint32 lastUpdateSerial = ~0u;
    bool wasActive = false;
    bool isRegistered = false;

    uint32 currentState = 0;
    uint32 previousState = InvalidAnimationGraphNode;
    uint32 stateClip = InvalidAnimationGraphNode;
    float stateTime = 0.0f;
    float transitionTime = 0.0f;
    float transitionDuration = 0.0f;
    float blendAlpha = 1.0f;
    uint32 transitionIndex = InvalidAnimationGraphNode;
};

// what a node sees during the update and pose passes
class ENGINE_API AnimationGraphContext
{
    friend class AnimationGraphInstance;

public:
    HYP_FORCE_INLINE float GetDelta() const
    {
        return m_delta;
    }

    HYP_FORCE_INLINE const SkeletonBindData& GetBindData() const
    {
        return *m_bindData;
    }

    float GetFloat(Name parameter, float fallback = 0.0f) const;
    bool GetBool(Name parameter) const;
    bool IsTriggered(Name parameter) const;
    Vec2f GetVector2(Name parameter) const;

    AnimationGraphNodeState& GetState(uint32 nodeIndex);

    // true the first time a node is reached in this update, for once-per-frame work
    bool BeginNodeUpdate(uint32 nodeIndex);

    void UpdateInput(uint32 nodeIndex, float weight);
    void EvaluateInput(uint32 nodeIndex, Span<Transform> outPose);

    void RegisterActiveClip(uint32 nodeIndex);

    const AnimationGraph& GetGraph() const;

    uint32 NumActiveClips() const;
    uint32 GetActiveClip(uint32 index) const;

    const ClipBinding& GetClipBinding(const Animation& animation);
    Span<const float> GetMaskWeights(const BoneMask& mask);

    Span<Transform> AcquireScratchPose();
    void ReleaseScratchPose();

private:
    AnimationGraphContext(AnimationGraphInstance& instance, const SkeletonBindData& bindData, float delta);

    AnimationGraphInstance* m_instance;
    const SkeletonBindData* m_bindData;
    float m_delta;
};

/*! \brief Runtime state for one entity playing an AnimationGraph: parameter values, per-node times and weights.
 *  Parameters can be set from any thread; they take effect at the next Update. */
HYP_CLASS(Serialize = false)
class ENGINE_API AnimationGraphInstance final : public ObjectBase
{
    HYP_OBJECT_BODY(AnimationGraphInstance);

    friend class AnimationGraphContext;

public:
    AnimationGraphInstance();
    explicit AnimationGraphInstance(const Handle<AnimationGraph>& graph);
    ~AnimationGraphInstance() override;

    HYP_METHOD()
    const Handle<AnimationGraph>& GetGraph() const
    {
        return m_graph;
    }

    // the graph revision this instance was built from; a newer one means the graph changed under it
    HYP_FORCE_INLINE uint32 GetGraphRevision() const
    {
        return m_graphRevision;
    }

    // takes the other instance's values for every parameter both graphs have
    void CopyParameterValues(const AnimationGraphInstance& other);

    HYP_METHOD()
    void SetFloat(Name parameter, float value);

    HYP_METHOD()
    void SetBool(Name parameter, bool value);

    HYP_METHOD()
    void SetTrigger(Name parameter);

    HYP_METHOD()
    void SetVector2(Name parameter, Vec2f value);

    HYP_METHOD()
    float GetFloat(Name parameter) const;

    HYP_METHOD()
    bool GetBool(Name parameter) const;

    HYP_METHOD()
    Vec2f GetVector2(Name parameter) const;

    // notifies the clips crossed during the last update
    HYP_METHOD()
    Array<Name> GetFiredNotifies() const;

    const AnimationGraphNodeState* GetNodeState(uint32 nodeIndex) const;

    float GetNodeWeight(uint32 nodeIndex) const;
    float GetNodeTime(uint32 nodeIndex) const;

    // the update pass: takes in parameter writes, works out weights, advances clip times and sync groups
    void Update(float delta, const Skeleton& skeleton);

    // the pose pass: builds the pose from the branches the last update left active
    void Evaluate(const Skeleton& skeleton, Span<Transform> outPose);

private:
    void SetValue(Name parameter, const Vec2f& value, bool isTrigger);
    void ValidateGraph();
    void ResetBindings(const Skeleton& skeleton);
    void AdvanceClips(float delta);
    void AdvanceClip(uint32 nodeIndex, const AnimationClipNode& clipNode, float time, bool fireNotifies);

    Handle<AnimationGraph> m_graph;
    uint32 m_graphRevision = 0;

    Map<Name, uint32> m_parameterIndices;
    Array<Vec2f> m_values;
    Array<bool> m_triggers;

    mutable Mutex m_pendingMutex;
    Array<Vec2f> m_pendingValues;
    Array<bool> m_pendingTriggers;
    Array<Name> m_publishedNotifies;

    Array<AnimationGraphNodeState> m_nodeStates;
    Array<bool> m_nodeValid;
    Array<uint32> m_activeClips;
    Array<Name> m_firedNotifies;
    uint32 m_updateSerial = 0;

    const Skeleton* m_boundSkeleton = nullptr;
    uint32 m_boundSkeletonRevision = ~0u;
    Map<const Animation*, ClipBinding> m_clipBindings;
    Map<const BoneMask*, Array<float>> m_maskWeights;

    Array<Array<Transform>> m_scratchPoses;
    uint32 m_scratchDepth = 0;
};

} // namespace Hyperion
