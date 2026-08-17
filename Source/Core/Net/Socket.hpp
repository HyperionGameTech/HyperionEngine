/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#if 0

#include <Core/Name/Name.hpp>

#include <Core/Threading/Thread.hpp>
#include <Core/Threading/Mutex.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/String.hpp>
#include <Core/Containers/Map.hpp>

#include <Core/Memory/UniquePtr.hpp>
#include <Core/Memory/SharedPtr.hpp>
#include <Core/Memory/ByteBuffer.hpp>

#include <Core/Functional/Proc.hpp>

#include <Core/Utilities/Variant.hpp>
#include <Core/Utilities/Result.hpp>

#include <Core/Defines.hpp>

#include <Core/Types.hpp>

#include <Core/Net/NetMemory.hpp>

namespace Hyperion {
namespace net {

/*! \brief The maximum payload size (in bytes) of a single frame that can be sent or received over a socket. */
static constexpr uint32 SocketMaxFrameSize = 128 * 1024 * 1024; // 128 MiB

struct SocketID
{
    uint64 value = 0;
};

enum SocketResultType
{
    SOCKET_RESULT_TYPE_NONE,
    SOCKET_RESULT_TYPE_ERROR,
    SOCKET_RESULT_TYPE_DATA,
    SOCKET_RESULT_TYPE_NO_DATA,
    SOCKET_RESULT_TYPE_DISCONNECTED
};

class SocketServer;
struct SocketServerImpl;

using SocketBuffer = memory::ByteBuffer<NetAllocator>;
using SocketError = Error;

using SocketProcArgument = Variant<SocketError, SocketBuffer, Name, int8, int16, int32, int64, uint8, uint16, uint32, uint64, float, double>;

/*! \brief A platform-agnostic host/port pair used to address a TCP socket. */
class CORE_API SocketAddress
{
public:
    SocketAddress();
    SocketAddress(const ANSIString& host, uint16 port);

    /*! \brief Parses a "host:port" string into a SocketAddress. Returns false if the string is invalid. */
    static bool Parse(ANSIStringView str, SocketAddress& outAddress);

    HYP_FORCE_INLINE const ANSIString& GetHost() const
    {
        return m_host;
    }

    HYP_FORCE_INLINE uint16 GetPort() const
    {
        return m_port;
    }

private:
    ANSIString m_host;
    uint16 m_port;
};

class CORE_API SocketConnection
{
public:
    SocketConnection() = default;
    
    SocketConnection(const SocketConnection&) = delete;
    SocketConnection& operator=(const SocketConnection&) = delete;
    
    SocketConnection(SocketConnection&&) noexcept = delete;
    SocketConnection& operator=(SocketConnection&&) noexcept = delete;

    virtual ~SocketConnection() = default;

    void SetEventProc(Name eventName, Proc<void(Array<SocketProcArgument, NetAllocator>&&)>&& proc)
    {
        m_eventProcs[eventName] = std::move(proc);
    }

    void TriggerProc(Name eventName, Array<SocketProcArgument, NetAllocator>&& args);

protected:
    Map<Name, Proc<void(Array<SocketProcArgument, NetAllocator>&&)>, NetAllocator> m_eventProcs;
};

class CORE_API SocketServerThread final : public Thread<Scheduler, SocketServer*>
{
public:
    SocketServerThread(const String& socketName);
    virtual ~SocketServerThread() override = default;

private:
    virtual void operator()(SocketServer*) override;
};

using SocketClientPtr = UniquePtr<class SocketClient, NetAllocator>;

class CORE_API SocketClient : public SocketConnection
{
public:
    SocketClient(Name name, SocketID internalId);
    
    SocketClient(const SocketClient&) = delete;
    SocketClient& operator=(const SocketClient&) = delete;
    
    SocketClient(SocketClient&&) noexcept = delete;
    SocketClient& operator=(SocketClient&&) noexcept = delete;

    virtual ~SocketClient() override;

    /*! \brief Connects to a remote host:port, creating a new heap-allocated SocketClient that the caller owns.
     *  On success, \p outClient is set and SOCKET_RESULT_TYPE_DATA is returned. */
    static SocketResultType Connect(const ANSIString& host, uint16 port, SocketClientPtr& outClient);

    HYP_FORCE_INLINE Name GetName() const
    {
        return m_name;
    }

    HYP_FORCE_INLINE bool IsOpen() const
    {
        return m_internalId.value != 0;
    }

    SocketResultType Send(ConstByteView view);
    SocketResultType Receive(SocketBuffer& outData);

    /*! \brief Blocks the calling thread (via select()) until the socket has data to read or
     *  \p timeoutMs elapses, instead of busy-waiting. Returns false on timeout. */
    bool WaitForData(uint32 timeoutMs) const;

    /*! \brief Flushes any buffered outgoing data to the socket. Called by the server thread for server-side clients. */
    void Flush();

    void Close();

private:
    friend class SocketServer;
    friend class SocketServerThread;

    SocketResultType TryDequeueFrame(SocketBuffer& outData);

    Name m_name;
    SocketID m_internalId;

    // Guards Send/Receive/Flush/Close: on the server side, a worker thread can call Send() (to
    // respond to a request) at the same time the poll thread calls Flush()/Receive() on the same
    // connection, and both touch the send/recv buffers and the raw handle.
    mutable Mutex m_ioMutex;

    SocketBuffer m_recvBuffer; //!< Accumulated incoming bytes, used for frame assembly.
    SocketBuffer m_sendBuffer; //!< Outgoing bytes waiting to be flushed to the socket.
};

class CORE_API SocketServer : public SocketConnection
{
public:
    friend class SocketServerThread;

    SocketServer(uint16 port, const ANSIString& host = ANSIString("0.0.0.0"), const String& name = String::empty);
    
    SocketServer(const SocketServer&) = delete;
    SocketServer& operator=(const SocketServer&) = delete;
    
    SocketServer(SocketServer&&) noexcept = delete;
    SocketServer& operator=(SocketServer&&) noexcept = delete;

    virtual ~SocketServer() override;

    HYP_FORCE_INLINE const SocketAddress& GetAddress() const
    {
        return m_address;
    }

    SocketResultType Send(Name clientName, ConstByteView view);

    bool Start();
    bool Stop();

private:
    // for the thread
    bool PollForConnections(Array<SharedPtr<SocketClient>, NetAllocator>& outConnections);

    // for the thread
    void AddConnection(SharedPtr<SocketClient>&& connection);
    bool RemoveConnection(Name clientName);

    SocketAddress m_address;
    String m_name;

    UniquePtr<SocketServerImpl, NetAllocator> m_impl;
    UniquePtr<SocketServerThread, NetAllocator> m_thread;

    // SocketServerThread controls the connections list
    Map<Name, SharedPtr<SocketClient>, NetAllocator> m_connections;
    Mutex m_connectionsMutex;
};

} // namespace net

using net::SocketAddress;
using net::SocketClient;
using net::SocketClientPtr;
using net::SocketConnection;
using net::SocketID;
using net::SocketError;
using net::SocketBuffer;
using net::SocketMaxFrameSize;
using net::SocketProcArgument;
using net::SocketResultType;
using net::SocketServer;
using net::SocketServerThread;

} // namespace Hyperion


#endif