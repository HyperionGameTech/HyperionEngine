/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>
#include <rtc/RTCClient.hpp>
#include <rtc/RTCServer.hpp>
#include <rtc/RTCTrack.hpp>
#include <rtc/RTCDataChannel.hpp>

#include <core/json/JSON.hpp>

#ifdef HYP_LIBDATACHANNEL

#include <rtc/configuration.hpp>
#include <rtc/peerconnection.hpp>

#include <variant>
#endif // HYP_LIBDATACHANNEL

namespace Hyperion {

Optional<RC<RTCDataChannel>> RTCClient::GetDataChannel(Name name) const
{
    const auto it = m_dataChannels.Find(name);

    if (it == m_dataChannels.End())
    {
        return {};
    }

    return it->second;
}

void RTCClient::AddTrack(RC<RTCTrackBase> track)
{
    if (m_state == RTC_CLIENT_STATE_CONNECTED || m_state == RTC_CLIENT_STATE_CONNECTING)
    {
        // Already connected/connecting, prepare the track immediately
        track->PrepareTrack(this);
    }

    m_tracks.PushBack(std::move(track));
}

void RTCClient::PrepareTracks()
{
    for (const RC<RTCTrackBase>& track : m_tracks)
    {
        track->PrepareTrack(this);
    }
}

NullRTCClient::NullRTCClient(String id, RTCServer* server)
    : RTCClient(std::move(id), server)
{
}

RC<RTCDataChannel> NullRTCClient::CreateDataChannel(Name name)
{
    RC<NullRTCDataChannel> dataChannel = MakeRefCountedPtr<NullRTCDataChannel>();

    m_dataChannels.Insert(name, dataChannel);

    return dataChannel;
}

void NullRTCClient::Connect()
{
}

void NullRTCClient::Disconnect()
{
}

void NullRTCClient::SetRemoteDescription(const String& type, const String& sdp)
{
}

#ifdef HYP_LIBDATACHANNEL

LibDataChannelRTCClient::LibDataChannelRTCClient(String id, RTCServer* server)
    : RTCClient(std::move(id), server)
{
    const char stunServer[] = "stun:stun.l.google.com:19302";

    rtc::Configuration rtcConfiguration;
    rtcConfiguration.iceServers.emplaceBack(stunServer);
    rtcConfiguration.disableAutoNegotiation = true;

    RC<rtc::PeerConnection> peerConnection = MakeRefCountedPtr<rtc::PeerConnection>(rtcConfiguration);

    peerConnection->onStateChange([this, id = m_id](rtc::PeerConnection::State state)
        {
            DebugLog(LogType::Debug, "State changed for Client with Id %s: %d\n", id.Data(), int(state));

            switch (state)
            {
            case rtc::PeerConnection::State::Disconnected:
                DebugLog(LogType::Debug, "Client with Id %s disconnected\n", id.Data());

                m_state = RTC_CLIENT_STATE_DISCONNECTED;

                m_callbacks.OnDisconnected(RTCClientCallbackData {});

                m_server->EnqueueClientRemoval(id);

                break;
            case rtc::PeerConnection::State::Failed:
                DebugLog(LogType::Debug, "Client with Id %s connection failed\n", id.Data());

                m_state = RTC_CLIENT_STATE_DISCONNECTED;

                m_callbacks.OnError(RTCClientCallbackData {
                    Optional<ByteBuffer>(),
                    Optional<RTCClientError>({ "Connection failed" }) });

                m_server->EnqueueClientRemoval(id);

                break;
            case rtc::PeerConnection::State::Closed:
                DebugLog(LogType::Debug, "Client with Id %s connection closed\n", id.Data());

                m_state = RTC_CLIENT_STATE_DISCONNECTED;

                m_server->EnqueueClientRemoval(id);

                break;
            case rtc::PeerConnection::State::Connecting:
                m_state = RTC_CLIENT_STATE_CONNECTING;

                break;
            case rtc::PeerConnection::State::Connected:
                m_state = RTC_CLIENT_STATE_CONNECTED;

                m_callbacks.OnConnected(RTCClientCallbackData {});

                break;
            default:
                break;
            }
        });

    peerConnection->onGatheringStateChange([this, id = m_id, pcWeak = Weak<rtc::PeerConnection>(peerConnection)](rtc::PeerConnection::GatheringState state)
        {
            DebugLog(LogType::Debug, "Gathering state changed for Client with Id %s: %d\n", id.Data(), int(state));

            if (state != rtc::PeerConnection::GatheringState::Complete)
            {
                return;
            }

            if (auto peerConnection = pcWeak.Lock())
            {
                auto description = peerConnection->localDescription();

                const JSON::Value messageJson(JSON::Object({ { "id", id },
                    { "type", description->typeString().c_str() },
                    { "sdp", String(std::string(description.value()).c_str()) } }));

                const String messageString = messageJson.ToString();

                DebugLog(LogType::Debug, " <- %s\n", messageString.Data());

                m_server->SendToSignallingServer(ByteBuffer(messageString.Size(), messageString.Data()));
            }
        });

    m_peerConnection = peerConnection;
}

RC<RTCDataChannel> LibDataChannelRTCClient::CreateDataChannel(Name name)
{
    Assert(m_peerConnection != nullptr);

    if (name == Name::Invalid())
    {
        name = CreateNameFromDynamicString(ANSIString("dc_") + ANSIString::ToString(m_dataChannels.Size()));
    }

    RC<LibDataChannelRTCDataChannel> dataChannel = MakeRefCountedPtr<LibDataChannelRTCDataChannel>();

    dataChannel->m_dataChannel = m_peerConnection->createDataChannel(name.LookupString());

    dataChannel->m_dataChannel->onOpen([dataChannelWeak = Weak<LibDataChannelRTCDataChannel>(dataChannel)](...) mutable
        {
            if (auto dataChannel = dataChannelWeak.Lock())
            {
                dataChannel->Send("Ping");
            }
            else
            {
                HYP_FAIL("Failed to lock data channel for onOpen callback");
            }
        });

    dataChannel->m_dataChannel->onMessage([this](rtc::messageVariant data)
        {
            if (std::holds_alternative<rtc::binary>(data))
            {
                const rtc::binary& bytes = std::get<rtc::binary>(data);

                m_callbacks.OnMessage({ Optional<ByteBuffer>(ByteBuffer(bytes.size(), bytes.data())),
                    Optional<RTCClientError>() });
            }
            else
            {
                const std::string& str = std::get<std::string>(data);

                m_callbacks.OnMessage({ Optional<ByteBuffer>(ByteBuffer(str.size(), str.data())),
                    Optional<RTCClientError>() });
            }
        });

    m_dataChannels.Insert(name, dataChannel);

    return dataChannel;
}

void LibDataChannelRTCClient::Connect()
{
    PrepareTracks();

    m_state = RTC_CLIENT_STATE_CONNECTING;

    m_peerConnection->setLocalDescription();
}

void LibDataChannelRTCClient::Disconnect()
{
    if (!m_peerConnection || m_peerConnection->state() == rtc::PeerConnection::State::Closed)
    {
        return;
    }

    m_peerConnection->close();

    m_state = RTC_CLIENT_STATE_DISCONNECTED;
}

void LibDataChannelRTCClient::SetRemoteDescription(const String& type, const String& sdp)
{
    Assert(m_peerConnection != nullptr);

    m_peerConnection->setRemoteDescription(rtc::Description(sdp.Data(), type.Data()));
}

#endif // HYP_LIBDATACHANNEL

} // namespace Hyperion
