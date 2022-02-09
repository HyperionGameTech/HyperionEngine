#ifndef AUDIO_CONTROL_H
#define AUDIO_CONTROL_H

#include "../control.h"
#include "audio_source.h"

#include <memory>

namespace hyperion {
class AudioControl : public EntityControl {
public:
    AudioControl(std::shared_ptr<AudioSource> source);

    inline std::shared_ptr<AudioSource> GetSource() { return m_source; }
    inline void SetSource(std::shared_ptr<AudioSource> source) { m_source = source; }

    void OnAdded();
    void OnRemoved();
    void OnUpdate(double dt);

private:
    virtual std::shared_ptr<EntityControl> CloneImpl() override;

    std::shared_ptr<AudioSource> m_source;
    Vector3 m_last_position;
};
} // namespace hyperion

#endif
