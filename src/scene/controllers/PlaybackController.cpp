#include "PlaybackController.hpp"

namespace hyperion::v2 {

PlaybackController::PlaybackController(const char *name)
    : Controller(name),
      m_state{}
{
}

void PlaybackController::Play(float speed, LoopMode loop_mode)
{
    m_state.speed     = speed;
    m_state.loop_mode = loop_mode;

    SetPlaybackState(PlaybackState::PLAYING);
}

void PlaybackController::Pause()
{
    SetPlaybackState(PlaybackState::PAUSED);    
}

void PlaybackController::Stop()
{
    SetPlaybackState(PlaybackState::STOPPED);
}

} // namespace hyperion::v2