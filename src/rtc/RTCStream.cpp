#include <rtc/RTCStream.hpp>
#include <rtc/RTCServer.hpp>
#include <rtc/RTCStreamEncoder.hpp>
#include <rtc/RTCTrack.hpp>

#include <TaskThread.hpp>

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

    UInt num_samples = 0;

    while (auto sample = m_encoder->PullData()) {
        for (const RC<RTCTrack> &track : destination.tracks) {
            track->SendData(sample.Get());
        }

        ++num_samples;
    }

    // DebugLog(LogType::Debug, "Sent %u samples to %llu tracks\n", num_samples, destination.tracks.Size());
}

}  // namespace hyperion::v2