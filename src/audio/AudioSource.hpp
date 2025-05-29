/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_AUDIO_SOURCE_HPP
#define HYPERION_AUDIO_SOURCE_HPP

#include <core/Base.hpp>

#include <core/object/HypObject.hpp>

#include <core/math/Vector3.hpp>

#include <Types.hpp>

namespace hyperion {

class Engine;

enum class AudioSourceFormat : uint32
{
    MONO8,
    MONO16,
    STEREO8,
    STEREO16
};

enum class AudioSourceState : uint32
{
    UNDEFINED,
    STOPPED,
    PLAYING,
    PAUSED
};

HYP_CLASS()
class AudioSource : public HypObject<AudioSource>
{
    HYP_OBJECT_BODY(AudioSource);

public:
    AudioSource();
    AudioSource(AudioSourceFormat format, const ByteBuffer& byte_buffer, uint64 freq);

    AudioSource(const AudioSource& other) = delete;
    AudioSource& operator=(const AudioSource& other) = delete;

    AudioSource(AudioSource&& other) noexcept;
    AudioSource& operator=(AudioSource&& other) noexcept;

    ~AudioSource();

    void Init();

    HYP_METHOD(Property = "Format", Serialize = true, Editor = true)
    HYP_FORCE_INLINE AudioSourceFormat GetFormat() const
    {
        return m_format;
    }

    HYP_METHOD(Property = "Format", Serialize = true, Editor = true)
    HYP_FORCE_INLINE void SetFormat(AudioSourceFormat format)
    {
        m_format = format;
    }

    HYP_METHOD(Property = "Freq", Serialize = true, Editor = true)
    HYP_FORCE_INLINE uint64 GetFreq() const
    {
        return m_freq;
    }

    HYP_METHOD(Property = "Freq", Serialize = true, Editor = true)
    HYP_FORCE_INLINE void SetFreq(uint64 freq)
    {
        m_freq = freq;
    }

    HYP_METHOD(Property = "Data", Serialize = true, Editor = true)
    HYP_FORCE_INLINE const ByteBuffer& GetData() const
    {
        return m_data;
    }

    HYP_METHOD(Property = "Data", Serialize = true, Editor = true)
    HYP_FORCE_INLINE void SetData(const ByteBuffer& data)
    {
        m_data = data;
    }

    HYP_METHOD(Property = "SampleLength", Serialize = true, Editor = true)
    HYP_FORCE_INLINE uint32 GetSampleLength() const
    {
        return m_sample_length;
    }

    HYP_METHOD(Property = "SampleLength", Serialize = true, Editor = true)
    HYP_FORCE_INLINE void SetSampleLength(uint32 sample_length)
    {
        m_sample_length = sample_length;
    }

    /*! \brief Get duration in seconds. */
    HYP_METHOD(Property = "Duration", Serialize = true, Editor = true)
    HYP_FORCE_INLINE double GetDuration() const
    {
        return double(m_sample_length) / double(m_freq);
    }

    HYP_METHOD(Property = "Duration", Serialize = true, Editor = true)
    HYP_FORCE_INLINE void SetDuration(double duration)
    {
        m_sample_length = uint32(duration * double(m_freq));
    }

    AudioSourceState GetState() const;

    void SetPosition(const Vec3f& vec);
    void SetVelocity(const Vec3f& vec);
    void SetPitch(float pitch);
    void SetGain(float gain);
    void SetLoop(bool loop);

    void Play();
    void Pause();
    void Stop();

private:
    void FindSampleLength();

    AudioSourceFormat m_format;
    ByteBuffer m_data;
    uint64 m_freq;

    uint32 m_buffer_id;
    uint32 m_source_id;
    uint32 m_sample_length;
};

} // namespace hyperion

#endif
