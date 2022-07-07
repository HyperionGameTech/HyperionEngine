#ifndef HYPERION_AUDIO_MANAGER_H
#define HYPERION_AUDIO_MANAGER_H

#include <AL/al.h>
#include <AL/alc.h>

#include "../math/Vector3.hpp"

namespace hyperion {
class AudioManager {
public:
    static AudioManager *GetInstance();
    static void Deinitialize();

    AudioManager();
    ~AudioManager();

    bool Initialize();
    inline bool IsInitialized() const { return m_is_initialized; }
    void ListDevices();
    inline ALCdevice *GetDevice() const { return m_device; }
    inline ALCcontext *GetContext() const { return m_context; }

    void SetListenerPosition(const Vector3 &position);
    void SetListenerOrientation(const Vector3 &forward, const Vector3 &up);

private:
    static AudioManager *instance;

    bool m_is_initialized;

    ALCdevice *m_device;
    ALCcontext *m_context;
};
} // namespace hyperion

#endif
