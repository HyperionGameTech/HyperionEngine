/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_OPTIONAL_HPP
#define HYPERION_OPTIONAL_HPP

#include <core/utilities/ValueStorage.hpp>
#include <core/system/Debug.hpp>

#include <Types.hpp>
#include <HashCode.hpp>

namespace hyperion {
namespace utilities {

namespace detail {

template <class T, typename IsReferenceType = void>
class Optional;

template <class T>
class Optional<T, std::enable_if_t< !std::is_reference_v< std::remove_const_t< T > > > >
{
public:
    Optional()
        : m_has_value(false)
    {
    }

    /*! \brief Constructs an Optional<T> from a pointer to T. If the given value is
     *  nullptr, it will be an empty Optional<T>. Otherwise, the value will be set to
     *  the value that is pointed to. */
    Optional(T *ptr)
        : m_has_value(ptr != nullptr)
    {
        if (ptr != nullptr) {
            new (&m_storage.data_buffer) T(*ptr);
        }
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
        Set(value);

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
        Set(std::move(value));

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

        if (other.m_has_value) {
            Set(other.Get());
        } else {
            Unset();
        }

        return *this;
    }

    Optional(Optional &&other) noexcept
        : m_has_value(other.m_has_value)
    {
        if (other.m_has_value) {
            new (&m_storage.data_buffer) T(std::move(other.Get()));
        }

        other.m_has_value = false;
    }

