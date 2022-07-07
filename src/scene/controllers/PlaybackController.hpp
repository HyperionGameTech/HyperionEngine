#ifndef HYPERION_V2_PLAYBACK_CONTROLLER_H
#define HYPERION_V2_PLAYBACK_CONTROLLER_H

#include "../Controller.hpp"

namespace hyperion::v2 {

enum class PlaybackState {
    STOPPED,
    PAUSED,
    PLAYING
};

enum class LoopMode {
    ONCE,
    REPEAT
};

class PlaybackController : public Controller {
public:
    PlaybackController(const char *name);
    virtual ~PlaybackController() override = default;

    bool IsPlaying() const { return m_state.playback_state == PlaybackState::PLAYING; }
    bool IsPaused() const  { return m_state.playback_state == PlaybackState::PAUSED; }
    bool IsStopped() const { return m_state.playback_state == PlaybackState::STOPPED; }
    
    virtual void OnAdded() override = 0;
    virtual void OnRemoved() override = 0;
    virtual void OnUpdate(GameCounter::TickUnit delta) override = 0;
    
    void Play(LoopMode loop_mode = LoopMode::ONCE) { Play(m_state.speed, loop_mode); }
    virtual void Play(float speed, LoopMode loop_mode = LoopMode::ONCE);
    virtual void Pause();
    virtual void Stop();

    LoopMode GetLoopMode() const         { return m_state.loop_mode; }
    void SetLoopMode(LoopMode loop_mode) { m_state.loop_mode = loop_mode; }

protected:
    PlaybackState GetPlaybackState() const { return m_state.playback_state; }

    void SetPlaybackState(PlaybackState playback_state)
    {
        m_state.playback_state = playback_state;

        if (playback_state == PlaybackState::STOPPED) {
            m_state = {};
        }
    }

    struct {
        PlaybackState playback_state = PlaybackState::STOPPED;
        LoopMode loop_mode           = LoopMode::ONCE;
        float  speed                 = 1.0f;
        float  current_time          = 0.0f;
    } m_state;
};

} // namespace hyperion::v2

#endif
