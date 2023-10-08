#ifndef HYPERION_V2_RTC_CLIENT_HPP
#define HYPERION_V2_RTC_CLIENT_HPP

#include <core/lib/String.hpp>
#include <core/lib/RefCountedPtr.hpp>

#ifdef HYP_LIBDATACHANNEL
namespace rtc {
class PeerConnection;
class DataChannel;
} // namespace rtc

#include <memory>

#endif

namespace hyperion::v2 {

class RTCServer;
class RTCStream;
class RTCTrack;

#ifdef HYP_LIBDATACHANNEL
class LibDataChannelRTCTrack;
#endif // HYP_LIBDATACHANNEL

enum RTCClientState
{
    RTC_CLIENT_STATE_UNKNOWN = 0,
    RTC_CLIENT_STATE_CONNECTING,
    RTC_CLIENT_STATE_CONNECTED,
    RTC_CLIENT_STATE_DISCONNECTED
};

class RTCClient
{
public:
    RTCClient(String id, RTCServer *server)
        : m_id(std::move(id)),
          m_server(server),
          m_state(RTC_CLIENT_STATE_DISCONNECTED)
    {
    }

    RTCClient(const RTCClient &other) = delete;
    RTCClient &operator=(const RTCClient &other) = delete;
    RTCClient(RTCClient &&other) noexcept = default;
    RTCClient &operator=(RTCClient &&other) noexcept = default;
    virtual ~RTCClient() = default;

    const String &GetID() const
        { return m_id; }

    RTCClientState GetState() const
        { return m_state; }

    const Array<RC<RTCTrack>> &GetTracks() const
        { return m_tracks; }

    virtual void Connect() = 0;
    virtual void Disconnect() = 0;

    virtual void SetRemoteDescription(const String &type, const String &sdp) = 0;

    virtual void AddTrack(RC<RTCTrack> track);

protected:
    virtual void PrepareTracks();

    String              m_id;
    RTCServer           *m_server;
    RTCClientState      m_state;
    Array<RC<RTCTrack>> m_tracks;
};

class NullRTCClient : public RTCClient
{
public:
    NullRTCClient(String id, RTCServer *server);
    NullRTCClient(const NullRTCClient &other) = delete;
    NullRTCClient &operator=(const NullRTCClient &other) = delete;
    NullRTCClient(NullRTCClient &&other) noexcept = default;
    NullRTCClient &operator=(NullRTCClient &&other) noexcept = default;
    virtual ~NullRTCClient() override = default;

    virtual void Connect() override;
    virtual void Disconnect() override;

    virtual void SetRemoteDescription(const String &type, const String &sdp) override;
};

#ifdef HYP_LIBDATACHANNEL

class LibDataChannelRTCClient : public RTCClient
{
    friend class LibDataChannelRTCTrack;

public:
    LibDataChannelRTCClient(String id, RTCServer *server);
    LibDataChannelRTCClient(const LibDataChannelRTCClient &other) = delete;
    LibDataChannelRTCClient &operator=(const LibDataChannelRTCClient &other) = delete;
    LibDataChannelRTCClient(LibDataChannelRTCClient &&other) noexcept = default;
    LibDataChannelRTCClient &operator=(LibDataChannelRTCClient &&other) noexcept = default;
    virtual ~LibDataChannelRTCClient() override = default;

    virtual void Connect() override;
    virtual void Disconnect() override;

    virtual void SetRemoteDescription(const String &type, const String &sdp) override;

private:
    RC<rtc::PeerConnection>             m_peer_connection;
    std::shared_ptr<rtc::DataChannel>   m_data_channel;
};

#else

using LibDataChannelRTCClient = NullRTCClient;

#endif // HYP_LIBDATACHANNEL

} // namespace hyperion::v2

#endif