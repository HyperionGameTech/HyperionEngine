#ifndef HYPERION_V2_ANIMATION_CONTROLLER_H
#define HYPERION_V2_ANIMATION_CONTROLLER_H

#include "PlaybackController.hpp"
#include "../../animation/Bone.hpp"
#include "../../animation/Animation.hpp"

#include <Types.hpp>

namespace hyperion::v2 {

class AnimationController : public PlaybackController
{
public:
    AnimationController();
    virtual ~AnimationController() override = default;

    void Play(const String &animation_name, float speed, LoopMode loop_mode = LoopMode::ONCE);
    virtual void Play(float speed, LoopMode loop_mode = LoopMode::ONCE) override;
    virtual void Stop() override;

    Animation *GetCurrentAnimation() const
    {
        return m_skeleton != nullptr && m_animation_index != ~0u
            ? m_skeleton->GetAnimation(m_animation_index)
            : nullptr;
    }

    virtual void OnAdded() override;
    virtual void OnRemoved() override;
    virtual void OnUpdate(GameCounter::TickUnit delta) override;

    virtual void OnAddedToNode(Node *node) override;
    virtual void OnRemovedFromNode(Node *node) override;

    // virtual void OnDescendentAdded(Node *) override {} /* TODO: check for bones, add them */
    // virtual void OnDescendentRemoved(Node *node) override {} /* TODO: remove any bones */

private:
    bool FindSkeleton(Node *node);
    bool FindSkeletonDirect(Entity *entity);
    
    UInt m_animation_index = ~0u;

    Handle<Skeleton> m_skeleton;
};

} // namespace hyperion::v2

#endif
