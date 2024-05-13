#ifndef HYPERION_NOTIFIER_HPP
#define HYPERION_NOTIFIER_HPP

#include <core/threading/AtomicVar.hpp>
#include <core/threading/Thread.hpp>
#include <core/threading/Threads.hpp>

namespace hyperion {
namespace threading {

struct Notifier
{
    using ValueType = uint32;

    Notifier(ValueType initial_value = ValueType())
        : value(initial_value)
    {
    }

    Notifier(const Notifier &other)                 = delete;
    Notifier &operator=(const Notifier &other)      = delete;
    Notifier(Notifier &&other) noexcept             = delete;
    Notifier &operator=(Notifier &&other) noexcept  = delete;
    ~Notifier()                                     = default;

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool Consume()
    {
        ValueType current_value = value.Get(MemoryOrder::ACQUIRE);

        if (current_value) {
            value.Decrement(1, MemoryOrder::RELAXED);

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

using threading::Notifier;

} // namespace hyperion

#endif