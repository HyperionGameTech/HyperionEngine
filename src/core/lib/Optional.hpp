/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_LIB_OPTIONAL_HPP
#define HYPERION_V2_LIB_OPTIONAL_HPP

#include <system/Debug.hpp>
#include <Types.hpp>
#include <core/lib/ValueStorage.hpp>
#include <HashCode.hpp>

namespace hyperion {

template <class T>
class Optional
{
public:
    Optional()
        : m_has_value(false)
    {
    }

    template <class Ty, class = std::enable_if_t<std::is_convertible_v<Ty, T>>>
    Optional(const Ty &value)
        : m_has_value(true)
    {
        new (&m_storage.data_buffer) T(value);
    }

    template <class Ty, class = std::enable_if_t<std::is_convertible_v<Ty, T>>>
    Optional &operator=(const Ty &value)
    {
        if (m_has_value) {
            Get() = value;
        } else {
            m_has_value = true;
            new (&m_storage.data_buffer) T(value);
        }

        return *this;
    }

    template <class Ty, class = std::enable_if_t<std::is_convertible_v<Ty, T>>>
    Optional(Ty &&value) noexcept
        : m_has_value(true)
    {
        new (&m_storage.data_buffer) T(std::move(value));
    }

    template <class Ty, class = std::enable_if_t<std::is_convertible_v<Ty, T>>>
    Optional &operator=(Ty &&value) noexcept
    {
        if (m_has_value) {
            Get() = std::move(value);
        } else {
            m_has_value = true;
            new (&m_storage.data_buffer) T(std::move(value));
        }

        return *this;
    }

    Optional(const Optional &other)
        : m_has_value(other.m_has_value)
    {
        if (m_has_value) {
            new (&m_storage.data_buffer) T(other.Get());
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
                new (&m_storage.data_buffer) T(other.Get());
                m_has_value = true;
            }
        }

        return *this;
    }

    Optional(Optional &&other) noexcept
        : m_has_value(other.m_has_value)
    {
        if (m_has_value) {
            new (&m_storage.data_buffer) T(std::move(other.Get()));
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
                new (&m_storage.data_buffer) T(std::move(other.Get()));
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

    [[nodiscard]]
    HYP_FORCE_INLINE
    explicit operator bool() const
        { return m_has_value; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    T *TryGet()
    {
        if (m_has_value) {
            return &Get();
        }

        return nullptr;
    }

    [[nodiscard]]
    HYP_FORCE_INLINE
    const T *TryGet() const
    {
        if (m_has_value) {
            return &Get();
        }

        return nullptr;
    }

    [[nodiscard]]
    HYP_FORCE_INLINE
    T &Get()
    {
        AssertThrow(m_has_value);

        return *static_cast<T *>(m_storage.GetPointer());
    }

    [[nodiscard]]
    HYP_FORCE_INLINE
    const T &Get() const
    {
        AssertThrow(m_has_value);

        return *static_cast<const T *>(m_storage.GetPointer());
    }

    [[nodiscard]]
    HYP_FORCE_INLINE
    T GetOr(T &&default_value) const &
    {
        if (m_has_value) {
            return Get();
        }

        return std::forward<T>(default_value);
    }

    [[nodiscard]]
    HYP_FORCE_INLINE
    T GetOr(T &&default_value) &&
    {
        if (m_has_value) {
            return std::move(Get());
        }

        return std::forward<T>(default_value);
    }
    
    HYP_FORCE_INLINE
    void Set(const T &value)
    {
        *this = value;
    }
    
    HYP_FORCE_INLINE
    void Set(T &&value)
    {
        *this = std::move(value);
    }

    //! \brief Remove the held value, setting the Optional<> to a default state.
    HYP_FORCE_INLINE
    void Unset()
    {
        if (m_has_value) {
            Get().~T();
            m_has_value = false;
        }
    }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    T *operator->()
        { return static_cast<T *>(m_storage.GetPointer()); }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    const T *operator->() const
        { return static_cast<const T *>(m_storage.GetPointer()); }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    T &operator*()
        { return Get(); }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    const T &operator*() const
        { return Get(); }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    bool HasValue() const
        { return m_has_value; }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    bool Any() const
        { return m_has_value; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    bool Empty() const
        { return !m_has_value; }
    
    [[nodiscard]]
    HYP_FORCE_INLINE
    HashCode GetHashCode() const
    {
        if (m_has_value) {
            return HashCode::GetHashCode(Get());
        }

        return HashCode();
    }

private:
    ValueStorage<T> m_storage;

    bool m_has_value;
};

} // namespace hyperion

#endif