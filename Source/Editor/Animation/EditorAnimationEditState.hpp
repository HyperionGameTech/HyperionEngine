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

#include <Core/Containers/Map.hpp>

#include <Core/Math/Transform.hpp>

namespace Hyperion {

class Entity;
class EditorAnimationEditState;
class DebugDrawCommandList;
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

    HYP_FIELD()
    ScriptableDelegate<void> OnTimelineChanged;

private:
    void ClearTarget() override;

    bool EnterWith(const Handle<Skeleton>& skeleton, const Handle<Animation>& animation, const Handle<Entity>& sourceEntity);

    Handle<Entity> FindPreviewSource() const;
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

    float m_time = 0.0f;
    float m_speed = 1.0f;
    bool m_isPlaying = false;
    bool m_isLooping = true;

    Array<Vec3f> m_rootTrail;
    const Animation* m_rootTrailClip = nullptr;
};

} // namespace Hyperion
