/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/containers/String.hpp>
#include <Core/memory/ByteBuffer.hpp>
#include <Core/memory/UniquePtr.hpp>

#ifdef HYP_LIBDATACHANNEL

namespace rtc {
class Track;
class RtcpSrReporter;
class RtpPacketizationConfig;
} // namespace rtc

#include <memory>
#endif // HYP_LIBDATACHANNEL

namespace Hyperion {

class RTCClient;
class RTCStream;

enum RTCTrackType
{
    RTC_TRACK_TYPE_UNKNOWN = 0,
    RTC_TRACK_TYPE_AUDIO,
    RTC_TRACK_TYPE_VIDEO
};

class HYP_API RTCTrackBase
{
public:
    RTCTrackBase(RTCTrackType trackType)
        : m_trackType(trackType)
    {
    }

    RTCTrackBase(const RTCTrackBase& other) = delete;
    RTCTrackBase& operator=(const RTCTrackBase& other) = delete;
    RTCTrackBase(RTCTrackBase&& other) noexcept = default;
    RTCTrackBase& operator=(RTCTrackBase&& other) noexcept = default;
    virtual ~RTCTrackBase() = default;

    RTCTrackType GetTrackType() const
    {
        return m_trackType;
    }

    virtual bool IsOpen() const = 0;

    virtual void PrepareTrack(RTCClient* client) = 0;

    virtual void SendData(const ByteBuffer& data, uint64 sampleTimestamp) = 0;

protected:
    RTCTrackType m_trackType;
};

class HYP_API NullRTCTrack : public RTCTrackBase
{
public:
    NullRTCTrack(RTCTrackType trackType)
        : RTCTrackBase(trackType)
    {
    }

    NullRTCTrack(const NullRTCTrack& other) = delete;
    NullRTCTrack& operator=(const NullRTCTrack& other) = delete;
    NullRTCTrack(NullRTCTrack&& other) noexcept = default;
    NullRTCTrack& operator=(NullRTCTrack&& other) noexcept = default;
    virtual ~NullRTCTrack() override = default;

    virtual bool IsOpen() const override;

    virtual void PrepareTrack(RTCClient* client) override;

    virtual void SendData(const ByteBuffer& data, uint64 sampleTimestamp) override;
};

#ifdef HYP_LIBDATACHANNEL

class HYP_API LibDataChannelRTCTrack : public RTCTrackBase
{
public:
    LibDataChannelRTCTrack(RTCTrackType trackType)
        : RTCTrackBase(trackType)
    {
    }

    LibDataChannelRTCTrack(const LibDataChannelRTCTrack& other) = delete;
    LibDataChannelRTCTrack& operator=(const LibDataChannelRTCTrack& other) = delete;
    LibDataChannelRTCTrack(LibDataChannelRTCTrack&& other) noexcept = default;
    LibDataChannelRTCTrack& operator=(LibDataChannelRTCTrack&& other) noexcept = default;
    virtual ~LibDataChannelRTCTrack() override = default;

    virtual bool IsOpen() const override;

    virtual void PrepareTrack(RTCClient* client) override;

    virtual void SendData(const ByteBuffer& data, uint64 sampleTimestamp) override;

private:
    std::shared_ptr<rtc::Track> m_track;
    std::shared_ptr<rtc::RtcpSrReporter> m_rtcpSrReporter;
    std::shared_ptr<rtc::RtpPacketizationConfig> m_rtpConfig;
};

#else

using LibDataChannelRTCTrack = NullRTCTrack;

#endif // HYP_LIBDATACHANNEL

} // namespace Hyperion
