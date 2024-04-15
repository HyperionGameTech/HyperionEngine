/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/net/Socket.hpp>

#include <core/lib/Queue.hpp>

#ifdef HYP_UNIX

#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <fcntl.h>

#endif

namespace hyperion::net {

struct SocketServerImpl
{
    int socket_id = 0;

#ifdef HYP_UNIX
    struct sockaddr_un local, remote;
#endif
};

SocketServer::SocketServer(String name)
    : m_name(std::move(name)),
      m_impl(nullptr)
{
}

SocketServer::~SocketServer()
{
    if (m_impl != nullptr) {
        Stop();
    }

    AssertThrow(m_thread == nullptr && !m_thread->IsRunning());
}

void SocketConnection::TriggerProc(Name event_name, Array<SocketProcArgument> &&args)
{
    const auto it = m_event_procs.Find(event_name);

    if (it == m_event_procs.End()) {
        return;
    }

    it->second(std::move(args));
}

bool SocketServer::Start()
{
    if (m_impl) {
        return false;
    }

    m_impl.Reset(new SocketServerImpl);

#ifdef HYP_UNIX
    m_impl->socket_id = socket(AF_UNIX, SOCK_STREAM, 0);
#endif

    if (m_impl->socket_id == -1) {
        DebugLog(
            LogType::Error,
            "Failed to open socket server. Code: %d\n",
            errno
        );

        TriggerProc(HYP_NAME(OnError), { SocketProcArgument(String("Failed to open socket")), SocketProcArgument(int32(errno)) });

        return false;
    }

    const char *socket_path = m_name.Data();

    constexpr SizeType max_connections = 5;
    
#ifdef HYP_UNIX
    m_impl->local.sun_family = AF_UNIX;
    strcpy(m_impl->local.sun_path, socket_path);
    unlink(m_impl->local.sun_path);

    const int len = strlen(m_impl->local.sun_path) + sizeof(m_impl->local.sun_family);

    int32 reuse_socket = 1;

    if (setsockopt(m_impl->socket_id, SOL_SOCKET, SO_REUSEADDR, &reuse_socket, sizeof(reuse_socket)) != 0) {
        const int32 error_code = errno;

        TriggerProc(HYP_NAME(OnError), { SocketProcArgument(String(strerror(error_code))), SocketProcArgument(error_code) });

        return false;
    }

    if (setsockopt(m_impl->socket_id, SOL_SOCKET, SO_REUSEPORT, &reuse_socket, sizeof(reuse_socket)) != 0) {
        const int32 error_code = errno;

        TriggerProc(HYP_NAME(OnError), { SocketProcArgument(String(strerror(error_code))), SocketProcArgument(error_code) });

        return false;
    }

    if (bind(m_impl->socket_id, (struct sockaddr *)&m_impl->local, len) != 0) {
        const int32 error_code = errno;

        TriggerProc(HYP_NAME(OnError), { SocketProcArgument(String(strerror(error_code))), SocketProcArgument(error_code) });


        return false;
    }

    if (listen(m_impl->socket_id, max_connections) != 0) {
        const int32 error_code = errno;

        TriggerProc(HYP_NAME(OnError), { SocketProcArgument(String(strerror(error_code))), SocketProcArgument(int32(errno)) });

        return false;
    }

    TriggerProc(HYP_NAME(OnServerStarted), { });

    m_thread.Reset(new SocketServerThread(m_name));
    m_thread->Start(this);

    return true;
#endif

    return false;
}

bool SocketServer::Stop()
{
    if (m_impl == nullptr) {
        return false;
    }

    if (m_thread != nullptr) {
        m_thread->Stop();

        if (m_thread->CanJoin()) {
            m_thread->Join();
        }
    }

    { // Close all connections
        Mutex::Guard guard(m_connections_mutex);

        for (auto &pair : m_connections) {
            if (pair.second != nullptr) {
                pair.second->Close();
            }
        }

        m_connections.Clear();
    }

#ifdef HYP_UNIX
    close(m_impl->socket_id);
    unlink(m_impl->local.sun_path);

    TriggerProc(HYP_NAME(OnServerStopped), { });

    m_impl.Reset();

    return true;
#endif

    return false;
}

bool SocketServer::PollForConnections(Array<UniquePtr<SocketClient>> &out_connections)
{
    if (m_impl == nullptr) {
        return false;
    }

    out_connections.Clear();

#if defined(HYP_UNIX)
    // the client
    struct sockaddr_un remote;
    socklen_t t = sizeof(remote);

    int new_socket;

    while ((new_socket = accept(m_impl->socket_id, (struct sockaddr *)&remote, &t)) != -1) {
        const Name client_name = Name::Unique("socket_client");

        // Make the socket non-blocking
        if (fcntl(new_socket, F_SETFL, O_NONBLOCK) == -1) {
            DebugLog(
                LogType::Error,
                "Failed to set socket to non-blocking. Code: %d\n",
                errno
            );

            close(new_socket);
            continue;
        }

        out_connections.PushBack(UniquePtr<SocketClient>(new SocketClient(client_name, SocketID { new_socket })));
    }

    return true;    

#endif

    return false;
}

void SocketServer::AddConnection(UniquePtr<SocketClient> &&connection)
{
    if (!connection) {
        return;
    }

    Mutex::Guard guard(m_connections_mutex);

    connection->TriggerProc(HYP_NAME(OnClientConnected), {
        SocketProcArgument(connection->GetName())
    });

    m_connections.Set(connection->GetName(), std::move(connection));
}

bool SocketServer::RemoveConnection(Name client_name)
{
    Mutex::Guard guard(m_connections_mutex);

    const auto it = m_connections.Find(client_name);

    if (it == m_connections.End()) {
        return false;
    }

    if (it->second != nullptr) {
        it->second->TriggerProc(HYP_NAME(OnClientDisconnected), {
            SocketProcArgument(it->second->GetName())
        });

        it->second->Close();
    }

    m_connections.Erase(it);

    return true;
}

SocketResultType SocketServer::Send(Name client_name, const ByteBuffer &data)
{
    Mutex::Guard guard(m_connections_mutex);

    const auto it = m_connections.Find(client_name);

    if (it == m_connections.End()) {
        return SOCKET_RESULT_TYPE_ERROR;
    }

    if (it->second == nullptr) {
        return SOCKET_RESULT_TYPE_ERROR;
    }

    return it->second->Send(data);
}

// SocketServerThread

SocketServerThread::SocketServerThread(const String &socket_name)
    : Thread(CreateNameFromDynamicString(ANSIString("SocketServerThread_") + socket_name.Data()))
{
}

void SocketServerThread::Stop()
{
    m_is_running.Set(false, MemoryOrder::RELAXED);
}

void SocketServerThread::operator()(SocketServer *server)
{
    m_is_running.Set(true, MemoryOrder::RELAXED);
    
    Queue<Scheduler::ScheduledTask> tasks;

    while (m_is_running.Get(MemoryOrder::RELAXED)) {
        // Check for incoming connections

        Array<UniquePtr<SocketClient>> new_connections;

        if (server->PollForConnections(new_connections)) {
            for (auto &connection : new_connections) {
                server->AddConnection(std::move(connection));
            }
        }

        Array<UniquePtr<SocketClient>> removed_connections;

        { // Check for incoming data
            ByteBuffer received_data;

            Mutex::Guard guard(server->m_connections_mutex);

            for (auto &pair : server->m_connections) {
                if (pair.second == nullptr) {
                    continue;
                }

                const SocketResultType result = pair.second->Receive(received_data);

                switch (result) {
                case SOCKET_RESULT_TYPE_DATA:
                    // Trigger the OnClientData event
                    pair.second->TriggerProc(HYP_NAME(OnClientData), {
                        SocketProcArgument(pair.second->GetName()),
                        SocketProcArgument(std::move(received_data))
                    });

                    break;
                case SOCKET_RESULT_TYPE_ERROR:
                    // Trigger the OnError event
                    pair.second->TriggerProc(HYP_NAME(OnClientError), {
                        SocketProcArgument(pair.second->GetName())
                    });

                    break;
                case SOCKET_RESULT_TYPE_NO_DATA:
                    // No data returned, do nothing
                    break;
                case SOCKET_RESULT_TYPE_DISCONNECTED:
                    removed_connections.PushBack(std::move(pair.second));

                    break;
                }
            }
        }

        // Mutex is unlocked here, so we can remove the connections
        for (auto &connection : removed_connections) {
            server->RemoveConnection(connection->GetName());
        }

        if (auto num_enqueued = m_scheduler.NumEnqueued()) {
            m_scheduler.AcceptAll(tasks);

            while (tasks.Any()) {
                tasks.Pop().Execute();
            }
        }
    }

    // flush scheduler
    m_scheduler.Flush([](auto &fn) {
        fn();
    });
}

// SocketClient

SocketClient::SocketClient(Name name, SocketID internal_id)
    : m_name(name),
      m_internal_id(internal_id)
{
}

SocketResultType SocketClient::Send(const ByteBuffer &data)
{
    if (m_internal_id.value == 0) {
        return SOCKET_RESULT_TYPE_ERROR;
    }

    if (data.Size() == 0) {
        return SOCKET_RESULT_TYPE_NO_DATA;
    }

#if defined(HYP_UNIX)
    const int32 sent = send(m_internal_id.value, data.Data(), data.Size(), 0);

    if (sent == -1) {
        return SOCKET_RESULT_TYPE_ERROR;
    }

    return SOCKET_RESULT_TYPE_DATA;
#else
    return SOCKET_RESULT_TYPE_ERROR;
#endif
}

SocketResultType SocketClient::Receive(ByteBuffer &out_data)
{
    if (m_internal_id.value == 0) {
        return SOCKET_RESULT_TYPE_ERROR;
    }

#if defined(HYP_UNIX)
    uint32 data_size;
    SizeType received = recv(m_internal_id.value, &data_size, sizeof(data_size), MSG_WAITALL);

    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return SOCKET_RESULT_TYPE_NO_DATA;
        } else {
            // other error
            return SOCKET_RESULT_TYPE_ERROR;
        }
    } else if (received == 0) {
        // connection closed (TODO: handle)
        return SOCKET_RESULT_TYPE_DISCONNECTED;
    }

    if (data_size == 0) {
        return SOCKET_RESULT_TYPE_NO_DATA;
    }

    out_data.SetSize(data_size);

    received = recv(m_internal_id.value, out_data.Data(), data_size, MSG_WAITALL);

    // TODO: handle partial data

    return received == data_size ? SOCKET_RESULT_TYPE_DATA : SOCKET_RESULT_TYPE_ERROR;
#else
    return SOCKET_RESULT_TYPE_ERROR;
#endif
}

void SocketClient::Close()
{
#if defined(HYP_UNIX)
    if (m_internal_id.value != 0) {
        close(m_internal_id.value);
    }
#endif

    m_internal_id.value = 0;
}

} // namespace hyperion::net