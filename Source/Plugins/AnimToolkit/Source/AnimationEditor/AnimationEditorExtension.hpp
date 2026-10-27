/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Editor/EditorExtension.hpp>

namespace Hyperion {

class Entity;
class EditorAnimationEditState;

HYP_CLASS(Serialize = false)
class ANIMATIONEDITOR_API AnimationEditorExtension final : public EditorExtension
{
    HYP_OBJECT_BODY(AnimationEditorExtension);

public:
    AnimationEditorExtension();
    ~AnimationEditorExtension() override;

    HYP_METHOD()
    Handle<EditorAnimationEditState> GetEditState();

    HYP_METHOD()
    bool IsEditingAnimation() const;

    HYP_METHOD()
    bool CanEditAnimation(const Handle<Node>& node) const;

    HYP_METHOD()
    bool IsShowBonesEnabled() const;

    HYP_METHOD()
    void SetShowBonesEnabled(bool enabled);

    HYP_METHOD()
    void StartAnimationPreview(const Handle<Entity>& entity);

    HYP_METHOD()
    void StopAnimationPreview();

    HYP_METHOD()
    bool IsAnimationPreviewActive(const Handle<Entity>& entity) const;

    HYP_METHOD()
    bool IsAnimationPreviewPlaying() const;

    HYP_METHOD()
    void SetAnimationPreviewPlaying(bool playing);

    HYP_METHOD()
    float GetAnimationPreviewTime() const;

    HYP_METHOD()
    void SetAnimationPreviewTime(float time);

    HYP_METHOD()
    float GetAnimationPreviewLength() const;

    HYP_METHOD()
    float GetAnimationClipLength(const Handle<Entity>& entity) const;

    void OnAttached() override;
    void OnDetached() override;

    void Update(float delta) override;
    
    void DebugDraw(DebugDrawCommandList& debugDrawCommandList) override;
    
    bool TestPickRay(const Ray& ray, RayTestResults& outResults) override;
    
    bool SelectsPickedNode(const Node* node) const override;

private:
    void UpdateAnimationPreview(float delta);
    void DebugDrawSelectedBones(DebugDrawCommandList& debugDrawCommandList);

    Handle<EditorAnimationEditState> m_editState;

    WeakHandle<Entity> m_animationPreviewEntity;
    float m_animationPreviewTime = 0.0f;
    bool m_isAnimationPreviewPlaying = false;
};

} // namespace Hyperion
