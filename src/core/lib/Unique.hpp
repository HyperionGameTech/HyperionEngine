#ifndef HYPERION_V2_LIB_UNIQUE_H
#define HYPERION_V2_LIB_UNIQUE_H

#include <core/Containers.hpp>
#include <Types.hpp>

namespace hyperion::v2 {

template <class T>
class UniqueRef {
public:
    UniqueRef(T *ptr);
    UniqueRef(const UniqueRef &other) = delete;
    UniqueRef &operator=(const UniqueRef &other) = delete;

    UniqueRef(UniqueRef &&other) noexcept
        : m_ptr(other.m_ptr)
    {
        other.m_ptr = nullptr;
    }

    UniqueRef &operator=(UniqueRef &&other) noexcept
    {
        if (&other == this) {
            return this;
        }

        if (m_ptr) {
            delete m_ptr;
        }

        m_ptr = other.m_ptr;
        other.m_ptr = nullptr;

        return *this;
    }

    ~UniqueRef();

    T *operator->()                { return m_ptr; }
    const T *operator->() const    { return m_ptr; }

    explicit operator bool() const { return m_ptr != nullptr; }

private:
    T *m_ptr;
};

} // namespace hyperion::v2

#endif