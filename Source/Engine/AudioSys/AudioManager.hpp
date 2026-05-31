/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/reflection/ObjectBase.hpp>

#include <Core/containers/Array.hpp>
#include <Core/containers/String.hpp>

#include <Core/math/Vector3.hpp>

#include <AL/al.h>
#include <AL/alc.h>

namespace Hyperion {

HYP_CLASS()
class ENGINE_API AudioManager : public ObjectBase
{
    HYP_OBJECT_BODY(AudioManager);

public:
    static const Handle<AudioManager>& GetInstance();

    AudioManager();

    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    ~AudioManager() override;

    HYP_FORCE_INLINE bool IsInitialized() const
    {
        return m_isInitialized;
    }

    void Initialize();
    void Shutdown();

    Array<String> ListDevices() const;

    ALCdevice* GetDevice() const
    {
        return m_device;
    }

    ALCcontext* GetContext() const
    {
        return m_context;
    }

    void SetListenerPosition(const Vec3f& position);
    void SetListenerOrientation(const Vec3f& forward, const Vec3f& up);

private:
    ALCdevice* m_device;
    ALCcontext* m_context;

    bool m_isInitialized;
};
} // namespace Hyperion
