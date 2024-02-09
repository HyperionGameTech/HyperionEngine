#ifndef HYPERION_AUDIO_SOURCE_H
#define HYPERION_AUDIO_SOURCE_H

#include <core/Base.hpp>
#include <core/Containers.hpp>
#include <math/Vector3.hpp>
#include <Types.hpp>


namespace hyperion::v2 {

class Engine;

class AudioSource : public BasicObject<STUB_CLASS(AudioSource)>
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

    AudioSource(Format format, const ByteBuffer &byte_buffer, SizeType freq);
    ~AudioSource();

    void Init();

    Format GetFormat() const
        { return m_format; }

    SizeType GetFreq() const
        { return m_freq; }

    const ByteBuffer &GetByteBuffer() const
        { return m_data; }

    uint32 GetSampleLength() const
        { return m_sample_length; }

    /*! \brief Get duration in seconds. */
    double GetDuration() const
        { return double(m_sample_length) / double(m_freq); }

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

} // namespace hyperion::v2

#endif
