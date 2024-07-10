/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_NET_MESSAGE_QUEUE_HPP
#define HYPERION_CORE_NET_MESSAGE_QUEUE_HPP

#include <core/threading/AtomicVar.hpp>
#include <core/containers/Queue.hpp>
#include <core/threading/Mutex.hpp>

#include <util/json/JSON.hpp>

namespace hyperion::net {

class MessageQueue
{
public:
    MessageQueue()                                      = default;
    MessageQueue(const MessageQueue &other)             = delete;
    MessageQueue &operator=(const MessageQueue &other)  = delete;
    MessageQueue(MessageQueue &&other)                  = delete;
    MessageQueue &operator=(MessageQueue &&other)       = delete;
    ~MessageQueue()                                     = default;

    void Push(json::JSONValue &&message)
    {
        Mutex::Guard guard(m_mutex);

        m_messages.Push(std::move(message));
        m_size.Increment(1, MemoryOrder::RELEASE);
    }

    json::JSONValue Pop()
    {
        AssertThrow(!Empty());
        
        Mutex::Guard guard(m_mutex);

        json::JSONValue last = m_messages.Pop();
        m_size.Decrement(1, MemoryOrder::RELEASE);
        
        return last;
    }

    HYP_FORCE_INLINE uint32 Size() const
        { return m_size.Get(MemoryOrder::ACQUIRE); }

    HYP_FORCE_INLINE bool Empty() const
        { return Size() == 0; }

private:
    Mutex                   m_mutex;
    Queue<json::JSONValue>  m_messages;
    AtomicVar<uint32>       m_size;
};

} // namespace hyperion::net

#endif