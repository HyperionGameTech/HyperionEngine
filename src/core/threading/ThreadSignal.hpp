#pragma once
#include <core/threading/AtomicVar.hpp>
#include <core/threading/Thread.hpp>

namespace Hyperion {
namespace threading {

struct ThreadSignal
{
    using ValueType = uint32;

    ThreadSignal(ValueType initialValue = ValueType())
        : value(initialValue)
    {
    }

    ThreadSignal(const ThreadSignal& other) = delete;
    ThreadSignal& operator=(const ThreadSignal& other) = delete;
    ThreadSignal(ThreadSignal&& other) noexcept = delete;
    ThreadSignal& operator=(ThreadSignal&& other) noexcept = delete;
    ~ThreadSignal() = default;

    bool Consume()
    {
        ValueType currentValue = value.Get(MemoryOrder::ACQUIRE);

        if (currentValue)
        {
            value.Decrement(1, MemoryOrder::RELEASE);

            return true;
        }

        return false;
    }

    void Notify(ValueType increment = 1)
    {
        value.Increment(increment, MemoryOrder::RELAXED);
    }

    AtomicVar<ValueType> value;
};

} // namespace threading

using threading::ThreadSignal;

} // namespace Hyperion
