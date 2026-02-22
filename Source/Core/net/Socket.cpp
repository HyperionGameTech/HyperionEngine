/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/net/Socket.hpp>

#include <core/containers/Queue.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#ifdef HYP_UNIX

#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>

#endif

namespace Hyperion {

namespace net {

struct SocketServerImpl
{
    int socketId = 0;

#ifdef HYP_UNIX
    struct sockaddr_un local, remote;
#endif
};

#pragma region SocketServer

SocketServer::SocketServer(String name)
    : m_name(std::move(name)),
      m_impl(nullptr)
{
}

SocketServer::~SocketServer()
{
    if (m_impl != nullptr)
    {
        Stop();
    }

    HYP_CORE_ASSERT(m_thread == nullptr && !m_thread->IsRunning());
}

void SocketConnection::TriggerProc(Name eventName, Array<SocketProcArgument>&& args)
{
    const auto it = m_eventProcs.Find(eventName);

    if (it == m_eventProcs.End())
    {
        return;
    }

    it->second(std::move(args));
}

bool SocketServer::Start()
{
    if (m_impl)
    {
        return false;
    }

    m_impl = MakeUnique<SocketServerImpl>();

#ifdef HYP_UNIX
    m_impl->socketId = socket(AF_UNIX, SOCK_STREAM, 0);
#endif

    if (m_impl->socketId == -1)
    {
        HYP_LOG(Socket, Error, "Failed to open socket server. Code: {}", errno);

        TriggerProc(NAME("OnError"), { SocketProcArgument(String("Failed to open socket")), SocketProcArgument(int32(errno)) });

        return false;
    }

    const char* socketPath = m_name.Data();

    constexpr SizeType maxConnections = 5;

#ifdef HYP_UNIX
    m_impl->local.sun_family = AF_UNIX;
    strcpy(m_impl->local.sun_path, socketPath);
    unlink(m_impl->local.sun_path);

    const int len = strlen(m_impl->local.sun_path) + sizeof(m_impl->local.sun_family);

    int32 reuseSocket = 1;

    if (setsockopt(m_impl->socketId, SOL_SOCKET, SO_REUSEADDR, &reuseSocket, sizeof(reuseSocket)) != 0)
    {
        const int32 errorCode = errno;

        TriggerProc(NAME("OnError"), { SocketProcArgument(String(strerror(errorCode))), SocketProcArgument(errorCode) });

        return false;
    }

    if (setsockopt(m_impl->socketId, SOL_SOCKET, SO_REUSEPORT, &reuseSocket, sizeof(reuseSocket)) != 0)
    {
        const int32 errorCode = errno;

        TriggerProc(NAME("OnError"), { SocketProcArgument(String(strerror(errorCode))), SocketProcArgument(errorCode) });

        return false;
    }

    if (bind(m_impl->socketId, (struct sockaddr*)&m_impl->local, len) != 0)
    {
        const int32 errorCode = errno;

        TriggerProc(NAME("OnError"), { SocketProcArgument(String(strerror(errorCode))), SocketProcArgument(errorCode) });

        return false;
    }

    if (listen(m_impl->socketId, maxConnections) != 0)
    {
        const int32 errorCode = errno;

        TriggerProc(NAME("OnError"), { SocketProcArgument(String(strerror(errorCode))), SocketProcArgument(int32(errno)) });

        return false;
    }

    TriggerProc(NAME("OnServerStarted"), {});

    m_thread = MakeUnique<SocketServerThread>(m_name);
    m_thread->Start(this);

    return true;
#endif

    return false;
}

bool SocketServer::Stop()
{
    if (m_impl == nullptr)
    {
        return false;
    }

    if (m_thread != nullptr)
    {
        m_thread->Stop();

        if (m_thread->CanJoin())
        {
            m_thread->Join();
        }
    }

    { // Close all connections
        Mutex::Guard guard(m_connectionsMutex);

        for (auto& pair : m_connections)
        {
            if (pair.second != nullptr)
            {
                pair.second->Close();
            }
        }

        m_connections.Clear();
    }

#ifdef HYP_UNIX
    close(m_impl->socketId);
    unlink(m_impl->local.sun_path);

    TriggerProc(NAME("OnServerStopped"), {});

    m_impl.Reset();

    return true;
#endif

    return false;
}

bool SocketServer::PollForConnections(Array<RC<SocketClient>>& outConnections)
{
    if (m_impl == nullptr)
    {
        return false;
    }

    outConnections.Clear();

#if defined(HYP_UNIX)
    // the client
    struct sockaddr_un remote;
    socklen_t t = sizeof(remote);

    int newSocket;

    while ((newSocket = accept(m_impl->socketId, (struct sockaddr*)&remote, &t)) != -1)
    {
        const Name clientName = Name::Unique("socket_client");

        // Make the socket non-blocking
        if (fcntl(newSocket, F_SETFL, O_NONBLOCK) == -1)
        {
            HYP_LOG(Socket, Error, "Failed to set socket to non-blocking. Code: {}", errno);

            close(newSocket);
            continue;
        }

        outConnections.PushBack(MakeRefCountedPtr<SocketClient>(clientName, SocketID { newSocket }));
    }

    return true;

#endif

    return false;
}

void SocketServer::AddConnection(RC<SocketClient>&& connection)
{
    if (!connection)
    {
        return;
    }

    Mutex::Guard guard(m_connectionsMutex);

    connection->TriggerProc(NAME("OnClientConnected"), { SocketProcArgument(connection->GetName()) });

    m_connections.Set(connection->GetName(), std::move(connection));
}

bool SocketServer::RemoveConnection(Name clientName)
{
    Mutex::Guard guard(m_connectionsMutex);

    const auto it = m_connections.Find(clientName);

    if (it == m_connections.End())
    {
        return false;
    }

    if (it->second != nullptr)
    {
        it->second->TriggerProc(NAME("OnClientDisconnected"), { SocketProcArgument(it->second->GetName()) });

        it->second->Close();
    }

    m_connections.Erase(it);

    return true;
}

SocketResultType SocketServer::Send(Name clientName, const ByteBuffer& data)
{
    Mutex::Guard guard(m_connectionsMutex);

    const auto it = m_connections.Find(clientName);

    if (it == m_connections.End())
    {
        return SOCKET_RESULT_TYPE_ERROR;
    }

    if (it->second == nullptr)
    {
        return SOCKET_RESULT_TYPE_ERROR;
    }

    return it->second->Send(data);
}

#pragma endregion SocketServer

#pragma region SocketServerThread

SocketServerThread::SocketServerThread(const String& socketName)
    : Thread(ThreadId(CreateNameFromDynamicString(ANSIString("SocketServerThread_") + socketName.Data())))
{
}

void SocketServerThread::operator()(SocketServer* server)
{
    Queue<Scheduler::ScheduledTask> tasks;

    while (!m_stopRequested.Load())
    {
        // Check for incoming connections

        Array<RC<SocketClient>> newConnections;

        if (server->PollForConnections(newConnections))
        {
            for (auto& connection : newConnections)
            {
                server->AddConnection(std::move(connection));
            }
        }

        Array<RC<SocketClient>> removedConnections;

        { // Check for incoming data
            ByteBuffer receivedData;

            Mutex::Guard guard(server->m_connectionsMutex);

            for (auto& pair : server->m_connections)
            {
                if (pair.second == nullptr)
                {
                    continue;
                }

                const SocketResultType result = pair.second->Receive(receivedData);

                switch (result)
                {
                case SOCKET_RESULT_TYPE_DATA:
                    // Trigger the OnClientData event
                    pair.second->TriggerProc(NAME("OnClientData"), { SocketProcArgument(pair.second->GetName()), SocketProcArgument(std::move(receivedData)) });

                    break;
                case SOCKET_RESULT_TYPE_ERROR:
                    // Trigger the OnError event
                    pair.second->TriggerProc(NAME("OnClientError"), { SocketProcArgument(pair.second->GetName()) });

                    break;
                case SOCKET_RESULT_TYPE_NO_DATA:
                    // No data returned, do nothing
                    break;
                case SOCKET_RESULT_TYPE_DISCONNECTED:
                    removedConnections.PushBack(std::move(pair.second));

                    break;
                }
            }
        }

        // Mutex is unlocked here, so we can remove the connections
        for (auto& connection : removedConnections)
        {
            server->RemoveConnection(connection->GetName());
        }

        if (uint32 numEnqueued = m_scheduler.NumEnqueued())
        {
            m_scheduler.AcceptAll(tasks);

            while (tasks.Any())
            {
                tasks.Pop().Execute();
            }
        }
    }

    // flush scheduler
    m_scheduler.Flush([](auto& operation)
        {
            operation.Execute();
        });
}

#pragma endregion SocketServerThread

#pragma region SocketClient

SocketClient::SocketClient(Name name, SocketID internalId)
    : m_name(name),
      m_internalId(internalId)
{
}

SocketResultType SocketClient::Send(const ByteBuffer& data)
{
    if (m_internalId.value == 0)
    {
        return SOCKET_RESULT_TYPE_ERROR;
    }

    if (data.Size() == 0)
    {
        return SOCKET_RESULT_TYPE_NO_DATA;
    }

#if defined(HYP_UNIX)
    const int32 sent = send(m_internalId.value, data.Data(), data.Size(), 0);

    if (sent == -1)
    {
        return SOCKET_RESULT_TYPE_ERROR;
    }

    return SOCKET_RESULT_TYPE_DATA;
#else
    return SOCKET_RESULT_TYPE_ERROR;
#endif
}

SocketResultType SocketClient::Receive(ByteBuffer& outData)
{
    if (m_internalId.value == 0)
    {
        return SOCKET_RESULT_TYPE_ERROR;
    }

#if defined(HYP_UNIX)
    uint32 dataSize;
    SizeType received = recv(m_internalId.value, &dataSize, sizeof(dataSize), MSG_WAITALL);

    if (received < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return SOCKET_RESULT_TYPE_NO_DATA;
        }
        else
        {
            // other error
            return SOCKET_RESULT_TYPE_ERROR;
        }
    }
    else if (received == 0)
    {
        // connection closed (TODO: handle)
        return SOCKET_RESULT_TYPE_DISCONNECTED;
    }

    if (dataSize == 0)
    {
        return SOCKET_RESULT_TYPE_NO_DATA;
    }

    outData.SetSize(dataSize);

    received = recv(m_internalId.value, outData.Data(), dataSize, MSG_WAITALL);

    // TODO: handle partial data

    return received == dataSize ? SOCKET_RESULT_TYPE_DATA : SOCKET_RESULT_TYPE_ERROR;
#else
    return SOCKET_RESULT_TYPE_ERROR;
#endif
}

void SocketClient::Close()
{
#if defined(HYP_UNIX)
    if (m_internalId.value != 0)
    {
        close(m_internalId.value);
    }
#endif

    m_internalId.value = 0;
}

#pragma endregion SocketClient

} // namespace net
} // namespace Hyperion