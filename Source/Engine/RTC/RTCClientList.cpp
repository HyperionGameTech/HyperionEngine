/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>
#include <RTC/RTCClientList.hpp>
#include <RTC/RTCClient.hpp>

namespace Hyperion {

void RTCClientList::Add(const String& id, SharedPtr<RTCClient> client)
{
    std::lock_guard guard(m_mutex);

    m_clients[id] = std::move(client);
}

void RTCClientList::Remove(String id)
{
    std::lock_guard guard(m_mutex);

    m_clients.Erase(id);
}

Optional<SharedPtr<RTCClient>> RTCClientList::Get(const String& id) const
{
    std::lock_guard guard(m_mutex);

    const auto it = m_clients.Find(id);

    if (it == m_clients.End())
    {
        return Optional<SharedPtr<RTCClient>>();
    }

    return it->second;
}

bool RTCClientList::Has(const String& id) const
{
    std::lock_guard guard(m_mutex);

    const auto it = m_clients.Find(id);

    return it != m_clients.End();
}

} // namespace Hyperion
