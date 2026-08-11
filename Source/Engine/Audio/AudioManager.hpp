/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Reflection/ObjectBase.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/String.hpp>

#include <Core/Math/Vector3.hpp>

#if defined(HYP_OPENAL) && HYP_OPENAL
#include <Audio/OpenAL/OpenALAudioAdapter.hpp>
#else
#include <Audio/Null/NullAudioAdapter.hpp>
#endif

namespace Hyperion {

#if defined(HYP_OPENAL) && HYP_OPENAL
using AudioAdapterImpl = OpenALAudioAdapter;
#else
using AudioAdapterImpl = NullAudioAdapter;
#endif

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

    HYP_FORCE_INLINE AudioAdapterImpl& GetAdapter()
    {
        return m_adapter;
    }

    HYP_FORCE_INLINE const AudioAdapterImpl& GetAdapter() const
    {
        return m_adapter;
    }

    void Initialize();
    void Shutdown();

    Array<String> ListDevices() const;

    void SetListenerPosition(const Vec3f& position);
    void SetListenerOrientation(const Vec3f& forward, const Vec3f& up);

private:
    AudioAdapterImpl m_adapter;

    bool m_isInitialized;
};
} // namespace Hyperion
