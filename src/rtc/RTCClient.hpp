/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RTC_CLIENT_HPP
#define HYPERION_RTC_CLIENT_HPP

#include <core/containers/String.hpp>
#include <core/containers/FlatMap.hpp>
#include <core/containers/Array.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/functional/Delegate.hpp>

#include <core/utilities/Optional.hpp>

#include <core/Name.hpp>

#ifdef HYP_LIBDATACHANNEL
namespace rtc {
class PeerConnection;
class DataChannel;
} // namespace rtc

#include <memory>

#endif

namespace hyperion {

class RTCServer;
class RTCStream;
class RTCTrackBase;
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

struct RTCClientError
{
    String message;
};

struct RTCClientCallbackData
{
    Optional<ByteBuffer> bytes;
    Optional<RTCClientError> error;
};

struct RTCClientCallbacks
{
    Delegate<void, const RTCClientCallbackData&> OnError;
    Delegate<void, const RTCClientCallbackData&> OnConnected;
    Delegate<void, const RTCClientCallbackData&> OnDisconnected;
    Delegate<void, const RTCClientCallbackData&> OnMessage;
};

class HYP_API RTCClient
{
public:
    RTCClient(String id, RTCServer* server)
        : m_id(std::move(id)),
          m_server(server),
          m_state(RTC_CLIENT_STATE_DISCONNECTED)
    {
    }

    RTCClient(const RTCClient& other) = delete;
    RTCClient& operator=(const RTCClient& other) = delete;
    RTCClient(RTCClient&& other) noexcept = delete;
    RTCClient& operator=(RTCClient&& other) noexcept = delete;
    virtual ~RTCClient() = default;

    const String& Id() const
    {
        return m_id;
    }

    RTCClientState GetState() const
    {
        return m_state;
    }

    const Array<RC<RTCTrackBase>>& GetTracks() const
    {
        return m_tracks;
    }

    RTCClientCallbacks& GetCallbacks()
    {
        return m_callbacks;
    }

    const RTCClientCallbacks& GetCallbacks() const
    {
        return m_callbacks;
    }

    virtual RC<RTCDataChannel> CreateDataChannel(Name name = Name::Invalid()) = 0;
    Optional<RC<RTCDataChannel>> GetDataChannel(Name name) const;

    virtual void Connect() = 0;
    virtual void Disconnect() = 0;

    virtual void SetRemoteDescription(const String& type, const String& sdp) = 0;

    virtual void AddTrack(RC<RTCTrackBase> track);

protected:
    virtual void PrepareTracks();

    String m_id;
    RTCServer* m_server;
    RTCClientState m_state;
    Array<RC<RTCTrackBase>> m_tracks;
    FlatMap<Name, RC<RTCDataChannel>> m_data_channels;
    RTCClientCallbacks m_callbacks;
};

class NullRTCClient : public RTCClient
{
public:
    NullRTCClient(String id, RTCServer* server);
    NullRTCClient(const NullRTCClient& other) = delete;
    NullRTCClient& operator=(const NullRTCClient& other) = delete;
    NullRTCClient(NullRTCClient&& other) noexcept = delete;
    NullRTCClient& operator=(NullRTCClient&& other) noexcept = delete;
    virtual ~NullRTCClient() override = default;

    virtual void Connect() override;
    virtual void Disconnect() override;

    virtual RC<RTCDataChannel> CreateDataChannel(Name name = Name::Invalid()) override;

    virtual void SetRemoteDescription(const String& type, const String& sdp) override;
};

#ifdef HYP_LIBDATACHANNEL

class LibDataChannelRTCClient : public RTCClient
{
    friend class LibDataChannelRTCTrack;

public:
    LibDataChannelRTCClient(String id, RTCServer* server);
    LibDataChannelRTCClient(const LibDataChannelRTCClient& other) = delete;
    LibDataChannelRTCClient& operator=(const LibDataChannelRTCClient& other) = delete;
    LibDataChannelRTCClient(LibDataChannelRTCClient&& other) noexcept = delete;
    LibDataChannelRTCClient& operator=(LibDataChannelRTCClient&& other) noexcept = delete;
    virtual ~LibDataChannelRTCClient() override = default;

    virtual RC<RTCDataChannel> CreateDataChannel(Name name = Name::Invalid()) override;

    virtual void Connect() override;
    virtual void Disconnect() override;

    virtual void SetRemoteDescription(const String& type, const String& sdp) override;

private:
    RC<rtc::PeerConnection> m_peer_connection;
    std::shared_ptr<rtc::DataChannel> m_data_channel;
};

#else

using LibDataChannelRTCClient = NullRTCClient;

#endif // HYP_LIBDATACHANNEL

} // namespace hyperion

#endif