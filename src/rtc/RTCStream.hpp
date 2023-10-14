#ifndef HYPERION_V2_RTC_STREAM_HPP
#define HYPERION_V2_RTC_STREAM_HPP

#include <core/lib/String.hpp>
#include <core/lib/ByteBuffer.hpp>
#include <core/lib/UniquePtr.hpp>

namespace hyperion::v2 {

class TaskThread;

class RTCStreamEncoder;
class RTCTrack;

enum RTCStreamType
{
    RTC_STREAM_TYPE_UNKNOWN = 0,
    RTC_STREAM_TYPE_AUDIO,
    RTC_STREAM_TYPE_VIDEO
};

struct RTCStreamDestination
{
    Array<RC<RTCTrack>> tracks;
};

struct RTCStreamParams
{
    UInt samples_per_second = 60;

    UInt GetSampleDuration() const
    {
        return 1000 * 1000 / samples_per_second;
    }
};

class RTCStream
{
public:
    RTCStream(RTCStreamType stream_type, UniquePtr<RTCStreamEncoder> &&encoder, RTCStreamParams params = { })
        : m_stream_type(stream_type),
          m_encoder(std::move(encoder)),
          m_params(params),
          m_timestamp(0)
    {
    }

    RTCStream(const RTCStream &other)                   = delete;
    RTCStream &operator=(const RTCStream &other)        = delete;
    RTCStream(RTCStream &&other) noexcept               = default;
    RTCStream &operator=(RTCStream &&other) noexcept    = default;
    virtual ~RTCStream()                                = default;

    RTCStreamType GetStreamType() const
        { return m_stream_type; }

    const UniquePtr<RTCStreamEncoder> &GetEncoder() const
        { return m_encoder; }

    UInt64 GetCurrentTimestamp() const
        { return m_timestamp; }

    virtual void SendSample(const RTCStreamDestination &destination);

    virtual void Start();
    virtual void Stop();

protected:
    RTCStreamType               m_stream_type;
    UniquePtr<RTCStreamEncoder> m_encoder;
    RTCStreamParams             m_params;
    UInt64                      m_timestamp;
};

class NullRTCStream : public RTCStream
{
public:
    NullRTCStream(RTCStreamType stream_type, UniquePtr<RTCStreamEncoder> &&encoder)
        : RTCStream(stream_type, std::move(encoder))
    {
    }

    NullRTCStream(const NullRTCStream &other)                   = delete;
    NullRTCStream &operator=(const NullRTCStream &other)        = delete;
    NullRTCStream(NullRTCStream &&other) noexcept               = default;
    NullRTCStream &operator=(NullRTCStream &&other) noexcept    = default;
    virtual ~NullRTCStream() override                           = default;
};

#ifdef HYP_LIBDATACHANNEL

class LibDataChannelRTCStream : public RTCStream
{
public:
    LibDataChannelRTCStream(RTCStreamType stream_type, UniquePtr<RTCStreamEncoder> &&encoder);
    LibDataChannelRTCStream(const LibDataChannelRTCStream &other)                   = delete;
    LibDataChannelRTCStream &operator=(const LibDataChannelRTCStream &other)        = delete;
    LibDataChannelRTCStream(LibDataChannelRTCStream &&other) noexcept               = default;
    LibDataChannelRTCStream &operator=(LibDataChannelRTCStream &&other) noexcept    = default;
    virtual ~LibDataChannelRTCStream() override                                     = default;
};

#else

using LibDataChannelRTCStream = NullRTCStream;

#endif // HYP_LIBDATACHANNEL

} // namespace hyperion::v2

#endif