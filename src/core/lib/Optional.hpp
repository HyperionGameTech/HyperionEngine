#ifndef HYPERION_V2_LIB_OPTIONAL_H
#define HYPERION_V2_LIB_OPTIONAL_H

#include <system/Debug.hpp>
#include <Types.hpp>

namespace hyperion {

template <class T>
class Optional {
public:
    Optional()
        : m_has_value(false)
    {
    }

    Optional(const T &value)
        : m_has_value(true)
    {
        new (m_storage.data_buffer) T(value);
    }

    Optional &operator=(const T &value)
    {
        if (m_has_value) {
            Get() = value;
        } else {
            m_has_value = true;
            new (m_storage.data_buffer) T(value);
        }
        return *this;
    }

    Optional(T &&value) noexcept
        : m_has_value(true)
    {
        new (m_storage.data_buffer) T(std::forward<T>(value));
    }

    Optional &operator=(T &&value)
    {
        if (m_has_value) {
            Get() = std::forward<T>(value);
        } else {
            m_has_value = true;
            new (m_storage.data_buffer) T(std::forward<T>(value));
        }

        return *this;
    }

    Optional(const Optional &other)
        : has_value(other.has_value)
    {
        if (m_has_value) {
            new (m_storage.data_buffer) T(other.Get());
        }
    }

    Optional &operator=(const Optional &other)
    {
        if (&other == this) {
            return *this;
        }

        if (m_has_value) {
            if (other.m_has_value) {
                Get() = other.Get();
            } else {
                Get().~T();
                m_has_value = false;
            }
        } else {
            if (other.m_has_value) {
                new (m_storage.data_buffer) T(other.Get());
                m_has_value = true;
            }
        }

        return *this;
    }

    Optional(Optional &&other) noexcept
        : has_value(other.has_value)
    {
        if (m_has_value) {
            new (m_storage.data_buffer) T(std::move(other.Get()));
        }
    }

    Optional &operator=(Optional &&other) noexcept
    {
        if (&other == this) {
            return *this;
        }

        if (m_has_value) {
            if (other.m_has_value) {
                Get() = std::move(other.Get());
            } else {
                Get().~T();
                m_has_value = false;
            }
        } else {
            if (other.m_has_value) {
                new (m_storage.data_buffer) T(std::move(other.Get()));
                m_has_value = true;
            }
        }

        other.m_has_value = false;

        return *this;
    }

    ~Optional()
    {
        if (m_has_value) {
            Get().~T();
        }
    }

    T &Get()
    {
        AssertThrow(has_value);

        return *reinterpret_cast<T *>(&m_storage.data_buffer);
    }

    const T &Get() const
    {
        AssertThrow(has_value);

        return *reinterpret_cast<const T *>(&m_storage.data_buffer);
    }

    bool Any() const { return m_hsa_value; }

private:
    struct Storage {
        using StorageType = typename std::aligned_storage_t<sizeof(T), alignof(T)>;

        StorageType data_buffer;
    } m_storage;

    bool m_has_value;
};

} // namespace hyperion

#endif