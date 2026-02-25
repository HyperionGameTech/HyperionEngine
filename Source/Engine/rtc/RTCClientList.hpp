/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/containers/String.hpp>
#include <Core/containers/FlatMap.hpp>
#include <Core/memory/RefCountedPtr.hpp>
#include <Core/utilities/Optional.hpp>

#include <mutex>

namespace Hyperion {

class RTCClient;

class HYP_API RTCClientList
{
public:
    using Iterator = typename FlatMap<String, RC<RTCClient>>::Iterator;
    using ConstIterator = typename FlatMap<String, RC<RTCClient>>::ConstIterator;

    RTCClientList() = default;
    RTCClientList(const RTCClientList& other) = delete;
    RTCClientList& operator=(const RTCClientList& other) = delete;
    RTCClientList(RTCClientList&& other) noexcept = delete;
    RTCClientList& operator=(RTCClientList&& other) noexcept = delete;
    ~RTCClientList() = default;

    void Add(const String& id, RC<RTCClient> client);
    void Remove(String id);
    Optional<RC<RTCClient>> Get(const String& id) const;
    bool Has(const String& id) const;

    HYP_DEF_STL_BEGIN_END(m_clients.Begin(), m_clients.End())

private:
    mutable std::mutex m_mutex;

    FlatMap<String, RC<RTCClient>> m_clients;
};

} // namespace Hyperion
