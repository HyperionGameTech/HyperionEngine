/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Containers/String.hpp>

#include <Core/Memory/ByteBuffer.hpp>
#include <Core/Memory/UniquePtr.hpp>
#include <Core/Memory/RefCountedPtr.hpp>

namespace Hyperion {
namespace threading {
class TaskThread;
} // namespace threading

using threading::TaskThread;

class RTCStreamEncoder;
class RTCTrackBase;

enum RTCStreamType
{
    RTC_STREAM_TYPE_UNKNOWN = 0,
    RTC_STREAM_TYPE_AUDIO,
    RTC_STREAM_TYPE_VIDEO
};

struct RTCStreamDestination
{
    Array<RC<RTCTrackBase>> tracks;
};

struct RTCStreamParams
{
    uint32 samplesPerSecond = 60;

    uint32 GetSampleDuration() const
    {
        return 1000 * 1000 / samplesPerSecond;
    }
};

class ENGINE_API RTCStream
{
public:
    RTCStream(RTCStreamType streamType, UniquePtr<RTCStreamEncoder>&& encoder, RTCStreamParams params = {})
        : m_streamType(streamType),
          m_encoder(std::move(encoder)),
          m_params(params),
          m_timestamp(0)
    {
    }

    RTCStream(const RTCStream& other) = delete;
    RTCStream& operator=(const RTCStream& other) = delete;
    RTCStream(RTCStream&& other) noexcept = default;
    RTCStream& operator=(RTCStream&& other) noexcept = default;
    virtual ~RTCStream() = default;

    RTCStreamType GetStreamType() const
    {
        return m_streamType;
    }

    const UniquePtr<RTCStreamEncoder>& GetEncoder() const
    {
        return m_encoder;
    }

    uint64 GetCurrentTimestamp() const
    {
        return m_timestamp;
    }

    virtual void SendSample(const RTCStreamDestination& destination);

    virtual void Start();
    virtual void Stop();

protected:
    RTCStreamType m_streamType;
    UniquePtr<RTCStreamEncoder> m_encoder;
    RTCStreamParams m_params;
    uint64 m_timestamp;
};

class ENGINE_API NullRTCStream : public RTCStream
{
public:
    NullRTCStream(RTCStreamType streamType, UniquePtr<RTCStreamEncoder>&& encoder)
        : RTCStream(streamType, std::move(encoder))
    {
    }

    NullRTCStream(const NullRTCStream& other) = delete;
    NullRTCStream& operator=(const NullRTCStream& other) = delete;
    NullRTCStream(NullRTCStream&& other) noexcept = default;
    NullRTCStream& operator=(NullRTCStream&& other) noexcept = default;
    virtual ~NullRTCStream() override = default;
};

#ifdef HYP_LIBDATACHANNEL

class ENGINE_API LibDataChannelRTCStream : public RTCStream
{
public:
    LibDataChannelRTCStream(RTCStreamType streamType, UniquePtr<RTCStreamEncoder>&& encoder);
    LibDataChannelRTCStream(const LibDataChannelRTCStream& other) = delete;
    LibDataChannelRTCStream& operator=(const LibDataChannelRTCStream& other) = delete;
    LibDataChannelRTCStream(LibDataChannelRTCStream&& other) noexcept = default;
    LibDataChannelRTCStream& operator=(LibDataChannelRTCStream&& other) noexcept = default;
    virtual ~LibDataChannelRTCStream() override = default;
};

#else

using LibDataChannelRTCStream = NullRTCStream;

#endif // HYP_LIBDATACHANNEL

} // namespace Hyperion
