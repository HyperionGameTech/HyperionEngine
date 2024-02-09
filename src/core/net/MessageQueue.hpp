#ifndef HYPERION_CORE_NET_MESSAGE_QUEUE_HPP
#define HYPERION_CORE_NET_MESSAGE_QUEUE_HPP

#include <core/lib/AtomicVar.hpp>
#include <core/lib/Queue.hpp>
#include <core/lib/Mutex.hpp>

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
        m_size.Increment(1, MemoryOrder::SEQUENTIAL);
    }

    json::JSONValue Pop()
    {
        AssertThrow(!Empty());
        
        Mutex::Guard guard(m_mutex);

        m_size.Decrement(1, MemoryOrder::SEQUENTIAL);
        return m_messages.Pop();
    }

    uint Size() const
        { return m_size.Get(MemoryOrder::SEQUENTIAL); }

    bool Empty() const
        { return Size() == 0; }

private:
    Mutex                   m_mutex;
    Queue<json::JSONValue>  m_messages;
    AtomicVar<uint>         m_size;
};

} // namespace hyperion::net

#endif