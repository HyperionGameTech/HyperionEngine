#ifndef HYPERION_V2_LIB_ATOMIC_VAR_H
#define HYPERION_V2_LIB_ATOMIC_VAR_H

#include <util/Defines.hpp>

#include <atomic>
#include <algorithm>
#include <utility>

namespace hyperion {

template <class T>
class AtomicVar {
    static_assert(
        std::is_integral_v<T> || std::is_pointer_v<T> || std::is_pod_v<T>,
        "T must be a type suitable for atomic intrinsics"
    );

    static_assert(
        sizeof(T) <= 8,
        "T must have a sizeof value of <= 8"
    );

    std::atomic<T> m_value;

public:
    AtomicVar() : m_value{} {}

    AtomicVar(const T &value)
        : m_value(value)
    {
    }

    explicit AtomicVar(const AtomicVar &other)
        : m_value(other.Get())
    {
    }

    AtomicVar &operator=(const AtomicVar &other) = delete;

    ~AtomicVar() = default;

    HYP_FORCE_INLINE
    T Get() const            { return m_value.load(); }

    HYP_FORCE_INLINE
    T Set(const T &value)    { m_value.store(value); return value; }
};

} // namespace hyperion

#endif
