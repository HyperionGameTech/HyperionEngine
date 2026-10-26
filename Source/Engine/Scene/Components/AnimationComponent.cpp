#include <ScenePch.hpp>

#include <Scene/Components/AnimationComponent.hpp>

#include <Scene/Animation/Animation.hpp>

#include <AnimationComponent.generated.inl>

namespace Hyperion {

void AnimationComponent::Play(const Handle<Animation>& clip, AnimationLoopMode loopMode, float speed)
{
    animation = clip;

    playbackState.status = clip.IsValid() ? AnimationPlaybackStatus::PLAYING : AnimationPlaybackStatus::STOPPED;
    playbackState.loopMode = loopMode;
    playbackState.speed = speed;
    playbackState.currentTime = 0.0f;
}

void AnimationComponent::Stop()
{
    playbackState.status = AnimationPlaybackStatus::STOPPED;
}

} // namespace Hyperion
