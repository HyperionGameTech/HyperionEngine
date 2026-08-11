/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Core/Memory/SharedPtr.hpp>

#include <Core/Functional/Proc.hpp>

#include <Core/Math/Vector3.hpp>

#include <Core/Types.hpp>

#include <Scripting/ScriptableDelegate.hpp>

namespace Hyperion {

class Sound;

HYP_ENUM()
enum class AudioSourceState : uint32
{
    UNDEFINED,
    STOPPED,
    PLAYING,
    PAUSED
};

HYP_CLASS()
class ENGINE_API AudioSource final : public ObjectBase
{
    HYP_OBJECT_BODY(AudioSource);

public:
    AudioSource();

    AudioSource(const AudioSource& other) = delete;
    AudioSource& operator=(const AudioSource& other) = delete;

    AudioSource(AudioSource&& other) noexcept;
    AudioSource& operator=(AudioSource&& other) noexcept;

    ~AudioSource();

    HYP_METHOD(Property = "Sound", Serialize, Editor)
    HYP_FORCE_INLINE const Handle<Sound>& GetSound() const
    {
        return m_sound;
    }

    HYP_METHOD(Property = "Sound", Serialize, Editor)
    void SetSound(const Handle<Sound>& sound);

    AudioSourceState GetState() const;

    void SetPosition(const Vec3f& vec);
    void SetVelocity(const Vec3f& vec);
    void SetPitch(float pitch);
    void SetGain(float gain);
    void SetLoop(bool loop);

    void Play();
    void Pause();
    void Stop();

    /*! \brief Opaque handle owned by the active AudioAdapter */
    HYP_FORCE_INLINE void* GetInternalData() const
    {
        return m_internalData.Get();
    }

    HYP_FORCE_INLINE void SetInternalData(SharedPtr<void>&& internalData)
    {
        m_internalData = std::move(internalData);
    }

    /*! \brief Set a callback to be invoked whenever a property on this AudioSource changes */
    HYP_FORCE_INLINE void SetOnChanged(Proc<void()>&& onChanged)
    {
        m_onChanged = std::move(onChanged);
    }

    /*! \brief Fired whenever a property on this AudioSource changes (eg. SetSound, SetPosition, ...),
     *  including changes made out-of-band (eg. by the deserializer, not through script/UI code).
     *  Bind per-instance via OnChanged.Bind(this, ...); mirrors Node::TransformUpdated. Used by the
     *  editor to know when to refresh a cached property view for this object. */
    HYP_FIELD()
    static ScriptableDelegate<void, AudioSource*> OnChanged;

private:
    void Init() override;

    HYP_FORCE_INLINE void NotifyChanged()
    {
        if (m_onChanged)
        {
            m_onChanged();
        }

        OnChanged.Fire(this, this);
    }

    HYP_FIELD(Property = "Sound", Serialize, Editor)
    Handle<Sound> m_sound;

    SharedPtr<void> m_internalData;

    Proc<void()> m_onChanged;
};

} // namespace Hyperion
