#ifndef HYPERION_V2_ANIMATION_CONTROLLER_H
#define HYPERION_V2_ANIMATION_CONTROLLER_H

#include "PlaybackController.hpp"
#include <scene/animation/Bone.hpp>
#include <scene/animation/Animation.hpp>

#include <Types.hpp>

namespace hyperion::v2 {

class AnimationController : public PlaybackController
{
public:
    static constexpr const char *controller_name = "AnimationController";

    AnimationController();
    virtual ~AnimationController() override = default;

    void Play(const String &animation_name, float speed, LoopMode loop_mode = LoopMode::ONCE);
    virtual void Play(float speed, LoopMode loop_mode = LoopMode::ONCE) override;
    virtual void Stop() override;

    UInt GetCurrentAnimationIndex() const
        { return m_animation_index; }

    const Animation *GetCurrentAnimation() const
    {
        return m_skeleton && m_animation_index != ~0u
            ? &m_skeleton->GetAnimation(m_animation_index)
            : nullptr;
    }

    virtual void OnAdded() override;
    virtual void OnRemoved() override;
    virtual void OnUpdate(GameCounter::TickUnit delta) override;

    virtual void OnAttachedToNode(Node *node) override;
    virtual void OnDetachedFromNode(Node *node) override;

private:
    bool FindSkeleton(Node *node);
    bool FindSkeletonDirect(Entity *entity);
    
    UInt m_animation_index = ~0u;

    Handle<Skeleton> m_skeleton;
};

} // namespace hyperion::v2

#endif
