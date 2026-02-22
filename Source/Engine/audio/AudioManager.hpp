/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/reflection/ObjectBase.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/String.hpp>

#include <core/math/Vector3.hpp>

#include <AL/al.h>
#include <AL/alc.h>

namespace Hyperion {

HYP_CLASS()
class HYP_API AudioManager : public ObjectBase
{
    HYP_OBJECT_BODY(AudioManager);

public:
    static const Handle<AudioManager>& GetInstance();

    AudioManager();
    ~AudioManager() override;

    void Init() override;

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
};
} // namespace Hyperion
