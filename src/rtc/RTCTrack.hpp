#ifndef HYPERION_V2_RTC_TRACK_HPP
#define HYPERION_V2_RTC_TRACK_HPP

#include <core/lib/String.hpp>
#include <core/lib/ByteBuffer.hpp>
#include <core/lib/UniquePtr.hpp>

#ifdef HYP_LIBDATACHANNEL

namespace rtc {
class Track;
class RtcpSrReporter;
class RtpPacketizationConfig;
} // namespace rtc

#include <memory>
#endif // HYP_LIBDATACHANNEL

namespace hyperion::v2 {

class RTCClient;
class RTCStream;

enum RTCTrackType
{
    RTC_TRACK_TYPE_UNKNOWN = 0,
    RTC_TRACK_TYPE_AUDIO,
    RTC_TRACK_TYPE_VIDEO
};

class RTCTrack
{
public:
    RTCTrack(RTCTrackType track_type)
        : m_track_type(track_type)
    {
    }

    RTCTrack(const RTCTrack &other)                     = delete;
    RTCTrack &operator=(const RTCTrack &other)          = delete;
    RTCTrack(RTCTrack &&other) noexcept                 = default;
    RTCTrack &operator=(RTCTrack &&other) noexcept      = default;
    virtual ~RTCTrack()                                 = default;

    RTCTrackType GetTrackType() const
        { return m_track_type; }

    virtual bool IsOpen() const = 0;

    virtual void PrepareTrack(RTCClient *client) = 0;

    virtual void SendData(const ByteBuffer &data, uint64 sample_timestamp) = 0;

protected:
    RTCTrackType    m_track_type;
};

class NullRTCTrack : public RTCTrack
{
public:
    NullRTCTrack(RTCTrackType track_type)
        : RTCTrack(track_type)
    {
    }

    NullRTCTrack(const NullRTCTrack &other)                     = delete;
    NullRTCTrack &operator=(const NullRTCTrack &other)          = delete;
    NullRTCTrack(NullRTCTrack &&other) noexcept                 = default;
    NullRTCTrack &operator=(NullRTCTrack &&other) noexcept      = default;
    virtual ~NullRTCTrack() override                            = default;

    virtual bool IsOpen() const override;

    virtual void PrepareTrack(RTCClient *client) override;

    virtual void SendData(const ByteBuffer &data, uint64 sample_timestamp) override;
};

#ifdef HYP_LIBDATACHANNEL

class LibDataChannelRTCTrack : public RTCTrack
{
public:
    LibDataChannelRTCTrack(RTCTrackType track_type)
        : RTCTrack(track_type)
    {
    }

    LibDataChannelRTCTrack(const LibDataChannelRTCTrack &other)                 = delete;
    LibDataChannelRTCTrack &operator=(const LibDataChannelRTCTrack &other)      = delete;
    LibDataChannelRTCTrack(LibDataChannelRTCTrack &&other) noexcept             = default;
    LibDataChannelRTCTrack &operator=(LibDataChannelRTCTrack &&other) noexcept  = default;
    virtual ~LibDataChannelRTCTrack() override                                  = default;

    virtual bool IsOpen() const override;

    virtual void PrepareTrack(RTCClient *client) override;

    virtual void SendData(const ByteBuffer &data, uint64 sample_timestamp) override;

private:
    std::shared_ptr<rtc::Track>                     m_track;
    std::shared_ptr<rtc::RtcpSrReporter>            m_rtcp_sr_reporter;
    std::shared_ptr<rtc::RtpPacketizationConfig>    m_rtp_config;
};

#else

using LibDataChannelRTCTrack = NullRTCTrack;

#endif // HYP_LIBDATACHANNEL

} // namespace hyperion::v2

#endif