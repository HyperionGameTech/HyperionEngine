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
    m_skeleton = nullptr;
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
            m_state = {};
        }
    }
}

void AnimationController::OnAddedToNode(Node *node)
{
    FindSkeleton(node);
}

void AnimationController::OnRemovedFromNode(Node *node)
{
    m_skeleton = nullptr;

    FindSkeletonDirect(GetOwner());
}

bool AnimationController::FindSkeleton(Node *node)
{
    if (auto &spatial = node->GetSpatial()) {
        if (FindSkeletonDirect(spatial.ptr)) {
            return true;
        }
    }

    return std::any_of(
        node->GetChildren().begin(),
        node->GetChildren().end(),
        [this](const auto &child) {
            return child != nullptr && FindSkeleton(child.get());
        }
    );
}

bool AnimationController::FindSkeletonDirect(Spatial *spatial)
{
    if (auto &skeleton = spatial->GetSkeleton()) {
        m_skeleton = skeleton.IncRef();

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

void AnimationController::Play(const std::string &animation_name, float speed, LoopMode loop_mode)
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
