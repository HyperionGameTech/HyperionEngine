/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_AUDIO_SOURCE_HPP
#define HYPERION_AUDIO_SOURCE_HPP

#include <core/Base.hpp>
#include <math/Vector3.hpp>

#include <Types.hpp>

namespace hyperion {

class Engine;

class AudioSource : public BasicObject<AudioSource>
{
public:
    enum class Format : uint
    {
        MONO8,
        MONO16,
        STEREO8,
        STEREO16
    };

    enum class State : uint
    {
        UNDEFINED,
        STOPPED,
        PLAYING,
        PAUSED
    };

    AudioSource();
    AudioSource(Format format, const ByteBuffer &byte_buffer, SizeType freq);

    AudioSource(const AudioSource &other)               = delete;
    AudioSource &operator=(const AudioSource &other)    = delete;

    AudioSource(AudioSource &&other) noexcept;
    AudioSource &operator=(AudioSource &&other) noexcept;

    ~AudioSource();

    void Init();

    HYP_NODISCARD HYP_FORCE_INLINE
    Format GetFormat() const
        { return m_format; }

    HYP_NODISCARD HYP_FORCE_INLINE
    SizeType GetFreq() const
        { return m_freq; }

    HYP_NODISCARD HYP_FORCE_INLINE
    const ByteBuffer &GetByteBuffer() const
        { return m_data; }

    HYP_NODISCARD HYP_FORCE_INLINE
    uint32 GetSampleLength() const
        { return m_sample_length; }

    /*! \brief Get duration in seconds. */
    HYP_NODISCARD HYP_FORCE_INLINE
    double GetDuration() const
        { return double(m_sample_length) / double(m_freq); }

    HYP_NODISCARD
    State GetState() const;

    void SetPosition(const Vec3f &vec);
    void SetVelocity(const Vec3f &vec);
    void SetPitch(float pitch);
    void SetGain(float gain);
    void SetLoop(bool loop);

    void Play();
    void Pause();
    void Stop();
    
private:
    void FindSampleLength();

    Format      m_format;
    ByteBuffer  m_data;
    SizeType    m_freq;

    uint32      m_buffer_id;
    uint32      m_source_id;
    uint32      m_sample_length;
};

} // namespace hyperion

#endif
