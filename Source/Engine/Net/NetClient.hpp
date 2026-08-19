/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Net/NetMemory.hpp>
#include <Core/Net/NetSocketUDP.hpp>
#include <Core/Net/NetAddress.hpp>

#include <Core/Utilities/Result.hpp>
#include <Core/Utilities/Time.hpp>

#include <Core/Functional/Delegate.hpp>

#include <Core/Threading/AtomicVar.hpp>
#include <Core/Threading/Mutex.hpp>

#include <Core/Containers/String.hpp>

namespace Hyperion {
namespace net {

struct NetServerDisconnectedData
{
    NetAddress serverAddress;
};

enum class NetClientConnectionState : uint8
{
    Disconnected,
    Connecting,
    Connected
};

class CORE_API NetClient
{
public:
    NetClient();

    NetClient(const NetClient& other) = delete;
    NetClient& operator=(const NetClient& other) = delete;

    ~NetClient();

    HYP_FORCE_INLINE NetClientConnectionState GetConnectionState() const
    {
        return m_connectionState.Get(MemoryOrder::ACQUIRE);
    }

    HYP_FORCE_INLINE bool IsConnected() const
    {
        return GetConnectionState() == NetClientConnectionState::Connected;
    }

    HYP_FORCE_INLINE const NetAddress& GetServerAddress() const
    {
        return m_serverAddress;
    }

    HYP_FORCE_INLINE Result GetLastError() const
    {
        Mutex::Guard guard(m_lastErrorMutex);

        return m_lastError;
    }

    Result Connect(const NetAddress& serverAddress);
    void Disconnect();

    void Update();

    Delegate<void, const NetServerDisconnectedData&> OnDisconnected;

private:
    NetSocketUDP m_socket;
    NetAddress m_serverAddress;
    AtomicVar<NetClientConnectionState> m_connectionState;
    Time m_lastActivityTime;
    Time m_lastKeepAliveTime;
    Time m_connectStartTime;

    mutable Mutex m_lastErrorMutex;
    Result m_lastError;
};

} // namespace net

using net::NetClient;
using net::NetClientConnectionState;
using net::NetServerDisconnectedData;

} // namespace Hyperion
