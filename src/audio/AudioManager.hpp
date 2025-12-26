/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <AL/al.h>
#include <AL/alc.h>

#include <core/containers/Array.hpp>
#include <core/containers/String.hpp>

#include <core/math/Vector3.hpp>

namespace Hyperion {
class AudioManager
{
public:
    static AudioManager& GetInstance();

    AudioManager();
    ~AudioManager();

    bool Initialize();
    void Shutdown();

    bool IsInitialized() const
    {
        return m_isInitialized;
    }

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
    static AudioManager* s_instance;

    bool m_isInitialized;

    ALCdevice* m_device;
    ALCcontext* m_context;
};
} // namespace Hyperion
