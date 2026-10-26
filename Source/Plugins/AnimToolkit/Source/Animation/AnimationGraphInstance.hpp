/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Animation/AnimationGraph.hpp>
#include <Animation/AnimationPose.hpp>

#include <Core/Containers/Map.hpp>

#include <Core/Memory/UniquePtr.hpp>

#include <Core/Resource/ResLock.hpp>

#include <Core/Threading/Mutex.hpp>

namespace Hyperion {

class Skeleton;
class BoneMask;
class AnimationClipNode;
class AnimationGraphInstance;
struct SkeletonBindData;

struct AnimationGraphNodeState
{
    float weight = 0.0f;
    float rootMotionWeight = 0.0f;
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
    bool isInertializing = false;
    bool isInertializePending = false;
};

// a state machine's last output and, while it inertializes, how far that was from the new state's first pose
struct AnimationInertialization
{
    Array<Transform> lastPose;
    Array<Quat4f> rotationOffsets;
    Array<Vec3f> translationOffsets;
};

struct AnimationSlotPlayback
{
    Handle<Animation> clip;
    Array<UniquePtr<TSharedResLock<AssetObject>>> trackLocks;
    float time = 0.0f;
    float previousTime = 0.0f;
    float playRate = 1.0f;
    float blendTime = 0.2f;
    float weight = 0.0f;
    bool isStopping = false;
};

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

    const AnimationSlotPlayback* GetSlotPlayback(Name slot) const;

    AnimationInertialization& GetInertialization(uint32 nodeIndex);

    // eases out the gap left by a jump the node started (isInertializePending), and keeps its last pose for the next one
    void ApplyInertialization(uint32 nodeIndex, Span<Transform> outPose);

    void AddRootMotion(const Vec3f& motion, float weight);

    float GetRootMotionScale() const;
    void SetRootMotionScale(float scale);

    const Skeleton& GetSkeleton() const;

    const Vec3f& GetModelUp() const;

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

    HYP_FORCE_INLINE uint32 GetGraphRevision() const
    {
        return m_graphRevision;
    }

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

    /// plays a one-shot clip in the Slot nodes named slot, blending in, and back out before it ends
    HYP_METHOD()
    void PlaySlot(Name slot, const Handle<Animation>& clip, float blendTime = 0.2f, float playRate = 1.0f);

    HYP_METHOD()
    void StopSlot(Name slot, float blendTime = 0.2f);

    HYP_METHOD()
    bool IsSlotPlaying(Name slot) const;

    /// how far root motion clips moved the character during the last update, in model space
    HYP_METHOD()
    Vec3f GetRootMotion() const;

    /// how much of the pose came from root motion clips during the last update, 0 to 1
    HYP_METHOD()
    float GetRootMotionWeight() const;

    /// \return the notified clips crossed during the last update
    HYP_METHOD()
    Array<Name> GetFiredNotifies() const;

    const AnimationGraphNodeState* GetNodeState(uint32 nodeIndex) const;

    float GetNodeWeight(uint32 nodeIndex) const;
    float GetNodeTime(uint32 nodeIndex) const;

    HYP_METHOD()
    void SetDebugCapture(bool capture);

    HYP_METHOD()
    bool IsDebugCaptured() const;

    HYP_METHOD()
    float GetDebugNodeWeight(uint32 nodeIndex) const;

    HYP_METHOD()
    String GetDebugNodeStatus(uint32 nodeIndex) const;

    HYP_METHOD()
    uint32 GetDebugCurrentState(uint32 nodeIndex) const;

    HYP_METHOD()
    uint32 GetDebugPreviousState(uint32 nodeIndex) const;

    HYP_METHOD()
    float GetDebugBlendAlpha(uint32 nodeIndex) const;

    HYP_METHOD()
    uint32 GetDebugTransition(uint32 nodeIndex) const;

    HYP_METHOD()
    float GetDebugStateProgress(uint32 nodeIndex) const;

    HYP_METHOD()
    Vec2f GetDebugValue(Name parameter) const;

    void Update(float delta, const Skeleton& skeleton);
    void Evaluate(const Skeleton& skeleton, Span<Transform> outPose);

private:
    void SetValue(Name parameter, const Vec2f& value, bool isTrigger);
   
    void ValidateGraph();
   
    void ResetBindings(const Skeleton& skeleton);
   
    void AdvanceClips(AnimationGraphContext& context, float delta);
   
    float GetPlayRate(const AnimationClipNode& clipNode) const;
   
    float GetSyncGroupCycleRate(const Array<uint32>& members) const;
   
    void AdvanceClip(AnimationGraphContext& context, uint32 nodeIndex, const AnimationClipNode& clipNode, float time, bool wrapped, bool fireNotifies);
   
    void ApplySlotRequests();
   
    void AdvanceSlots(float delta);
   
    void FireNotifies(const Animation& clip, float previousTime, float time);

    Handle<AnimationGraph> m_graph;
    uint32 m_graphRevision = 0;

    Map<Name, uint32> m_parameterIndices;
    Array<Vec2f> m_values;
    Array<bool> m_triggers;

    mutable Mutex m_pendingMutex;
    Array<Vec2f> m_pendingValues;
    Array<bool> m_pendingTriggers;
    Array<Name> m_publishedNotifies;

    Array<Pair<Name, AnimationSlotPlayback>> m_pendingSlotPlays;
    Array<Pair<Name, float>> m_pendingSlotStops;
    Array<Name> m_publishedSlots;
    Map<Name, AnimationSlotPlayback> m_slots;

    Map<uint32, AnimationInertialization> m_inertialization;

    Vec3f m_rootMotion;
    Vec3f m_publishedRootMotion;
    float m_rootMotionWeight = 0.0f;
    float m_rootMotionScale = 1.0f;
    float m_publishedRootMotionWeight = 0.0f;
    Vec3f m_modelUp = Vec3f::UnitY();

    bool m_isDebugCaptured = false;
    Array<AnimationGraphNodeState> m_debugNodeStates;
    Array<Vec2f> m_debugValues;

    Array<AnimationGraphNodeState> m_nodeStates;
    Array<bool> m_nodeValid;
    Array<uint32> m_activeClips;
    Array<Name> m_firedNotifies;
    uint32 m_updateSerial = 0;

    WeakHandle<Skeleton> m_boundSkeleton;
    uint32 m_boundSkeletonRevision = ~0u;
    Map<WeakHandle<Animation>, ClipBinding> m_clipBindings;
    Map<WeakHandle<BoneMask>, Pair<uint32, Array<float>>> m_maskWeights;

    Array<Array<Transform>> m_scratchPoses;
    uint32 m_scratchDepth = 0;
};

} // namespace Hyperion
