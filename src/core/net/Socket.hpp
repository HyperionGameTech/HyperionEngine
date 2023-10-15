#ifndef HYPERION_CORE_NET_SOCKET_HPP
#define HYPERION_CORE_NET_SOCKET_HPP

#include <core/lib/Variant.hpp>
#include <core/lib/DynArray.hpp>
#include <core/lib/UniquePtr.hpp>
#include <core/lib/Proc.hpp>
#include <core/lib/HashMap.hpp>
#include <core/Name.hpp>
#include <util/Defines.hpp>
#include <Types.hpp>

namespace hyperion::net {

struct OpaqueSocketID
{

};

class Socket
{
public:
    OpaqueSocketID GetSocketID() const
        { return m_id; }

private:
    OpaqueSocketID m_id;
};

struct SocketServerImpl;

using SocketProcArgument = Variant<String, Int8, Int16, Int32, Int64, UInt8, UInt16, UInt32, UInt64, Float32, Float64>;

class SocketConnection
{
public:
    virtual ~SocketConnection() = default;

    void SetEventProc(Name event_name, Proc<void, Array<SocketProcArgument> &&> &&proc)
    {
        m_event_procs[event_name] = std::move(proc);
    }

protected:
    HashMap<Name, Proc<void, Array<SocketProcArgument> &&>> m_event_procs;

    void TriggerProc(Name event_name, Array<SocketProcArgument> &&args);
};

class SocketServer : public SocketConnection
{
public:
    SocketServer();
    virtual ~SocketServer() override;

    bool Start();

private:
    UniquePtr<SocketServerImpl> m_impl;
};

} // namespace hyperion::net

#endif