/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#include <rtc/RTCClientList.hpp>
#include <rtc/RTCClient.hpp>

namespace hyperion {

void RTCClientList::Add(const String& id, RC<RTCClient> client)
{
    std::lock_guard guard(m_mutex);

    m_clients[id] = std::move(client);
}

void RTCClientList::Remove(String id)
{
    std::lock_guard guard(m_mutex);

    m_clients.Erase(id);
}

Optional<RC<RTCClient>> RTCClientList::Get(const String& id) const
{
    std::lock_guard guard(m_mutex);

    const auto it = m_clients.Find(id);

    if (it == m_clients.End())
    {
        return Optional<RC<RTCClient>>();
    }

    return it->second;
}

bool RTCClientList::Has(const String& id) const
{
    std::lock_guard guard(m_mutex);

    const auto it = m_clients.Find(id);

    return it != m_clients.End();
}

} // namespace hyperion