/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Editor/EditorIsolatedEditState.hpp>

#include <Scene/Node.hpp>

#include <Scene/Animation/Animation.hpp>
#include <Scene/Animation/Skeleton.hpp>
#include <Scene/Animation/AnimationGraph.hpp>

#include <Core/Containers/Map.hpp>

#include <Core/Math/Transform.hpp>

namespace Hyperion {

class Entity;
class EditorAnimationEditState;
class DebugDrawCommandList;
class AnimationGraphInstance;
struct Ray;

HYP_CLASS(Serialize = false)
class EDITOR_API EditorBoneNode final : public Node
{
    HYP_OBJECT_BODY(EditorBoneNode);

public:
    EditorBoneNode();
    EditorBoneNode(EditorAnimationEditState* owner, Name boneName, uint32 boneIndex, Name parentBoneName, const Transform& bindTransform);
    ~EditorBoneNode() override;

    HYP_METHOD(Property = "BoneName", EditEnabled = false)
    Name GetBoneName() const
    {
        return m_boneName;
    }

    HYP_METHOD(Property = "BoneIndex", EditEnabled = false)
    uint32 GetBoneIndex() const
    {
        return m_boneIndex;
    }

    HYP_METHOD(Property = "ParentBone", EditEnabled = false)
    Name GetParentBoneName() const
    {
        return m_parentBoneName;
    }

    HYP_METHOD(Property = "BindTransform", EditEnabled = false)
    const Transform& GetBindTransform() const
    {
        return m_bindTransform;
    }

    HYP_METHOD(EditorOnly, EditorAction = "Add Socket")
    void AddSocket();

private:
    EditorAnimationEditState* m_owner = nullptr;

    Name m_boneName;
    uint32 m_boneIndex = ~0u;
    Name m_parentBoneName;
    Transform m_bindTransform;
};

HYP_CLASS(Serialize = false)
class EDITOR_API EditorSocketNode final : public Node
{
    HYP_OBJECT_BODY(EditorSocketNode);

public:
    EditorSocketNode();
    explicit EditorSocketNode(Name name);
    ~EditorSocketNode() override;
};

HYP_CLASS(Serialize = false)
class EDITOR_API EditorAnimationEditState final : public EditorIsolatedEditState
{
    HYP_OBJECT_BODY(EditorAnimationEditState);

public:
    EditorAnimationEditState();
    ~EditorAnimationEditState() override;

    bool IsActive() const override;

    HYP_METHOD()
    bool EnterSkeleton(const Handle<Skeleton>& skeleton);

    HYP_METHOD()
    bool EnterAnimation(const Handle<Animation>& animation);

    HYP_METHOD()
    bool EnterEntity(const Handle<Entity>& entity);

    void Apply() override;
    void Revert() override;

    String GetDisplayName() const override;
    String GetKindName() const override;

    HYP_METHOD()
    const Handle<Skeleton>& GetSkeleton() const;

    HYP_METHOD()
    const Handle<Animation>& GetAnimation() const;

    HYP_METHOD()
    void SetAnimation(const Handle<Animation>& animation);

    HYP_METHOD()
    bool SetAnimationByName(Name name);

    HYP_METHOD()
    Array<Name> GetClipNames() const;

    HYP_METHOD()
    const Handle<Entity>& GetPreviewEntity() const;

    HYP_METHOD()
    float GetTime() const;

    HYP_METHOD()
    void SetTime(float time);

    HYP_METHOD()
    float GetLength() const;

    HYP_METHOD()
    float GetFrameRate() const;

    HYP_METHOD()
    void StepFrames(int32 numFrames);

    HYP_METHOD()
    bool IsPlaying() const;

    HYP_METHOD()
    void SetPlaying(bool playing);

    HYP_METHOD()
    bool IsLooping() const;

    HYP_METHOD()
    void SetLooping(bool looping);

    HYP_METHOD()
    float GetSpeed() const;

    HYP_METHOD()
    void SetSpeed(float speed);

    HYP_METHOD()
    uint32 NumNotifies() const;

    HYP_METHOD()
    Name GetNotifyName(uint32 index) const;

    HYP_METHOD()
    float GetNotifyTime(uint32 index) const;

    HYP_METHOD()
    float GetNotifyDuration(uint32 index) const;

    HYP_METHOD()
    void AddNotify(float time);

    HYP_METHOD()
    void RemoveNotify(uint32 index);

    HYP_METHOD()
    void SetNotifyTime(uint32 index, float time);

    HYP_METHOD()
    void SetNotifyName(uint32 index, Name name);

    HYP_METHOD()
    void SetNotifyDuration(uint32 index, float duration);

    HYP_METHOD()
    uint32 NumSockets() const;

    void AddSocket(EditorBoneNode& bone);

    bool IsProtectedNode(const Node* node) const;

    EditorBoneNode* PickJoint(const Ray& ray, float& outDistance) const;

    void Update(float delta);

    void DebugDraw(DebugDrawCommandList& debugDrawCommandList, Span<const Handle<Node>> selectedNodes) const;

    HYP_METHOD()
    bool EnterGraph(const Handle<AnimationGraph>& graph);

    // the working copy the document edits; Apply writes it back to the asset
    HYP_METHOD()
    const Handle<AnimationGraph>& GetGraph() const;

    HYP_METHOD()
    bool IsEditingGraph() const;

