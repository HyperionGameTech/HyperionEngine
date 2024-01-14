#include "AnimationController.hpp"

namespace hyperion::v2 {

AnimationController::AnimationController()
    : PlaybackController()
{
}

AnimationController::AnimationController(Handle<Skeleton> skeleton)
    : PlaybackController(),
      m_skeleton(std::move(skeleton))
{
}

void AnimationController::OnAdded()
{
}

void AnimationController::OnRemoved()
{
    m_skeleton.Reset();
}

void AnimationController::OnUpdate(GameCounter::TickUnit delta)
{
    if (!m_skeleton) {
        m_state = { };

        return;
    }

    if (IsPlaying()) {
        if (m_skeleton && m_animation_index != ~0u) {
            Animation &animation = m_skeleton->GetAnimation(m_animation_index);

            m_state.current_time += delta * m_state.speed;

            if (m_state.current_time > animation.GetLength()) {
                m_state.current_time = 0.0f;

                if (m_state.loop_mode == LoopMode::ONCE) {
                    SetPlaybackState(PlaybackState::STOPPED);
                }
            }
            
            animation.ApplyBlended(m_state.current_time, 0.5f);
        } else {
            m_state = { };
        }
    }
}

void AnimationController::OnAttachedToNode(Node *node)
{
    // FindSkeleton(node);
}

void AnimationController::OnDetachedFromNode(Node *node)
{
    m_skeleton.Reset();

    // FindSkeletonDirect(GetOwner());
}

Bool AnimationController::FindSkeleton(Node *node)
{
    if (node == nullptr) {
        return false;
    }

    // @TODO: Remove this; will be converted to ECS system

    // if (auto &entity = node->GetEntity()) {
    //     if (FindSkeletonDirect(entity.Get())) {
    //         return true;
    //     }
    // }

    // for (auto &it : node->GetChildren()) {
    //     if (FindSkeleton(it.Get())) {
    //         return true;
    //     }
    // }

    return false;
}

bool AnimationController::FindSkeletonDirect(Entity *entity)
{
    if (auto &skeleton = entity->GetSkeleton()) {
        m_skeleton = skeleton;

        return true;
    }

    return false;
}

void AnimationController::Play(float speed, LoopMode loop_mode)
{
    if (m_skeleton == nullptr || m_skeleton->NumAnimations() == 0) {
        Stop();

        return;
    }

    if (!MathUtil::InRange(m_animation_index, { 0u, UInt(m_skeleton->NumAnimations()) })) {
        m_animation_index = 0;
    }
    
    PlaybackController::Play(speed, loop_mode);
}

void AnimationController::Play(const String &animation_name, float speed, LoopMode loop_mode)
{
    m_animation_index = ~0u;

    if (m_skeleton == nullptr || m_skeleton->NumAnimations() == 0) {
        return;
    }
    
    if (m_skeleton->FindAnimation(animation_name, &m_animation_index) == nullptr) {
        Stop();

        return;
    }
    
    PlaybackController::Play(speed, loop_mode);
}

void AnimationController::Stop()
{
    PlaybackController::Stop();

    m_animation_index = ~0u;
}

} // namespace hyperion::v2
