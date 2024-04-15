/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#include <rtc/RTCStream.hpp>
#include <rtc/RTCServer.hpp>
#include <rtc/RTCStreamEncoder.hpp>
#include <rtc/RTCTrack.hpp>

#include <TaskThread.hpp>

#ifdef HYP_LIBDATACHANNEL
#include <rtc/rtcpsrreporter.hpp>
#include <rtc/rtppacketizationconfig.hpp>
#endif

namespace hyperion::v2 {

void RTCStream::Start()
{
    if (m_encoder) {
        m_encoder->Start();
    }
}

void RTCStream::Stop()
{
    if (m_encoder) {
        m_encoder->Stop();
    }
}

void RTCStream::SendSample(const RTCStreamDestination &destination)
{
    if (!m_encoder) {
        DebugLog(LogType::Warn, "SendSample() called but encoder is not set\n");

        return;
    }

    uint num_samples = 0;

    while (auto sample = m_encoder->PullData()) {
        for (const RC<RTCTrack> &track : destination.tracks) {
            if (!track->IsOpen()) {
                continue;
            }

            track->SendData(sample.Get(), m_timestamp);
        }

        ++num_samples;
    }

    m_timestamp += m_params.GetSampleDuration();

    // DebugLog(LogType::Debug, "Sent %u samples to %llu tracks\n", num_samples, destination.tracks.Size());
}

#ifdef HYP_LIBDATACHANNEL

LibDataChannelRTCStream::LibDataChannelRTCStream(RTCStreamType stream_type, UniquePtr<RTCStreamEncoder> &&encoder)
    : RTCStream(stream_type, std::move(encoder), { 60 })
{
}

#endif

}  // namespace hyperion::v2