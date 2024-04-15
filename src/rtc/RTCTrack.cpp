/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rtc/RTCTrack.hpp>
#include <rtc/RTCClient.hpp>

#ifdef HYP_LIBDATACHANNEL
#include <rtc/description.hpp>
#include <rtc/peerconnection.hpp>
#include <rtc/rtppacketizationconfig.hpp>
#include <rtc/h264rtppacketizer.hpp>
#include <rtc/h264packetizationhandler.hpp>
#include <rtc/rtcpsrreporter.hpp>
#include <rtc/rtcpnackresponder.hpp>
#endif // HYP_LIBDATACHANNEL

namespace hyperion::v2 {

bool NullRTCTrack::IsOpen() const
{
    return true;
}

void NullRTCTrack::SendData(const ByteBuffer &data, uint64 sample_timestamp)
{
}

void NullRTCTrack::PrepareTrack(RTCClient *client)
{
}


#ifdef HYP_LIBDATACHANNEL

bool LibDataChannelRTCTrack::IsOpen() const
{
    return m_track != nullptr && m_track->isOpen();
}

void LibDataChannelRTCTrack::PrepareTrack(RTCClient *client)
{
    AssertThrow(client != nullptr);

    const auto *lib_data_channel_client = dynamic_cast<LibDataChannelRTCClient *>(client);
    AssertThrowMsg(lib_data_channel_client != nullptr,
        "client must be a LibDataChannelRTCClient instance to use on LibDataChannelRTCTrack");

    AssertThrowMsg(lib_data_channel_client->m_peer_connection != nullptr,
        "m_peer_connection is nullptr on the RTCClient -- make sure PrepareTrack() is being called in the right place");

    switch (m_track_type) {
    case RTC_TRACK_TYPE_AUDIO:
        break;
    case RTC_TRACK_TYPE_VIDEO: {
        auto video_description = rtc::Description::Video();
        video_description.addH264Codec(102);
        video_description.addSSRC(1, "video-stream", "stream1");

        m_track = lib_data_channel_client->m_peer_connection->addTrack(video_description);

        m_rtp_config = std::make_shared<rtc::RtpPacketizationConfig>(1, "video-stream", 102, rtc::H264RtpPacketizer::defaultClockRate);
        auto packetizer = std::make_shared<rtc::H264RtpPacketizer>(rtc::NalUnit::Separator::StartSequence, m_rtp_config);
        auto h264_handler = std::make_shared<rtc::H264PacketizationHandler>(packetizer);

        m_rtcp_sr_reporter = std::make_shared<rtc::RtcpSrReporter>(m_rtp_config);
        h264_handler->addToChain(m_rtcp_sr_reporter);

        auto nack_responder = std::make_shared<rtc::RtcpNackResponder>();
        h264_handler->addToChain(nack_responder);

        m_track->setMediaHandler(h264_handler);
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
        AssertThrowMsg(false, "Invalid track type");
    }
}


void LibDataChannelRTCTrack::SendData(const ByteBuffer &data, uint64 sample_timestamp)
{
    if (!m_track) {
        DebugLog(LogType::Warn, "Track in undefined state, not sending data\n");

        return;
    }

    if (!m_track->isOpen()) {
        DebugLog(LogType::Warn, "Track closed, not sending data\n");

        return;
    }

    const double elapsed_time_seconds = double(sample_timestamp) / double(1000 * 1000);
    
    const uint32 elapsed_timestamp = m_rtp_config->secondsToTimestamp(elapsed_time_seconds);
    m_rtp_config->timestamp = m_rtp_config->startTimestamp + elapsed_timestamp;

    const uint32 report_elapsed_timestamp = m_rtp_config->timestamp - m_rtcp_sr_reporter->lastReportedTimestamp();

    if (m_rtp_config->timestampToSeconds(report_elapsed_timestamp) > 1) {
        m_rtcp_sr_reporter->setNeedsToReport();
    }

    m_track->send(reinterpret_cast<const rtc::byte *>(data.Data()), data.Size());
}


#endif // HYP_LIBDATACHANNEL

}  // namespace hyperion::v2