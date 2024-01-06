#include <rtc/RTCClient.hpp>
#include <rtc/RTCServer.hpp>
#include <rtc/RTCTrack.hpp>
#include <rtc/RTCDataChannel.hpp>

#include <util/json/JSON.hpp>

#ifdef HYP_LIBDATACHANNEL

#include <rtc/configuration.hpp>
#include <rtc/peerconnection.hpp>

#include <variant>
#endif // HYP_LIBDATACHANNEL

namespace hyperion::v2 {

Optional<RC<RTCDataChannel>> RTCClient::GetDataChannel(Name name) const
{
    const auto it = m_data_channels.Find(name);

    if (it == m_data_channels.End()) {
        return { };
    }

    return it->second;
}

void RTCClient::AddTrack(RC<RTCTrack> track)
{
    if (m_state == RTC_CLIENT_STATE_CONNECTED || m_state == RTC_CLIENT_STATE_CONNECTING) {
        // Already connected/connecting, prepare the track immediately
        track->PrepareTrack(this);
    }

    m_tracks.PushBack(std::move(track));
}

void RTCClient::PrepareTracks()
{
    for (const RC<RTCTrack> &track : m_tracks) {
        track->PrepareTrack(this);
    }
}


NullRTCClient::NullRTCClient(String id, RTCServer *server)
    : RTCClient(std::move(id), server)
{
}

RC<RTCDataChannel> NullRTCClient::CreateDataChannel(Name name)
{
    auto data_channel = RC<RTCDataChannel>(new NullRTCDataChannel());

    m_data_channels.Insert(name, data_channel);

    return data_channel;
}

void NullRTCClient::Connect()
{
}

void NullRTCClient::Disconnect()
{
}

void NullRTCClient::SetRemoteDescription(const String &type, const String &sdp)
{
}

#ifdef HYP_LIBDATACHANNEL

LibDataChannelRTCClient::LibDataChannelRTCClient(String id, RTCServer *server)
    : RTCClient(std::move(id), server)
{
    const char stun_server[] = "stun:stun.l.google.com:19302";

    rtc::Configuration rtc_configuration;
    rtc_configuration.iceServers.emplace_back(stun_server);
    rtc_configuration.disableAutoNegotiation = true;

    RC<rtc::PeerConnection> peer_connection;
    peer_connection.Reset(new rtc::PeerConnection(rtc_configuration));

    peer_connection->onStateChange([this, id = m_id](rtc::PeerConnection::State state)
    {
        DebugLog(LogType::Debug, "State changed for Client with ID %s: %d\n", id.Data(), Int(state));

        switch (state) {
        case rtc::PeerConnection::State::Disconnected:
            DebugLog(LogType::Debug, "Client with ID %s disconnected\n", id.Data());
            
            m_state = RTC_CLIENT_STATE_DISCONNECTED;

            m_callbacks.Trigger(RTCClientCallbackMessages::DISCONNECTED, RTCClientCallbackData { });

            m_server->EnqueueClientRemoval(id);

            break;
        case rtc::PeerConnection::State::Failed:
            DebugLog(LogType::Debug, "Client with ID %s connection failed\n", id.Data());
            
            m_state = RTC_CLIENT_STATE_DISCONNECTED;

            m_callbacks.Trigger(RTCClientCallbackMessages::ERROR, RTCClientCallbackData {
                Optional<ByteBuffer>(),
                Optional<RTCClientError>({ "Connection failed" })
            });

            m_server->EnqueueClientRemoval(id);

            break;
        case rtc::PeerConnection::State::Closed:
            DebugLog(LogType::Debug, "Client with ID %s connection closed\n", id.Data());
            
            m_state = RTC_CLIENT_STATE_DISCONNECTED;

            m_server->EnqueueClientRemoval(id);

            break;
        case rtc::PeerConnection::State::Connecting:
            m_state = RTC_CLIENT_STATE_CONNECTING;

            break;
        case rtc::PeerConnection::State::Connected:
            m_state = RTC_CLIENT_STATE_CONNECTED;

            m_callbacks.Trigger(RTCClientCallbackMessages::CONNECTED, RTCClientCallbackData { });

            break;
        default:
            break;
        }
    });

    peer_connection->onGatheringStateChange([this, id = m_id, pc_weak = Weak<rtc::PeerConnection>(peer_connection)](rtc::PeerConnection::GatheringState state)
    {
        DebugLog(LogType::Debug, "Gathering state changed for Client with ID %s: %d\n", id.Data(), Int(state));

        if (state != rtc::PeerConnection::GatheringState::Complete) {
            return;
        }

        if (auto peer_connection = pc_weak.Lock()) {
            auto description = peer_connection->localDescription();

            const json::JSONValue message_json(json::JSONObject({
                { "id", id },
                { "type", description->typeString().c_str() },
                { "sdp", String(std::string(description.value()).c_str()) }
            }));

            const String message_string = message_json.ToString();

            DebugLog(LogType::Debug, " <- %s\n", message_string.Data());
            
            m_server->SendToSignallingServer(ByteBuffer(message_string.Size(), message_string.Data()));
        }
    });

    m_peer_connection = peer_connection;
}

RC<RTCDataChannel> LibDataChannelRTCClient::CreateDataChannel(Name name)
{
    AssertThrow(m_peer_connection != nullptr);

    if (name == Name::invalid) {
        name = CreateNameFromDynamicString(ANSIString("dc_") + ANSIString::ToString(m_data_channels.Size()));
    }

    RC<LibDataChannelRTCDataChannel> data_channel;
    data_channel.Reset(new LibDataChannelRTCDataChannel());

    data_channel->m_data_channel = m_peer_connection->createDataChannel(name.LookupString());

    data_channel->m_data_channel->onOpen([data_channel_weak = Weak<LibDataChannelRTCDataChannel>(data_channel)](...) mutable
    {
        if (auto data_channel = data_channel_weak.Lock()) {
            data_channel->Send("Ping");
        } else {
            AssertThrow(false);
        }
    });

    data_channel->m_data_channel->onMessage([this](rtc::message_variant data)
    {
        if (std::holds_alternative<rtc::binary>(data)) {
            const rtc::binary &bytes = std::get<rtc::binary>(data);

            m_callbacks.Trigger(RTCClientCallbackMessages::MESSAGE, {
                Optional<ByteBuffer>(ByteBuffer(bytes.size(), bytes.data())),
                Optional<RTCClientError>()
            });
        } else {
            const std::string &str = std::get<std::string>(data);

            m_callbacks.Trigger(RTCClientCallbackMessages::MESSAGE, {
                Optional<ByteBuffer>(ByteBuffer(str.size(), str.data())),
                Optional<RTCClientError>()
            });
        }
    });

    m_data_channels.Insert(name, data_channel);

    return data_channel;
}

void LibDataChannelRTCClient::Connect()
{
    PrepareTracks();

    m_state = RTC_CLIENT_STATE_CONNECTING;

    m_peer_connection->setLocalDescription();
}

void LibDataChannelRTCClient::Disconnect()
{
    if (!m_peer_connection || m_peer_connection->state() == rtc::PeerConnection::State::Closed) {
        return;
    }

    m_peer_connection->close();

    m_state = RTC_CLIENT_STATE_DISCONNECTED;
}

void LibDataChannelRTCClient::SetRemoteDescription(const String &type, const String &sdp)
{
    AssertThrow(m_peer_connection != nullptr);

    m_peer_connection->setRemoteDescription(rtc::Description(sdp.Data(), type.Data()));
}

#endif // HYP_LIBDATACHANNEL

}  // namespace hyperion::v2