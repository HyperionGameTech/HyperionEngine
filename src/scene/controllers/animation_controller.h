#ifndef HYPERION_V2_ANIMATION_CONTROLLER_H
#define HYPERION_V2_ANIMATION_CONTROLLER_H

#include "playback_controller.h"
#include "../../animation/bone.h"
#include "../../animation/animation.h"

#include <types.h>

namespace hyperion::v2 {

class AnimationController : public PlaybackController {
public:
    AnimationController();
    virtual ~AnimationController() override = default;

    void Play(const std::string &animation_name, float speed, LoopMode loop_mode = LoopMode::ONCE);
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

    virtual void OnDescendentAdded(Node *) override {} /* TODO: check for bones, add them */
    virtual void OnDescendentRemoved(Node *node) override {} /* TODO: remove any bones */

private:
    bool FindSkeleton(Node *node);
    
    UInt m_animation_index = ~0u;

    Ref<Skeleton> m_skeleton;
};

} // namespace hyperion::v2

#endif
