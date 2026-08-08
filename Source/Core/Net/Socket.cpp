/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <Core/Net/Socket.hpp>

#include <Core/Containers/Queue.hpp>

#include <Core/Logging/LogChannels.hpp>
#include <Core/Logging/Logger.hpp>

#include <Core/Memory/Memory.hpp>

#include <Core/Threading/Threads.hpp>

#include <cstdio>
#include <cstdlib>

#if defined(HYP_WINDOWS)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#else

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#endif

namespace Hyperion {

namespace net {

#pragma region Platform helpers

namespace {

#if defined(HYP_WINDOWS)
using SocketHandle = SOCKET;
static constexpr SocketHandle InvalidSocket = INVALID_SOCKET;
#else
using SocketHandle = int;
static constexpr SocketHandle InvalidSocket = -1;
#endif

static constexpr int SocketErrorValue = -1;

int SocketLastError()
{
#if defined(HYP_WINDOWS)
    return int(WSAGetLastError());
#else
    return errno;
#endif
}

bool SocketWouldBlock(int errorCode)
{
#if defined(HYP_WINDOWS)
    return errorCode == WSAEWOULDBLOCK;
#else
    return errorCode == EAGAIN || errorCode == EWOULDBLOCK;
#endif
}

void SocketClose(SocketHandle handle)
{
#if defined(HYP_WINDOWS)
    closesocket(handle);
#else
    close(handle);
#endif
}

bool SocketSetNonBlocking(SocketHandle handle)
{
#if defined(HYP_WINDOWS)
    u_long nonBlocking = 1;

    return ioctlsocket(handle, FIONBIO, &nonBlocking) == 0;
#else
    int flags = fcntl(handle, F_GETFL, 0);

    if (flags == -1)
    {
        return false;
    }

    return fcntl(handle, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}

bool SocketWait(const SocketHandle& handle, int timeoutMs, bool waitWrite)
{
    fd_set set;
    FD_ZERO(&set);
    FD_SET(handle, &set);

    struct timeval tv = {};
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;

    const int result = waitWrite
        ? select(int(handle) + 1, nullptr, &set, nullptr, &tv)
        : select(int(handle) + 1, &set, nullptr, nullptr, &tv);

    return result > 0 && FD_ISSET(handle, &set);
}

/*! \brief Sends as many bytes as possible without blocking. Returns the number of bytes sent.
 *  \p outWouldBlock is set if the call stopped because the socket send buffer is full. */
size_t SendBytes(const SocketHandle& handle, const ubyte* data, size_t size, bool* outWouldBlock, bool* outError)
{
    *outWouldBlock = false;
    *outError = false;

    size_t offset = 0;

    while (offset < size)
    {
        const size_t remaining = size - offset;
        const int chunk = int(remaining > (1u << 20) ? (1u << 20) : remaining);

        const int sent = send(handle, reinterpret_cast<const char*>(data + offset), chunk, 0);

        if (sent == SocketErrorValue)
        {
            const int errorCode = SocketLastError();

            if (SocketWouldBlock(errorCode))
            {
                *outWouldBlock = true;
            }
            else
            {
                *outError = true;
            }

            return offset;
        }

        offset += size_t(sent);
    }

    return offset;
}

// WSAStartup/WSACleanup is reference-counted across all socket usage in this module.
struct SocketGlobalState
{
    static SocketGlobalState& GetInstance()
    {
        static SocketGlobalState instance;

        return instance;
    }

    bool Acquire()
    {
        Mutex::Guard guard(m_mutex);

        if (m_refCount++ == 0)
        {
#if defined(HYP_WINDOWS)
            WSADATA wsaData;

            if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
            {
                --m_refCount;

                return false;
            }
#endif
        }

        return true;
    }

    void Release()
    {
        Mutex::Guard guard(m_mutex);

        if (--m_refCount == 0)
        {
#if defined(HYP_WINDOWS)
            WSACleanup();
#endif
        }
    }

    Mutex m_mutex;
    int m_refCount = 0;
};

} // anonymous namespace

#pragma endregion Platform helpers

#pragma region SocketAddress

SocketAddress::SocketAddress()
    : m_host("0.0.0.0"),
      m_port(0)
{
}

SocketAddress::SocketAddress(const ANSIString& host, uint16 port)
    : m_host(host),
      m_port(port)
{
}

bool SocketAddress::Parse(ANSIStringView str, SocketAddress& outAddress)
{
    if (str.Size() == 0)
    {
        return false;
    }

    const size_t colonPos = str.FindLastIndex(":");

    if (colonPos == ANSIString::NotFound || colonPos + 1 >= str.Size())
    {
        return false;
    }

    ANSIString host(str.Substr(0, colonPos));
    ANSIString portStr(str.Substr(colonPos + 1, SIZE_MAX));

    char* endPtr = nullptr;
    const long port = std::strtol(portStr.Data(), &endPtr, 10);

    if (endPtr == portStr.Data() || port <= 0 || port > 65535)
    {
        return false;
    }

    outAddress.m_host = std::move(host);
    outAddress.m_port = uint16(port);

    return true;
}

#pragma endregion SocketAddress

struct SocketServerImpl
{
    ~SocketServerImpl()
    {
        if (listenSocket != InvalidSocket)
        {
            SocketClose(listenSocket);
            listenSocket = InvalidSocket;
        }
    }

