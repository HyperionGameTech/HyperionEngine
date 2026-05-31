/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>
#include <RTC/RTCTrack.hpp>
#include <RTC/RTCClient.hpp>

#ifdef HYP_LIBDATACHANNEL
#include <RTC/description.hpp>
#include <RTC/peerconnection.hpp>
#include <RTC/rtppacketizationconfig.hpp>
#include <RTC/h264rtppacketizer.hpp>
#include <RTC/h264packetizationhandler.hpp>
#include <RTC/rtcpsrreporter.hpp>
#include <RTC/rtcpnackresponder.hpp>
#endif // HYP_LIBDATACHANNEL

namespace Hyperion {

bool NullRTCTrack::IsOpen() const
{
    return true;
}

void NullRTCTrack::SendData(const ByteBuffer& data, uint64 sampleTimestamp)
{
}

void NullRTCTrack::PrepareTrack(RTCClient* client)
{
}

#ifdef HYP_LIBDATACHANNEL

bool LibDataChannelRTCTrack::IsOpen() const
{
    return m_track != nullptr && m_track->isOpen();
}

void LibDataChannelRTCTrack::PrepareTrack(RTCClient* client)
{
    Assert(client != nullptr);

    const auto* libDataChannelClient = dynamic_cast<LibDataChannelRTCClient*>(client);
    Assert(libDataChannelClient != nullptr,
        "client must be a LibDataChannelRTCClient instance to use on LibDataChannelRTCTrack");

    Assert(libDataChannelClient->m_peerConnection != nullptr,
        "m_peer_connection is nullptr on the RTCClient -- make sure PrepareTrack() is being called in the right place");

    switch (m_trackType)
    {
    case RTC_TRACK_TYPE_AUDIO:
        break;
    case RTC_TRACK_TYPE_VIDEO:
    {
        auto videoDescription = rtc::Description::Video();
        videoDescription.addH264Codec(102);
        videoDescription.addSSRC(1, "video-stream", "stream1");

        m_track = libDataChannelClient->m_peerConnection->addTrack(videoDescription);

        m_rtpConfig = std::make_shared<rtc::RtpPacketizationConfig>(1, "video-stream", 102, rtc::H264RtpPacketizer::defaultClockRate);
        auto packetizer = std::make_shared<rtc::H264RtpPacketizer>(rtc::NalUnit::Separator::StartSequence, m_rtpConfig);
        auto h264Handler = std::make_shared<rtc::H264PacketizationHandler>(packetizer);

        m_rtcpSrReporter = std::make_shared<rtc::RtcpSrReporter>(m_rtpConfig);
        h264Handler->addToChain(m_rtcpSrReporter);

        auto nackResponder = std::make_shared<rtc::RtcpNackResponder>();
        h264Handler->addToChain(nackResponder);

        m_track->setMediaHandler(h264Handler);
        m_track->onOpen([this]
            {
                DebugLog(LogType::Debug, "Video channel opened\n");
            });

        m_track->onClosed([this]
            {
                DebugLog(LogType::Debug, "Video channel closed\n");
            });

        m_track->onError([this](std::string message)
            {
                DebugLog(LogType::Debug, "Video channel error: %s\n", message.c_str());
            });

        break;
    }

    default:
        HYP_UNREACHABLE();
    }
}

void LibDataChannelRTCTrack::SendData(const ByteBuffer& data, uint64 sampleTimestamp)
{
    if (!m_track)
    {
        DebugLog(LogType::Warn, "Track in undefined state, not sending data\n");

        return;
    }

    if (!m_track->isOpen())
    {
        DebugLog(LogType::Warn, "Track closed, not sending data\n");

        return;
    }

    const double elapsedTimeSeconds = double(sampleTimestamp) / double(1000 * 1000);

    const uint32 elapsedTimestamp = m_rtpConfig->secondsToTimestamp(elapsedTimeSeconds);
    m_rtpConfig->timestamp = m_rtpConfig->startTimestamp + elapsedTimestamp;

    const uint32 reportElapsedTimestamp = m_rtpConfig->timestamp - m_rtcpSrReporter->lastReportedTimestamp();

    if (m_rtpConfig->timestampToSeconds(reportElapsedTimestamp) > 1)
    {
        m_rtcpSrReporter->setNeedsToReport();
    }

    m_track->send(reinterpret_cast<const rtc::byte*>(data.Data()), data.Size());
}

#endif // HYP_LIBDATACHANNEL

} // namespace Hyperion
