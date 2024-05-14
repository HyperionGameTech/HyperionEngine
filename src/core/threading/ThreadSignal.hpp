#ifndef HYPERION_THREAD_SIGNAL_HPP
#define HYPERION_THREAD_SIGNAL_HPP

#include <core/threading/AtomicVar.hpp>
#include <core/threading/Thread.hpp>
#include <core/threading/Threads.hpp>

namespace hyperion {
namespace threading {

struct ThreadSignal
{
    using ValueType = uint32;

    ThreadSignal(ValueType initial_value = ValueType())
        : value(initial_value)
    {
    }

    ThreadSignal(const ThreadSignal &other)                 = delete;
    ThreadSignal &operator=(const ThreadSignal &other)      = delete;
    ThreadSignal(ThreadSignal &&other) noexcept             = delete;
    ThreadSignal &operator=(ThreadSignal &&other) noexcept  = delete;
    ~ThreadSignal()                                         = default;

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool Consume()
    {
        ValueType current_value = value.Get(MemoryOrder::ACQUIRE);

        if (current_value) {
            value.Decrement(1, MemoryOrder::RELEASE);

            return true;
        }

        return false;
    }

    void Notify(ValueType increment = 1)
    {
        value.Increment(increment, MemoryOrder::RELAXED);
    }

    AtomicVar<ValueType>    value;
};

} // namespace threading

using threading::ThreadSignal;

} // namespace hyperion

#endif