    SocketHandle listenSocket = InvalidSocket;
    SocketAddress address;
};

#pragma region SocketServer

SocketServer::SocketServer(uint16 port, const ANSIString& host, const String& name)
    : m_address(host, port),
      m_name(name),
      m_impl(nullptr)
{
    if (m_name.Empty())
    {
        char nameBuf[128];
        std::snprintf(nameBuf, sizeof(nameBuf), "SocketServer_%s:%u", host.Data(), uint32(port));

        m_name = String(nameBuf);
    }
}

SocketServer::~SocketServer()
{
    if (m_impl != nullptr)
    {
        Stop();
    }
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
    if (m_impl != nullptr)
    {
        return false;
    }

    if (!SocketGlobalState::GetInstance().Acquire())
    {
        TriggerProc(NAME("OnError"), { SocketProcArgument(String("Failed to initialize sockets")), SocketProcArgument(int32(SocketLastError())) });

        return false;
    }

    char portStr[16];
    std::snprintf(portStr, sizeof(portStr), "%u", uint32(m_address.GetPort()));

    struct addrinfo hints = {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo* results = nullptr;

    if (getaddrinfo(m_address.GetHost().Data(), portStr, &hints, &results) != 0)
    {
        const int32 errorCode = SocketLastError();

        TriggerProc(NAME("OnError"), { SocketProcArgument(String("Failed to resolve listen address")), SocketProcArgument(errorCode) });

        SocketGlobalState::GetInstance().Release();

        return false;
    }

    SocketHandle listenSocket = InvalidSocket;

    for (struct addrinfo* rp = results; rp != nullptr; rp = rp->ai_next)
    {
        listenSocket = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);

        if (listenSocket == InvalidSocket)
        {
            continue;
        }

        int32 reuseSocket = 1;

        setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuseSocket, sizeof(reuseSocket));

#if !defined(HYP_WINDOWS)
        setsockopt(listenSocket, SOL_SOCKET, SO_REUSEPORT, &reuseSocket, sizeof(reuseSocket));
#endif

        if (bind(listenSocket, rp->ai_addr, int(rp->ai_addrlen)) == 0)
        {
            break;
        }

        SocketClose(listenSocket);
        listenSocket = InvalidSocket;
    }

    freeaddrinfo(results);

    if (listenSocket == InvalidSocket)
    {
        const int32 errorCode = SocketLastError();

        TriggerProc(NAME("OnError"), { SocketProcArgument(String("Failed to bind socket")), SocketProcArgument(errorCode) });

        SocketGlobalState::GetInstance().Release();

        return false;
    }

    if (listen(listenSocket, 16) != 0)
    {
        const int32 errorCode = SocketLastError();

        SocketClose(listenSocket);

        TriggerProc(NAME("OnError"), { SocketProcArgument(String("Failed to listen on socket")), SocketProcArgument(errorCode) });

        SocketGlobalState::GetInstance().Release();

        return false;
    }

