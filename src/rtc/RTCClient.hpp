#ifndef HYPERION_V2_RTC_CLIENT_HPP
#define HYPERION_V2_RTC_CLIENT_HPP

#include <core/Containers.hpp>
#include <core/lib/String.hpp>
#include <core/lib/RefCountedPtr.hpp>
#include <core/lib/Optional.hpp>
#include <core/Name.hpp>

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
class RTCDataChannel;

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

enum class RTCClientCallbackMessages : UInt32
{
    UNKNOWN         = 0,

    ERROR           = UInt32(-1),

    CONNECTED       = 1,
    DISCONNECTED,

    MESSAGE
};

struct RTCClientError
{
    String message;
};

struct RTCClientCallbackData
{
    Optional<ByteBuffer>        bytes;
    Optional<RTCClientError>    error;
};

using RTCClientCallbacks = Callbacks<RTCClientCallbackMessages, RTCClientCallbackData>;

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
    RTCClient(RTCClient &&other) noexcept = delete;
    RTCClient &operator=(RTCClient &&other) noexcept = delete;
    virtual ~RTCClient() = default;

    const String &GetID() const
        { return m_id; }

    RTCClientState GetState() const
        { return m_state; }

    const Array<RC<RTCTrack>> &GetTracks() const
        { return m_tracks; }

    RTCClientCallbacks &GetCallbacks()
        { return m_callbacks; }

    const RTCClientCallbacks &GetCallbacks() const
        { return m_callbacks; }

    virtual RC<RTCDataChannel> CreateDataChannel(Name name = Name::invalid) = 0;
    Optional<RC<RTCDataChannel>> GetDataChannel(Name name) const;

    virtual void Connect() = 0;
    virtual void Disconnect() = 0;

    virtual void SetRemoteDescription(const String &type, const String &sdp) = 0;

    virtual void AddTrack(RC<RTCTrack> track);

protected:
    virtual void PrepareTracks();

    String                              m_id;
    RTCServer                           *m_server;
    RTCClientState                      m_state;
    Array<RC<RTCTrack>>                 m_tracks;
    FlatMap<Name, RC<RTCDataChannel>>   m_data_channels;
    RTCClientCallbacks                  m_callbacks;
};

class NullRTCClient : public RTCClient
{
public:
    NullRTCClient(String id, RTCServer *server);
    NullRTCClient(const NullRTCClient &other) = delete;
    NullRTCClient &operator=(const NullRTCClient &other) = delete;
    NullRTCClient(NullRTCClient &&other) noexcept = delete;
    NullRTCClient &operator=(NullRTCClient &&other) noexcept = delete;
    virtual ~NullRTCClient() override = default;

    virtual void Connect() override;
    virtual void Disconnect() override;

    virtual RC<RTCDataChannel> CreateDataChannel(Name name = Name::invalid) override;

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
    LibDataChannelRTCClient(LibDataChannelRTCClient &&other) noexcept = delete;
    LibDataChannelRTCClient &operator=(LibDataChannelRTCClient &&other) noexcept = delete;
    virtual ~LibDataChannelRTCClient() override = default;

    virtual RC<RTCDataChannel> CreateDataChannel(Name name = Name::invalid) override;

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