/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>
#include <RTC/RTCStream.hpp>
#include <RTC/RTCServer.hpp>
#include <RTC/RTCStreamEncoder.hpp>
#include <RTC/RTCTrack.hpp>

#include <Core/Threading/TaskThread.hpp>

#ifdef HYP_LIBDATACHANNEL
#include <RTC/rtcpsrreporter.hpp>
#include <RTC/rtppacketizationconfig.hpp>
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