    if (!SocketSetNonBlocking(listenSocket))
    {
        const int32 errorCode = SocketLastError();

        SocketClose(listenSocket);

        TriggerProc(NAME("OnError"), { SocketProcArgument(String("Failed to set socket to non-blocking")), SocketProcArgument(errorCode) });

        SocketGlobalState::GetInstance().Release();

        return false;
    }

    m_impl = MakeUnique<SocketServerImpl>();
    m_impl->listenSocket = listenSocket;
    m_impl->address = m_address;

    TriggerProc(NAME("OnServerStarted"), {});

    m_thread = MakeUnique<SocketServerThread>(m_name);
    m_thread->Start(this);

    return true;
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

    m_impl.Reset();

    TriggerProc(NAME("OnServerStopped"), {});

    SocketGlobalState::GetInstance().Release();

    return true;
}

bool SocketServer::PollForConnections(Array<SharedPtr<SocketClient>>& outConnections)
{
    if (m_impl == nullptr)
    {
        return false;
    }

    outConnections.Clear();

    for (;;)
    {
        const SocketHandle newSocket = accept(m_impl->listenSocket, nullptr, nullptr);

        if (newSocket == InvalidSocket)
        {
#if !defined(HYP_WINDOWS)
            const int errorCode = SocketLastError();

            if (errorCode == EINTR)
            {
                continue;
            }
#endif

            break;
        }

        if (!SocketSetNonBlocking(newSocket))
        {
            SocketClose(newSocket);

            continue;
        }

        if (!SocketGlobalState::GetInstance().Acquire())
        {
            SocketClose(newSocket);

            continue;
        }

        const Name clientName = Name::Unique("socket_client");

        outConnections.PushBack(MakeShared<SocketClient>(clientName, SocketID { uint64(newSocket) }));
    }

    return true;
}

void SocketServer::AddConnection(SharedPtr<SocketClient>&& connection)
{
    if (!connection)
    {
        return;
    }

    {
        Mutex::Guard guard(m_connectionsMutex);

        m_connections.Set(connection->GetName(), connection);
    }

    connection->TriggerProc(NAME("OnClientConnected"), { SocketProcArgument(connection->GetName()) });
}

bool SocketServer::RemoveConnection(Name clientName)
{
    SharedPtr<SocketClient> removedConnection;

    {
        Mutex::Guard guard(m_connectionsMutex);

        const auto it = m_connections.Find(clientName);

        if (it == m_connections.End())
        {
            return false;
        }

        removedConnection = std::move(it->second);

        m_connections.Erase(it);
    }

    if (removedConnection != nullptr)
    {
        removedConnection->TriggerProc(NAME("OnClientDisconnected"), { SocketProcArgument(removedConnection->GetName()) });

        removedConnection->Close();
    }

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
    struct PendingDataEvent
    {
        SharedPtr<SocketClient> client;
        ByteBuffer data;
    };

    Queue<Scheduler::ScheduledTask> tasks;

    while (HYP_LIKELY(!m_stopRequested.LoadVolatile()))
    {
        // Check for incoming connections
        Array<SharedPtr<SocketClient>> newConnections;

        if (server->PollForConnections(newConnections))
        {
            for (auto& connection : newConnections)
            {
                server->AddConnection(std::move(connection));
            }
        }

        Array<PendingDataEvent> pendingDataEvents;
        Array<SharedPtr<SocketClient>> pendingErrorEvents;
        Array<SharedPtr<SocketClient>> disconnectedConnections;

        { // Check for incoming data
            Mutex::Guard guard(server->m_connectionsMutex);

            for (auto& pair : server->m_connections)
            {
                if (pair.second == nullptr)
                {
                    continue;
                }

                pair.second->Flush();

                ByteBuffer receivedData;

                switch (pair.second->Receive(receivedData))
                {
                case SOCKET_RESULT_TYPE_DATA:
                    pendingDataEvents.PushBack(PendingDataEvent { pair.second, std::move(receivedData) });

                    break;
                case SOCKET_RESULT_TYPE_ERROR:
                    pendingErrorEvents.PushBack(pair.second);

                    break;
                case SOCKET_RESULT_TYPE_NO_DATA:
                    // No data returned, do nothing
                    break;
                case SOCKET_RESULT_TYPE_DISCONNECTED:
                    disconnectedConnections.PushBack(pair.second);

                    break;
                default:
                    break;
                }
            }
        }

        // Events are dispatched outside of the connection lock so handlers can safely
        // call back into Send() (e.g. to respond to a request) without deadlocking.
        for (auto& event : pendingDataEvents)
        {
            event.client->TriggerProc(NAME("OnClientData"), { SocketProcArgument(event.client->GetName()), SocketProcArgument(std::move(event.data)) });
        }

        for (auto& connection : pendingErrorEvents)
        {
            connection->TriggerProc(NAME("OnClientError"), { SocketProcArgument(connection->GetName()) });
        }

        for (auto& connection : disconnectedConnections)
        {
            server->RemoveConnection(connection->GetName());
        }

        if (uint32 numEnqueued = m_scheduler->NumEnqueued())
        {
            m_scheduler->AcceptAll(tasks);

            while (tasks.Any())
            {
                tasks.Pop().Execute();
            }
        }

        // Pace the poll loop so it does not spin hot when idle.
        ThreadSleep(1);
    }

    // flush scheduler
    m_scheduler->Flush([](auto& operation)
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

SocketClient::~SocketClient()
{
    Close();
}

SocketResultType SocketClient::Connect(const ANSIString& host, uint16 port, SocketClient** outClient)
{
    if (outClient == nullptr)
    {
        return SOCKET_RESULT_TYPE_ERROR;
    }

    *outClient = nullptr;

    if (!SocketGlobalState::GetInstance().Acquire())
    {
        return SOCKET_RESULT_TYPE_ERROR;
    }

    char portStr[16];
    std::snprintf(portStr, sizeof(portStr), "%u", uint32(port));

    struct addrinfo hints = {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo* results = nullptr;

    if (getaddrinfo(host.Data(), portStr, &hints, &results) != 0)
    {
        SocketGlobalState::GetInstance().Release();

        return SOCKET_RESULT_TYPE_ERROR;
    }

    SocketHandle sock = InvalidSocket;

    for (struct addrinfo* rp = results; rp != nullptr; rp = rp->ai_next)
    {
        sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);

        if (sock == InvalidSocket)
        {
            continue;
        }

        SocketSetNonBlocking(sock);

        if (connect(sock, rp->ai_addr, int(rp->ai_addrlen)) == 0)
        {
            break;
        }

#if defined(HYP_WINDOWS)
        const bool connectInProgress = SocketWouldBlock(SocketLastError());
#else
        const int connectError = SocketLastError();
        const bool connectInProgress = (connectError == EINPROGRESS)
            || (connectError == EAGAIN)
            || (connectError == EWOULDBLOCK);
#endif

        if (connectInProgress)
        {
            // Non-blocking connect is in progress; wait for it to complete.
            if (SocketWait(sock, 10000, /* waitWrite */ true))
            {
                int socketError = 0;
                socklen_t optionLength = sizeof(socketError);

                getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&socketError, &optionLength);

                if (socketError == 0)
                {
                    break;
                }
            }
        }

        SocketClose(sock);
        sock = InvalidSocket;
    }

    freeaddrinfo(results);

    if (sock == InvalidSocket)
    {
        SocketGlobalState::GetInstance().Release();

        return SOCKET_RESULT_TYPE_ERROR;
    }

    *outClient = new SocketClient(Name::Unique("socket_client_conn"), SocketID { uint64(sock) });

    return SOCKET_RESULT_TYPE_DATA;
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

    if (data.Size() > SocketMaxFrameSize)
    {
        return SOCKET_RESULT_TYPE_ERROR;
    }

    const SocketHandle handle = SocketHandle(m_internalId.value);

    ByteBuffer frame;
    frame.SetSize(sizeof(uint32) + data.Size());

    const uint32 lengthBe = htonl(uint32(data.Size()));

    Memory::Copy(frame.Data(), &lengthBe, sizeof(uint32));
    Memory::Copy(frame.Data() + sizeof(uint32), data.Data(), data.Size());

    bool wouldBlock = false;
    bool error = false;

    const size_t sent = SendBytes(handle, frame.Data(), frame.Size(), &wouldBlock, &error);

    if (error)
    {
        return SOCKET_RESULT_TYPE_ERROR;
    }

    if (sent < frame.Size())
    {
        // Buffer the unsent remainder; it will be flushed on the next poll.
        m_sendBuffer.SetData(frame.Size() - sent, frame.Data() + sent);
    }

    return SOCKET_RESULT_TYPE_DATA;
}

SocketResultType SocketClient::TryDequeueFrame(ByteBuffer& outData)
{
    constexpr size_t headerSize = sizeof(uint32);

    while (m_recvBuffer.Size() >= headerSize)
    {
        uint32 lengthBe;

        Memory::Copy(&lengthBe, m_recvBuffer.Data(), headerSize);

        const uint32 length = ntohl(lengthBe);

        if (length > SocketMaxFrameSize)
        {
            return SOCKET_RESULT_TYPE_ERROR;
        }

        const size_t frameSize = headerSize + size_t(length);

        if (m_recvBuffer.Size() < frameSize)
        {
            // Incomplete frame, wait for more data
            return SOCKET_RESULT_TYPE_NO_DATA;
        }

        outData.SetSize(length);
        Memory::Copy(outData.Data(), m_recvBuffer.Data() + headerSize, size_t(length));

        const size_t remaining = m_recvBuffer.Size() - frameSize;

        if (remaining == 0)
        {
            m_recvBuffer.Clear();
        }
        else
        {
            ByteBuffer rest(remaining, m_recvBuffer.Data() + frameSize);

            m_recvBuffer = std::move(rest);
        }

        return SOCKET_RESULT_TYPE_DATA;
    }

    return SOCKET_RESULT_TYPE_NO_DATA;
}

SocketResultType SocketClient::Receive(ByteBuffer& outData)
{
    if (m_internalId.value == 0)
    {
        return SOCKET_RESULT_TYPE_ERROR;
    }

    SocketResultType result = TryDequeueFrame(outData);

    if (result != SOCKET_RESULT_TYPE_NO_DATA)
    {
        return result;
    }

    const SocketHandle handle = SocketHandle(m_internalId.value);

    char recvBuffer[8192];

    for (;;)
    {
        const int received = recv(handle, recvBuffer, int(sizeof(recvBuffer)), 0);

        if (received == SocketErrorValue)
        {
            const int errorCode = SocketLastError();

            if (SocketWouldBlock(errorCode))
            {
                return SOCKET_RESULT_TYPE_NO_DATA;
            }

            return SOCKET_RESULT_TYPE_ERROR;
        }

        if (received == 0)
        {
            // peer closed the connection
            return SOCKET_RESULT_TYPE_DISCONNECTED;
        }

        const size_t oldSize = m_recvBuffer.Size();
        m_recvBuffer.SetSize(oldSize + size_t(received));
        Memory::Copy(m_recvBuffer.Data() + oldSize, recvBuffer, size_t(received));

        result = TryDequeueFrame(outData);

        if (result != SOCKET_RESULT_TYPE_NO_DATA)
        {
            return result;
        }
    }
}

void SocketClient::Flush()
{
    if (m_internalId.value == 0 || m_sendBuffer.Size() == 0)
    {
        return;
    }

    const SocketHandle handle = SocketHandle(m_internalId.value);

    bool wouldBlock = false;
    bool error = false;

    const size_t sent = SendBytes(handle, m_sendBuffer.Data(), m_sendBuffer.Size(), &wouldBlock, &error);

    if (error)
    {
        m_sendBuffer.Clear();

        return;
    }

    if (sent == m_sendBuffer.Size())
    {
        m_sendBuffer.Clear();

        return;
    }

    ByteBuffer remaining(m_sendBuffer.Size() - sent, m_sendBuffer.Data() + sent);

    m_sendBuffer = std::move(remaining);
}

void SocketClient::Close()
{
    if (m_internalId.value == 0)
    {
        return;
    }

    SocketClose(SocketHandle(m_internalId.value));

    m_internalId.value = 0;

    m_recvBuffer.Clear();
    m_sendBuffer.Clear();

    SocketGlobalState::GetInstance().Release();
}

#pragma endregion SocketClient

} // namespace net
} // namespace Hyperion