    HYP_METHOD()
    Array<Name> GetGraphNodeClasses() const;

    HYP_METHOD()
    uint32 NumGraphNodes() const;

    HYP_METHOD()
    AnimationGraphNode* GetGraphNode(uint32 nodeIndex) const;

    HYP_METHOD()
    Name GetGraphNodeClass(uint32 nodeIndex) const;

    HYP_METHOD()
    Name GetGraphNodeName(uint32 nodeIndex) const;

    HYP_METHOD()
    Vec2f GetGraphNodePosition(uint32 nodeIndex) const;

    HYP_METHOD()
    uint32 GetGraphNodeInputCount(uint32 nodeIndex) const;

    HYP_METHOD()
    Name GetGraphNodeInputName(uint32 nodeIndex, uint32 slot) const;

    HYP_METHOD()
    uint32 GetGraphNodeInput(uint32 nodeIndex, uint32 slot) const;

    HYP_METHOD()
    bool CanAddGraphNodeInputs(uint32 nodeIndex) const;

    HYP_METHOD()
    uint32 GetGraphRootNode() const;

    HYP_METHOD()
    uint32 AddGraphNode(Name className, Vec2f position);

    HYP_METHOD()
    uint32 AddGraphClipNode(const Handle<Animation>& clip, Vec2f position);

    HYP_METHOD()
    void RemoveGraphNode(uint32 nodeIndex);

    // slot at or past the node's input count adds an input, for nodes that take any number
    HYP_METHOD()
    void ConnectGraphNodes(uint32 fromNode, uint32 toNode, uint32 slot);

    HYP_METHOD()
    void DisconnectGraphInput(uint32 nodeIndex, uint32 slot);

    HYP_METHOD()
    void SetGraphRootNode(uint32 nodeIndex);

    HYP_METHOD()
    void MoveGraphNode(uint32 nodeIndex, Vec2f position);

    HYP_METHOD()
    void SetBlendSamplePosition(uint32 nodeIndex, uint32 sample, Vec2f position);

    HYP_METHOD()
    void RenameGraphState(uint32 nodeIndex, uint32 stateIndex, Name name);

    HYP_METHOD()
    uint32 AddGraphTransition(uint32 nodeIndex, uint32 from, bool fromAnyState, uint32 to, float duration);

    HYP_METHOD()
    void AddGraphTransitionCondition(uint32 nodeIndex, uint32 transitionIndex, Name parameter, uint32 op, float value);

    HYP_METHOD()
    Vec2f GetBlendSamplePosition(uint32 nodeIndex, uint32 sample) const;

    HYP_METHOD()
    Name GetBlendParameter(uint32 nodeIndex) const;

    HYP_METHOD()
    Array<Name> GetGraphParameterNames() const;

    HYP_METHOD()
    uint32 GetGraphParameterType(Name parameter) const;

    HYP_METHOD()
    void AddGraphParameter(Name parameter, uint32 type);

    HYP_METHOD()
    void RemoveGraphParameter(Name parameter);

    // records whatever was written straight to the working graph (e.g. by the property panel) as one undoable edit
    HYP_METHOD()
    void CommitGraphEdit(const String& actionName);

    HYP_METHOD()
    void SetPreviewParameter(Name parameter, Vec2f value);

    HYP_METHOD()
    void TriggerPreviewParameter(Name parameter);

    HYP_METHOD()
    Vec2f GetPreviewParameter(Name parameter) const;

    HYP_METHOD()
    float GetPreviewNodeWeight(uint32 nodeIndex) const;

    // a state machine's current state and any running transition, empty for other nodes
    HYP_METHOD()
    String GetPreviewNodeStatus(uint32 nodeIndex) const;

    AnimationGraphInstance* GetPreviewGraphInstance() const;

    HYP_FIELD()
    ScriptableDelegate<void> OnTimelineChanged;

    HYP_FIELD()
    ScriptableDelegate<void> OnGraphChanged;

private:
    void ClearTarget() override;

    bool EnterWith(const Handle<Skeleton>& skeleton, const Handle<Animation>& animation, const Handle<Entity>& sourceEntity);

    void SetUpGraph(const Handle<AnimationGraph>& sourceGraph);
    void RestoreGraph(const String& text);

    void BuildWorkingRoot();

    void ApplyPreviewClip();
    void UpdateBoneMirrors();
    void UpdateRootTrail();

    float GetJointRadius() const;

    Array<SkeletonSocket> CollectSockets() const;

    const Array<AnimationNotify>& GetWorkingNotifies() const;
    void PushNotifyEdit(const String& actionName, Array<AnimationNotify> notifies);

    Handle<Skeleton> m_skeleton;
    Handle<Animation> m_animation;
    WeakHandle<Entity> m_sourceEntity;

    Handle<Node> m_workingRoot;
    Handle<Entity> m_previewEntity;
    Array<Handle<EditorBoneNode>> m_boneNodes;

    Map<Handle<Animation>, Array<AnimationNotify>> m_workingNotifies;

    Handle<AnimationGraph> m_sourceGraph;
    Handle<AnimationGraph> m_graph;
    String m_graphSnapshot;

    float m_time = 0.0f;
    float m_speed = 1.0f;
    bool m_isPlaying = false;
    bool m_isLooping = true;

    Array<Vec3f> m_rootTrail;
    const Animation* m_rootTrailClip = nullptr;
};

} // namespace Hyperion