    Optional &operator=(Optional &&other) noexcept
    {
        if (other.m_has_value) {
            Set(std::move(other.Get()));
        } else {
            Unset();
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

    HYP_FORCE_INLINE explicit operator bool() const
        { return m_has_value; }

    HYP_FORCE_INLINE bool operator==(const Optional &other) const
    {
        if (m_has_value != other.m_has_value) {
            return false;
        }

        if (m_has_value) {
            return Get() == other.Get();
        }

        return true;
    }

    HYP_FORCE_INLINE bool operator!=(const Optional &other) const
    {
        if (m_has_value != other.m_has_value) {
            return true;
        }

        if (m_has_value) {
            return Get() != other.Get();
        }

        return false;
    }

    HYP_FORCE_INLINE bool operator==(const T &value) const
    {
        if (!m_has_value) {
            return false;
        }

        return Get() == value;
    }

    HYP_FORCE_INLINE bool operator!=(const T &value) const
    {
        if (!m_has_value) {
            return true;
        }

        return Get() != value;
    }

    HYP_FORCE_INLINE T *TryGet()
    {
        if (m_has_value) {
            return &Get();
        }

        return nullptr;
    }

    HYP_FORCE_INLINE const T *TryGet() const
    {
        if (m_has_value) {
            return &Get();
        }

        return nullptr;
    }

    HYP_FORCE_INLINE T &Get()
    {
        AssertThrow(m_has_value);

        return *static_cast<T *>(m_storage.GetPointer());
    }

    HYP_FORCE_INLINE const T &Get() const
    {
        AssertThrow(m_has_value);

        return *static_cast<const T *>(m_storage.GetPointer());
    }

    HYP_NODISCARD HYP_FORCE_INLINE T GetOr(T &&default_value) const &
    {
        if (m_has_value) {
            return Get();
        }

        return std::forward<T>(default_value);
    }

    HYP_NODISCARD HYP_FORCE_INLINE T GetOr(T &&default_value) &&
    {
        if (m_has_value) {
            return std::move(Get());
        }

        return std::forward<T>(default_value);
    }
    
    HYP_FORCE_INLINE void Set(const T &value)
    {
        if constexpr (std::is_copy_assignable_v<T>) {
            if (m_has_value) {
                Get() = value;
            } else {
                new (&m_storage.data_buffer) T(value);
            }
        } else {
            if (m_has_value) {
                Get().~T();
            }

            new (&m_storage.data_buffer) T(value);
        }

        m_has_value = true;
    }
    
    HYP_FORCE_INLINE void Set(T &&value)
    {
        if constexpr (std::is_move_assignable_v<T>) {
            if (m_has_value) {
                Get() = std::move(value);
            } else {
                new (&m_storage.data_buffer) T(std::move(value));
            }
        } else {
            if (m_has_value) {
                Get().~T();
            }

            new (&m_storage.data_buffer) T(std::move(value));
        }

        m_has_value = true;
    }

    //! \brief Remove the held value, setting the Optional<> to a default state.
    HYP_FORCE_INLINE void Unset()
    {
        if (m_has_value) {
            Get().~T();
            m_has_value = false;
        }
    }
    
    /*! \brief Construct the held value in-place. */
    template <class... Args>
    HYP_FORCE_INLINE void Emplace(Args &&... args)
    {
        if (m_has_value) {
            Get().~T();
        }

        m_has_value = true;
        new (&m_storage.data_buffer) T(std::forward<Args>(args)...);
    }
    
    HYP_FORCE_INLINE T *operator->()
        { return static_cast<T *>(m_storage.GetPointer()); }
    
    HYP_FORCE_INLINE const T *operator->() const
        { return static_cast<const T *>(m_storage.GetPointer()); }
    
    HYP_FORCE_INLINE T &operator*()
        { return Get(); }
    
    HYP_FORCE_INLINE const T &operator*() const
        { return Get(); }
    
    HYP_FORCE_INLINE bool HasValue() const
        { return m_has_value; }
    
    HYP_FORCE_INLINE bool Any() const
        { return m_has_value; }

    HYP_FORCE_INLINE bool Empty() const
        { return !m_has_value; }
    
    HYP_FORCE_INLINE HashCode GetHashCode() const
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

template <class T>
class Optional<T, std::enable_if_t< std::is_reference_v< std::remove_const_t< T > > > >
{
public:
    Optional()
        : m_ptr(nullptr)
    {
    }

    /*! \brief Constructs an Optional<T> from a pointer to T. If the given value is
     *  nullptr, it will be an empty Optional<T>. Otherwise, the value will be set to
     *  the value that is pointed to. */
    Optional(typename std::remove_reference_t< T > *ptr)
        : m_ptr(ptr)
    {
    }

    Optional(typename std::remove_reference_t< T > &ref)
        : m_ptr(&ref)
    {
    }

    Optional &operator=(typename std::remove_reference_t< T > &ref)
    {
        m_ptr = &ref;

        return *this;
    }

    Optional(const Optional &other)
        : m_ptr(other.m_ptr)
    {
    }

    Optional &operator=(const Optional &other)
    {
        if (&other == this) {
            return *this;
        }

        m_ptr = other.m_ptr;

        return *this;
    }

    Optional(Optional &&other) noexcept
        : m_ptr(other.m_ptr)
    {
        other.m_ptr = nullptr;
    }

    Optional &operator=(Optional &&other) noexcept
    {
        if (&other == this) {
            return *this;
        }

        m_ptr = other.m_ptr;
        other.m_ptr = nullptr;

        return *this;
    }

    ~Optional() = default;

    HYP_FORCE_INLINE explicit operator bool() const
        { return m_ptr != nullptr; }

    HYP_FORCE_INLINE bool operator==(const Optional &other) const
    {
        return m_ptr == other.m_ptr;
    }

    HYP_FORCE_INLINE bool operator!=(const Optional &other) const
    {
        return m_ptr != other.m_ptr;
    }

    HYP_FORCE_INLINE bool operator==(const T &value) const
    {
        if (m_ptr == nullptr) {
            return false;
        }

        return *m_ptr == value;
    }

    HYP_FORCE_INLINE bool operator!=(const T &value) const
    {
        if (m_ptr == nullptr) {
            return true;
        }

        return *m_ptr != value;
    }

    HYP_FORCE_INLINE typename std::remove_reference_t< T > *TryGet()
    {
        return m_ptr;
    }

    HYP_FORCE_INLINE const typename std::remove_reference_t< T > *TryGet() const
    {
        return m_ptr;
    }

    HYP_FORCE_INLINE typename std::remove_reference_t< T > &Get()
    {
        AssertThrow(m_ptr != nullptr);

        return *m_ptr;
    }

    HYP_FORCE_INLINE const typename std::remove_reference_t< T > &Get() const
    {
        AssertThrow(m_ptr != nullptr);

        return *m_ptr;
    }

    /*! \brief Replace the reference that is held with a new reference.
     *  \note This changes the pointer to the value we're referencing, it will not modify the current value. */
    HYP_FORCE_INLINE void Set(typename std::remove_reference_t< T > &ref)
    {
        *this = ref;
    }

    /*! \brief Remove the held value, setting the Optional<> to a default state. */
    HYP_FORCE_INLINE void Unset()
    {
        m_ptr = nullptr;
    }

    /*! \brief Construct the held value in-place. */
    HYP_FORCE_INLINE void Emplace(T &value)
    {
        m_ptr = &value;
    }
    
    HYP_FORCE_INLINE std::remove_reference_t< T > *operator->()
        { return m_ptr; }
    
    HYP_FORCE_INLINE const std::remove_reference_t< T > *operator->() const
        { return m_ptr; }
    
    HYP_FORCE_INLINE std::remove_reference_t< T > &operator*()
        { return Get(); }
    
    HYP_FORCE_INLINE const std::remove_reference_t< T > &operator*() const
        { return Get(); }
    
    HYP_FORCE_INLINE bool HasValue() const
        { return m_ptr != nullptr; }
    
    HYP_FORCE_INLINE bool Any() const
        { return m_ptr != nullptr; }

    HYP_FORCE_INLINE bool Empty() const
        { return m_ptr == nullptr; }
    
    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        if (m_ptr != nullptr) {
            return HashCode::GetHashCode(Get());
        }

        return HashCode();
    }

private:
    typename std::remove_reference_t< T >   *m_ptr;
};

} // namespace detail

using detail::Optional;

} // namespace utilities

using utilities::Optional;

} // namespace hyperion

#endif