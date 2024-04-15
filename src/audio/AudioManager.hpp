/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_AUDIO_MANAGER_H
#define HYPERION_AUDIO_MANAGER_H

#include <AL/al.h>
#include <AL/alc.h>

#include <core/lib/DynArray.hpp>
#include <core/lib/String.hpp>

#include <math/Vector3.hpp>

namespace hyperion {
class AudioManager {
public:
  static AudioManager *GetInstance();
  static void Deinitialize();

  AudioManager();
  ~AudioManager();

  bool Initialize();

  bool IsInitialized() const
    { return m_is_initialized; }

  Array<String> ListDevices() const;

  ALCdevice *GetDevice() const { return m_device; }

  ALCcontext *GetContext() const { return m_context; }

  void SetListenerPosition(const Vec3f &position);
  void SetListenerOrientation(const Vec3f &forward, const Vec3f &up);

private:
  static AudioManager *instance;

  bool          m_is_initialized;

  ALCdevice     *m_device;
  ALCcontext    *m_context;
};
} // namespace hyperion

#endif
