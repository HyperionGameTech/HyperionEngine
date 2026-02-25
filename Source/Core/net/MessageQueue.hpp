/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/threading/AtomicVar.hpp>
#include <Core/threading/Mutex.hpp>

#include <Core/containers/Queue.hpp>

#include <Core/json/JSON.hpp>

namespace Hyperion {
namespace net {

class MessageQueue
{
public:
    MessageQueue() = default;
    MessageQueue(const MessageQueue& other) = delete;
    MessageQueue& operator=(const MessageQueue& other) = delete;
    MessageQueue(MessageQueue&& other) = delete;
    MessageQueue& operator=(MessageQueue&& other) = delete;
    ~MessageQueue() = default;

    void Push(JSON::Value&& message)
    {
        Mutex::Guard guard(m_mutex);

        m_messages.Push(std::move(message));
        m_size.Increment(1, MemoryOrder::RELEASE);
    }

    JSON::Value Pop()
    {
        HYP_CORE_ASSERT(!Empty());

        Mutex::Guard guard(m_mutex);

        JSON::Value last = m_messages.Pop();
        m_size.Decrement(1, MemoryOrder::RELEASE);

        return last;
    }

    HYP_FORCE_INLINE uint32 Size() const
    {
        return m_size.Get(MemoryOrder::ACQUIRE);
    }

    HYP_FORCE_INLINE bool Empty() const
    {
        return Size() == 0;
    }

private:
    Mutex m_mutex;
    Queue<JSON::Value> m_messages;
    AtomicVar<uint32> m_size;
};

} // namespace net

using net::MessageQueue;

} // namespace Hyperion
