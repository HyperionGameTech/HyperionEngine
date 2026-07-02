/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Memory/ByteBuffer.hpp>
#include <Core/Memory/UniquePtr.hpp>
#include <Core/Utilities/Optional.hpp>
#include <Core/Functional/Delegate.hpp>

#include <RTC/RTCClientList.hpp>

namespace rtc {
class WebSocket;
} // namespace rtc

namespace Hyperion {

class RTCClient;
class RTCServerThread;

struct RTCServerError
{
    String message;
};

struct RTCServerCallbackData
{
    Optional<ByteBuffer> bytes;
    Optional<RTCServerError> error;
};

struct RTCServerCallbacks
{
    Delegate<void, RTCServerCallbackData&> OnError;
    Delegate<void, RTCServerCallbackData&> OnConnected;
    Delegate<void, RTCServerCallbackData&> OnDisconnected;
    Delegate<void, RTCServerCallbackData&> OnMessage;
};

struct RTCServerAddress
{
    String host;
    uint16 port;
    String path;
};

struct RTCServerParams
{
    RTCServerAddress address;
};

class ENGINE_API RTCServer
{
public:
    RTCServer(RTCServerParams params);
    RTCServer(const RTCServer& other) = delete;
    RTCServer& operator=(const RTCServer& other) = delete;
    RTCServer(RTCServer&& other) = delete;
    RTCServer& operator=(RTCServer&& other) = delete;
    virtual ~RTCServer();

    virtual void Start() = 0;
    virtual void Stop() = 0;

    virtual SharedPtr<RTCClient> CreateClient(String id) = 0;

    void EnqueueClientRemoval(String clientId);

    virtual void SendToSignallingServer(ByteBuffer bytes) = 0;
    virtual void SendToClient(String clientId, const ByteBuffer& bytes) = 0;

    const RTCServerParams& GetParams() const
    {
        return m_params;
    }

    RTCServerCallbacks& GetCallbacks()
    {
        return m_callbacks;
    }

    const RTCServerCallbacks& GetCallbacks() const
    {
        return m_callbacks;
    }

    RTCClientList& GetClientList()
    {
        return m_clientList;
    }

    const RTCClientList& GetClientList() const
    {
        return m_clientList;
    }

protected:
    RTCServerParams m_params;
    RTCServerCallbacks m_callbacks;
    RTCClientList m_clientList;
    UniquePtr<RTCServerThread> m_thread;
};

class ENGINE_API NullRTCServer : public RTCServer
{
public:
    NullRTCServer(RTCServerParams params);
    NullRTCServer(const NullRTCServer& other) = delete;
    NullRTCServer& operator=(const NullRTCServer& other) = delete;
    NullRTCServer(NullRTCServer&& other) = delete;
    NullRTCServer& operator=(NullRTCServer&& other) = delete;
    virtual ~NullRTCServer() override = default;

    virtual void Start() override;
    virtual void Stop() override;

    virtual SharedPtr<RTCClient> CreateClient(String id) override;

    virtual void SendToSignallingServer(ByteBuffer bytes) override;
    virtual void SendToClient(String clientId, const ByteBuffer& bytes) override;
};

#ifdef HYP_LIBDATACHANNEL

class ENGINE_API LibDataChannelRTCServer : public RTCServer
{
public:
    LibDataChannelRTCServer(RTCServerParams params);
    LibDataChannelRTCServer(const LibDataChannelRTCServer& other) = delete;
    LibDataChannelRTCServer& operator=(const LibDataChannelRTCServer& other) = delete;
    LibDataChannelRTCServer(LibDataChannelRTCServer&& other) = delete;
    LibDataChannelRTCServer& operator=(LibDataChannelRTCServer&& other) = delete;
    virtual ~LibDataChannelRTCServer() override;

    virtual void Start() override;
    virtual void Stop() override;

    virtual SharedPtr<RTCClient> CreateClient(String id) override;

    virtual void SendToSignallingServer(ByteBuffer bytes) override;
    virtual void SendToClient(String clientId, const ByteBuffer& bytes) override;

private:
    UniquePtr<rtc::WebSocket> m_websocket;
};

#else

using LibDataChannelRTCServer = NullRTCServer;

#endif // HYP_LIBDATACHANNEL

} // namespace Hyperion
