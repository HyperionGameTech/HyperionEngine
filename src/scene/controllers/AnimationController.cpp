#include "AnimationController.hpp"
#include <set>
#include <iterator>

namespace hyperion::v2 {

AnimationController::AnimationController()
    : PlaybackController("AnimationController")
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
    if (IsPlaying()) {
        if (auto *animation = GetCurrentAnimation()) {
            m_state.current_time += delta * m_state.speed;

            if (m_state.current_time > animation->GetLength()) {
                m_state.current_time = 0.0f;

                if (m_state.loop_mode == LoopMode::ONCE) {
                    SetPlaybackState(PlaybackState::STOPPED);
                }
            }

            animation->ApplyBlended(m_state.current_time, 0.5f);
        } else {
            m_state = { };
        }
    }
}

void AnimationController::OnAddedToNode(Node *node)
{
    FindSkeleton(node);
}

void AnimationController::OnRemovedFromNode(Node *node)
{
    m_skeleton.Reset();

    FindSkeletonDirect(GetOwner());
}

bool AnimationController::FindSkeleton(Node *node)
{
    if (node == nullptr) {
        return false;
    }

    if (auto &entity = node->GetEntity()) {
        if (FindSkeletonDirect(entity.Get())) {
            return true;
        }
    }

    return std::any_of(
        node->GetChildren().begin(),
        node->GetChildren().end(),
        [this](auto &child) {
            return FindSkeleton(child.Get());
        }
    );
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

    if (!MathUtil::InRange(m_animation_index, {0u, static_cast<UInt>(m_skeleton->NumAnimations())})) {
        m_animation_index = 0;
    }
    
    PlaybackController::Play(speed, loop_mode);
}

void AnimationController::Play(const String &animation_name, float speed, LoopMode loop_mode)
{
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
