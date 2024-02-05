#ifndef HYPERION_CORE_NET_SOCKET_HPP
#define HYPERION_CORE_NET_SOCKET_HPP

#include <core/Name.hpp>
#include <core/Thread.hpp>
#include <core/Scheduler.hpp>
#include <core/lib/AtomicVar.hpp>
#include <core/lib/Variant.hpp>
#include <core/lib/DynArray.hpp>
#include <core/lib/String.hpp>
#include <core/lib/UniquePtr.hpp>
#include <core/lib/Proc.hpp>
#include <core/lib/Mutex.hpp>
#include <core/lib/FlatMap.hpp>
#include <core/lib/HashMap.hpp>
#include <core/lib/ByteBuffer.hpp>
#include <util/Defines.hpp>
#include <Types.hpp>

namespace hyperion::net {

struct SocketID
{
    int value;
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

using SocketProcArgument = Variant<String, ByteBuffer, Name, Int8, Int16, Int32, Int64, UInt8, UInt16, UInt32, UInt64, float, double>;

class SocketConnection
{
public:
    virtual ~SocketConnection() = default;

    void SetEventProc(Name event_name, Proc<void, Array<SocketProcArgument> &&> &&proc)
    {
        m_event_procs[event_name] = std::move(proc);
    }

    void TriggerProc(Name event_name, Array<SocketProcArgument> &&args);

protected:
    HashMap<Name, Proc<void, Array<SocketProcArgument> &&>> m_event_procs;

};

using hyperion::v2::Thread;
using hyperion::v2::Scheduler;
using hyperion::v2::Task;

class SocketServerThread final : public Thread<Scheduler<Task<void>>, SocketServer *>
{
public:
    SocketServerThread(const String &socket_name);
    virtual ~SocketServerThread() override = default;

    void Stop();

    bool IsRunning() const
        { return m_is_running.Get(MemoryOrder::RELAXED); }

private:
    virtual void operator()(SocketServer *) override;

    AtomicVar<Bool> m_is_running;
};

class SocketClient : public SocketConnection
{
public:
    SocketClient(Name name, SocketID internal_id);
    SocketClient(const SocketClient &)                  = delete;
    SocketClient &operator=(const SocketClient &)       = delete;
    SocketClient(SocketClient &&) noexcept              = delete;
    SocketClient &operator=(SocketClient &&) noexcept   = delete;
    virtual ~SocketClient() override                    = default;

    Name GetName() const
        { return m_name; }

    SocketResultType Send(const ByteBuffer &data);
    SocketResultType Receive(ByteBuffer &out_data);

    void Close();

private:
    Name        m_name;
    SocketID    m_internal_id;
};

class SocketServer : public SocketConnection
{
public:
    friend class SocketServerThread;

    SocketServer(String name);
    SocketServer(const SocketServer &)                  = delete;
    SocketServer &operator=(const SocketServer &)       = delete;
    SocketServer(SocketServer &&) noexcept              = delete;
    SocketServer &operator=(SocketServer &&) noexcept   = delete;
    virtual ~SocketServer() override;

    SocketResultType Send(Name client_name, const ByteBuffer &data);

    bool Start();
    bool Stop();

private:
    // for the thread
    bool PollForConnections(Array<UniquePtr<SocketClient>> &out_connections);

    // for the thread
    void AddConnection(UniquePtr<SocketClient> &&connection);
    bool RemoveConnection(Name client_name);

    String                                  m_name;
    UniquePtr<SocketServerImpl>             m_impl;
    UniquePtr<SocketServerThread>           m_thread;

    // SocketServerThread controls the connections list
    FlatMap<Name, UniquePtr<SocketClient>>  m_connections;
    Mutex                                   m_connections_mutex;
};

} // namespace hyperion::net

#endif