#ifndef HYPERION_V2_RTC_CLIENT_LIST_HPP
#define HYPERION_V2_RTC_CLIENT_LIST_HPP

#include <core/lib/String.hpp>
#include <core/lib/FlatMap.hpp>
#include <core/lib/RefCountedPtr.hpp>
#include <core/lib/Optional.hpp>

#include <mutex>

namespace hyperion::v2 {

class RTCClient;

class RTCClientList
{
public:
    using Iterator = typename FlatMap<String, RC<RTCClient>>::Iterator;
    using ConstIterator = typename FlatMap<String, RC<RTCClient>>::ConstIterator;

    RTCClientList() = default;
    RTCClientList(const RTCClientList &other) = delete;
    RTCClientList &operator=(const RTCClientList &other) = delete;
    RTCClientList(RTCClientList &&other) noexcept = delete;
    RTCClientList &operator=(RTCClientList &&other) noexcept = delete;
    ~RTCClientList() = default;

    void Add(const String &id, RC<RTCClient> client);
    void Remove(String id);
    Optional<RC<RTCClient>> Get(const String &id) const;
    bool Has(const String &id) const;

    HYP_DEF_STL_BEGIN_END(
        m_clients.Begin(),
        m_clients.End()
    )

private:
    mutable std::mutex m_mutex;

    FlatMap<String, RC<RTCClient>> m_clients;
};


} // namespace hyperion::v2

#endif