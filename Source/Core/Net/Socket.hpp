/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

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

#include <Core/Defines.hpp>

#include <Core/Types.hpp>

namespace Hyperion {
namespace net {

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

using SocketProcArgument = Variant<String, ByteBuffer, Name, int8, int16, int32, int64, uint8, uint16, uint32, uint64, float, double>;

class CORE_API SocketConnection
{
public:
    SocketConnection() = default;
    SocketConnection(const SocketConnection&) = delete;
    SocketConnection& operator=(const SocketConnection&) = delete;
    SocketConnection(SocketConnection&&) noexcept = delete;
    SocketConnection& operator=(SocketConnection&&) noexcept = delete;
    virtual ~SocketConnection() = default;

    void SetEventProc(Name eventName, Proc<void(Array<SocketProcArgument>&&)>&& proc)
    {
        m_eventProcs[eventName] = std::move(proc);
    }

    void TriggerProc(Name eventName, Array<SocketProcArgument>&& args);

protected:
    TMap<Name, Proc<void(Array<SocketProcArgument>&&)>> m_eventProcs;
};

class CORE_API SocketServerThread final : public Thread<Scheduler, SocketServer*>
{
public:
    SocketServerThread(const String& socketName);
    virtual ~SocketServerThread() override = default;

private:
    virtual void operator()(SocketServer*) override;
};

class CORE_API SocketClient : public SocketConnection
{
public:
    SocketClient(Name name, SocketID internalId);
    SocketClient(const SocketClient&) = delete;
    SocketClient& operator=(const SocketClient&) = delete;
    SocketClient(SocketClient&&) noexcept = delete;
    SocketClient& operator=(SocketClient&&) noexcept = delete;
    virtual ~SocketClient() override = default;

    Name GetName() const
    {
        return m_name;
    }

    SocketResultType Send(const ByteBuffer& data);
    SocketResultType Receive(ByteBuffer& outData);

    void Close();

private:
    Name m_name;
    SocketID m_internalId;
};

class CORE_API SocketServer : public SocketConnection
{
public:
    friend class SocketServerThread;

    SocketServer(String name);
    SocketServer(const SocketServer&) = delete;
    SocketServer& operator=(const SocketServer&) = delete;
    SocketServer(SocketServer&&) noexcept = delete;
    SocketServer& operator=(SocketServer&&) noexcept = delete;
    virtual ~SocketServer() override;

    SocketResultType Send(Name clientName, const ByteBuffer& data);

    bool Start();
    bool Stop();

private:
    // for the thread
    bool PollForConnections(Array<SharedPtr<SocketClient>>& outConnections);

    // for the thread
    void AddConnection(SharedPtr<SocketClient>&& connection);
    bool RemoveConnection(Name clientName);

    String m_name;
    UniquePtr<SocketServerImpl> m_impl;
    UniquePtr<SocketServerThread> m_thread;

    // SocketServerThread controls the connections list
    TMap<Name, SharedPtr<SocketClient>> m_connections;
    Mutex m_connectionsMutex;
};

} // namespace net

using net::SocketClient;
using net::SocketConnection;
using net::SocketID;
using net::SocketProcArgument;
using net::SocketResultType;
using net::SocketServer;
using net::SocketServerThread;

} // namespace Hyperion
