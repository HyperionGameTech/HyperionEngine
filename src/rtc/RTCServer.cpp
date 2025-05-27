/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#include <rtc/RTCServer.hpp>
#include <rtc/RTCClientList.hpp>
#include <rtc/RTCClient.hpp>
#include <rtc/RTCServerThread.hpp>

#include <core/memory/Memory.hpp>

#ifdef HYP_LIBDATACHANNEL

    #include <rtc/rtc.hpp>
    #include <variant>

#else
// Stub it out

namespace rtc {
class WebSocket
{
};
} // namespace rtc

#endif // HYP_LIBDATACHANNEL

namespace hyperion {

RTCServer::RTCServer(RTCServerParams params)
    : m_params(std::move(params)),
      m_thread(MakeUnique<RTCServerThread>())
{
}

RTCServer::~RTCServer()
{
    if (m_thread)
    {
        if (m_thread->IsRunning())
        {
            m_thread->Stop();
        }

        if (m_thread->CanJoin())
        {
            m_thread->Join();
        }
    }
}

void RTCServer::EnqueueClientRemoval(String client_id)
{
    AssertThrow(m_thread != nullptr && m_thread->IsRunning());

    m_thread->GetScheduler().Enqueue([this, id = std::move(client_id)]() mutable
        {
            Optional<RC<RTCClient>> client = m_client_list.Get(id);

            if (!client.HasValue())
            {
                return;
            }

            client.Get()->Disconnect();

            m_client_list.Remove(id);
        },
        TaskEnqueueFlags::FIRE_AND_FORGET);
}

// null (stubbed) implementation

NullRTCServer::NullRTCServer(RTCServerParams params)
    : RTCServer(std::move(params))
{
}

void NullRTCServer::Start()
{
    // Do nothing
}

void NullRTCServer::Stop()
{
    // Do nothing
}

RC<RTCClient> NullRTCServer::CreateClient(String id)
{
    RC<RTCClient> client = MakeRefCountedPtr<NullRTCClient>(id, this);
    m_client_list.Add(id, client);

    return client;
}

void NullRTCServer::SendToSignallingServer(ByteBuffer bytes)
{
    // Do nothing
}

void NullRTCServer::SendToClient(String client_id, const ByteBuffer& bytes)
{
    // Do nothing
}

// libdatachannel implementation

#ifdef HYP_LIBDATACHANNEL

LibDataChannelRTCServer::LibDataChannelRTCServer(RTCServerParams params)
    : RTCServer(std::move(params))
{
}

LibDataChannelRTCServer::~LibDataChannelRTCServer()
{
    Stop();
}

void LibDataChannelRTCServer::Start()
{
    AssertThrowMsg(!m_thread->IsRunning(), "LibDataChannelRTCServer::Start() called, but server is already running!");

    AssertThrowMsg(m_websocket == nullptr, "LibDataChannelRTCServer::Start() called, but m_websocket is not nullptr!");

    m_websocket.Emplace();

    m_thread->Start(this);
    m_thread->GetScheduler().Enqueue([this]()
        {
            m_websocket->onOpen([this]()
                {
                    m_callbacks.OnConnected({ Optional<ByteBuffer>(),
                        Optional<RTCServerError>() });
                });

            m_websocket->onClosed([this]()
                {
                    m_callbacks.OnDisconnected({ Optional<ByteBuffer>(),
                        Optional<RTCServerError>() });

                    Stop();
                });

            m_websocket->onError([this](const std::string& error)
                {
                    m_callbacks.OnError({ Optional<ByteBuffer>(),
                        Optional<RTCServerError>(RTCServerError { error.c_str() }) });
                });

            m_websocket->onMessage([this](rtc::message_variant data)
                {
                    if (std::holds_alternative<rtc::binary>(data))
                    {
                        const rtc::binary& bytes = std::get<rtc::binary>(data);

                        m_callbacks.OnMessage({ Optional<ByteBuffer>(ByteBuffer(bytes.size(), bytes.data())),
                            Optional<RTCServerError>() });
                    }
                    else
                    {
                        const std::string& str = std::get<std::string>(data);

                        m_callbacks.OnMessage({ Optional<ByteBuffer>(ByteBuffer(str.size(), str.data())),
                            Optional<RTCServerError>() });
                    }
                });

            const String websocket_url = m_params.address.host + ":"
                + String::ToString(m_params.address.port)
                + (m_params.address.path.Any()
                        ? m_params.address.path.StartsWith("/")
                            ? m_params.address.path
                            : "/" + m_params.address.path
                        : "");

            DebugLog(LogType::Debug, "Attempting to connect websocket server to url: %s\n", websocket_url.Data());

            m_websocket->open(websocket_url.Data());
        },
        TaskEnqueueFlags::FIRE_AND_FORGET);
}

void LibDataChannelRTCServer::Stop()
{
    if (m_thread != nullptr)
    {
        m_thread->GetScheduler().Enqueue([this, ws = std::move(m_websocket)](...) mutable
            {
                for (const auto& client : m_client_list)
                {
                    client.second->Disconnect();
                }

                if (ws != nullptr)
                {
                    if (ws->isOpen())
                    {
                        ws->close();
                    }
                }
            },
            TaskEnqueueFlags::FIRE_AND_FORGET);
    }
    else
    {
        m_websocket.Reset();
    }
}

RC<RTCClient> LibDataChannelRTCServer::CreateClient(String id)
{
    RC<RTCClient> client = MakeRefCountedPtr<LibDataChannelRTCClient>(id, this);
    m_client_list.Add(id, client);

    return client;
}

void LibDataChannelRTCServer::SendToSignallingServer(ByteBuffer bytes)
{
    AssertThrowMsg(m_thread->IsRunning(), "LibDataChannelRTCServer::SendToSignallingServer() called, but server is not running!");

    AssertThrow(m_websocket != nullptr);
    AssertThrowMsg(m_websocket->isOpen(), "Expected websocket to be open");

    if (!bytes.Size())
    {
        return;
    }

    m_thread->GetScheduler().Enqueue([this, byte_buffer = MakeRefCountedPtr<ByteBuffer>(std::move(bytes))]() mutable
        {
            rtc::binary bin;
            bin.resize(byte_buffer->Size());

            Memory::MemCpy(bin.data(), byte_buffer->Data(), byte_buffer->Size());

            if (!m_websocket->send(std::move(bin)))
            {
                m_callbacks.OnError({ Optional<ByteBuffer>(),
                    Optional<RTCServerError>(RTCServerError { "Message could not be sent" }) });
            }
        },
        TaskEnqueueFlags::FIRE_AND_FORGET);
}

void LibDataChannelRTCServer::SendToClient(String client_id, const ByteBuffer& bytes)
{
    AssertThrowMsg(m_thread->IsRunning(), "LibDataChannelRTCServer::SendToClient() called, but server is not running!");

    // if (Optional<RC<RTCClient>> client = m_client_list.Get(client_id)) {
    //     client.Get()->Send(bytes);
    // }
}

#endif // HYP_LIBDATACHANNEL

} // namespace hyperion