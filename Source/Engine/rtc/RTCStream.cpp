/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <HyperionPch.hpp>
#include <rtc/RTCStream.hpp>
#include <rtc/RTCServer.hpp>
#include <rtc/RTCStreamEncoder.hpp>
#include <rtc/RTCTrack.hpp>

#include <Core/threading/TaskThread.hpp>

#ifdef HYP_LIBDATACHANNEL
#include <rtc/rtcpsrreporter.hpp>
#include <rtc/rtppacketizationconfig.hpp>
#endif

namespace Hyperion {

void RTCStream::Start()
{
    if (m_encoder)
    {
        m_encoder->Start();
    }
}

void RTCStream::Stop()
{
    if (m_encoder)
    {
        m_encoder->Stop();
    }
}

void RTCStream::SendSample(const RTCStreamDestination& destination)
{
    if (!m_encoder)
    {
        DebugLog(LogType::Warn, "SendSample() called but encoder is not set\n");

        return;
    }

    uint32 numSamples = 0;

    while (auto sample = m_encoder->PullData())
    {
        for (const RC<RTCTrackBase>& track : destination.tracks)
        {
            if (!track->IsOpen())
            {
                continue;
            }

            track->SendData(sample.Get(), m_timestamp);
        }

        ++numSamples;
    }

    m_timestamp += m_params.GetSampleDuration();

    // DebugLog(LogType::Debug, "Sent %u samples to %llu tracks\n", numSamples, destination.tracks.Size());
}

#ifdef HYP_LIBDATACHANNEL

LibDataChannelRTCStream::LibDataChannelRTCStream(RTCStreamType streamType, UniquePtr<RTCStreamEncoder>&& encoder)
    : RTCStream(streamType, std::move(encoder), { 60 })
{
}

#endif

} // namespace Hyperion
