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
    enum class Format : UInt
    {
        MONO8,
        MONO16,
        STEREO8,
        STEREO16
    };

    enum class State : UInt
    {
        UNDEFINED,
        STOPPED,
        PLAYING,
        PAUSED
    };

    AudioSource(Format format, const ByteBuffer &byte_buffer, SizeType freq);
    ~AudioSource();

    void Init();

    Format GetFormat() const { return m_format; }
    SizeType GetFreq() const { return m_freq; }
    const ByteBuffer &GetByteBuffer() const { return m_data; }
    unsigned int GetSampleLength() const { return m_sample_length; }

    State GetState() const;

    void SetPosition(const Vector3 &vec);
    void SetVelocity(const Vector3 &vec);
    void SetPitch(float pitch);
    void SetGain(float gain);
    void SetLoop(bool loop);

    void Play();
    void Pause();
    void Stop();
    
private:
    void FindSampleLength();

    Format m_format;
    ByteBuffer m_data;
    SizeType m_freq;

    unsigned int m_buffer_id;
    unsigned int m_source_id;
    unsigned int m_sample_length;
};

} // namespace hyperion::v2

#endif
