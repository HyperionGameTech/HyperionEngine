/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_NOT_NULL_PTR_HPP
#define HYPERION_NOT_NULL_PTR_HPP

#include <core/Defines.hpp>

#include <Types.hpp>
#include <Constants.hpp>

#include <type_traits>

namespace hyperion {
namespace memory {

/*! \brief A pointer to T that is never allowed to be null. If a null pointer is passed to it during construction or assignment, an assertion will be triggered. */
template <class T, class T2 = void>
class NotNullPtr;

template <class T>
class NotNullPtr<T, std::enable_if_t<!std::is_const_v<T>>>
{
    NotNullPtr() { }

    NotNullPtr(std::nullptr_t) { }
    NotNullPtr &operator=(std::nullptr_t) { }

public:
    NotNullPtr(T * HYP_NOTNULL ptr)
    {
        m_ptr = ptr;
    }

    NotNullPtr(const NotNullPtr &other)                 = default;
    NotNullPtr &operator=(const NotNullPtr &other)      = default;

    NotNullPtr(NotNullPtr &&other) noexcept             = default;
    NotNullPtr &operator=(NotNullPtr &&other) noexcept  = default;

    template <class OtherT>
    NotNullPtr(const NotNullPtr<OtherT> &other)
    {
        static_assert(std::is_convertible_v<T *, std::remove_const_t<OtherT> *>);

        m_ptr = other.m_ptr;
    }

    template <class OtherT>
    NotNullPtr &operator=(const NotNullPtr<OtherT> &other)
    {
        static_assert(std::is_convertible_v<T *, std::remove_const_t<OtherT> *>);

        m_ptr = other.m_ptr;

        return *this;
    }

    template <class OtherT>
    NotNullPtr(NotNullPtr<OtherT> &&other) noexcept
    {
        static_assert(std::is_convertible_v<T *, std::remove_const_t<OtherT> *>);

        m_ptr = other.m_ptr;
    }

    template <class OtherT>
    NotNullPtr &operator=(NotNullPtr<OtherT> &&other) noexcept
    {
        static_assert(std::is_convertible_v<T *, std::remove_const_t<OtherT> *>);

        m_ptr = other.m_ptr;

        return *this;
    }

    ~NotNullPtr()                                       = default;

    template <class OtherT>
    HYP_FORCE_INLINE bool operator==(const NotNullPtr<OtherT> &other) const
        { return m_ptr == other.m_ptr; }

    template <class OtherT>
    HYP_FORCE_INLINE bool operator!=(const NotNullPtr<OtherT> &other) const
        { return m_ptr != other.m_ptr; }

    constexpr HYP_FORCE_INLINE bool operator==(std::nullptr_t) const
        { return false; }

    constexpr HYP_FORCE_INLINE bool operator!=(std::nullptr_t) const
        { return true; }

    template <class OtherT>
    HYP_FORCE_INLINE bool operator==(OtherT *ptr) const
        { return m_ptr == ptr; }

    template <class OtherT>
    HYP_FORCE_INLINE bool operator!=(OtherT *ptr) const
        { return m_ptr != ptr; }

    template <class OtherT>
    HYP_FORCE_INLINE bool operator<(const NotNullPtr<OtherT> &other) const
        { return m_ptr < other.m_ptr; }

    template <class OtherT>
    HYP_FORCE_INLINE bool operator<=(const NotNullPtr<OtherT> &other) const
        { return m_ptr <= other.m_ptr; }

    template <class OtherT>
    HYP_FORCE_INLINE bool operator>(const NotNullPtr<OtherT> &other) const
        { return m_ptr > other.m_ptr; }

    template <class OtherT>
    HYP_FORCE_INLINE bool operator>=(const NotNullPtr<OtherT> &other) const
        { return m_ptr >= other.m_ptr; }

    constexpr HYP_FORCE_INLINE explicit operator bool() const
        { return true; }

    constexpr HYP_FORCE_INLINE bool operator!() const
        { return false; }

    HYP_FORCE_INLINE T &operator*() const
        { return *m_ptr; }

    HYP_FORCE_INLINE T *operator->() const
        { return m_ptr; }

    HYP_FORCE_INLINE operator T *() const
        { return m_ptr; }

    HYP_FORCE_INLINE operator const T *() const
        { return m_ptr; }

    HYP_FORCE_INLINE T **operator &()
        { return &m_ptr; }

    HYP_FORCE_INLINE const T **operator &() const
        { return &m_ptr; }

    HYP_FORCE_INLINE T *operator+(void *other) const
        { return m_ptr + other; }

    HYP_FORCE_INLINE T *operator-(void *other) const
        { return m_ptr - other; }

    HYP_FORCE_INLINE T *Get() const
        { return m_ptr; }

private:
    T   *m_ptr;
};

} // namespace memory

template <class T>
using NotNullPtr = memory::NotNullPtr<T>;

} // namespace hyperion

#endif