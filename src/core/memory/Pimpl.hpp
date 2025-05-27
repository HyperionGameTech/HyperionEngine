/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_PIMPL_HPP
#define HYPERION_PIMPL_HPP

#include <core/Defines.hpp>

#include <core/memory/Memory.hpp>

#include <Types.hpp>
#include <Constants.hpp>

#include <cstdlib>

namespace hyperion {
namespace memory {

template <class T>
class Pimpl
{
public:
    Pimpl()
        : m_ptr(nullptr),
          m_dtor(nullptr)
    {
    }

    Pimpl(std::nullptr_t)
        : m_ptr(nullptr),
          m_dtor(nullptr)
    {
    }

    /*! \brief Takes ownership of ptr.

        Ty may be a derived class of T, and the type ID of Ty will be stored, allowing
        for conversion back to UniquePtr<Ty> using Cast<Ty>().

        Do not delete the pointer passed to this,
        as it will be automatically deleted when this object or any object that takes ownership
        over from this object is destroyed. */

    template <class Ty>
    explicit Pimpl(Ty* ptr)
        : m_ptr(nullptr),
          m_dtor(nullptr)
    {
        using TyN = NormalizedType<Ty>;
        static_assert(std::is_convertible_v<std::add_pointer_t<TyN>, std::add_pointer_t<T>>, "Ty must be convertible to T!");

        Reset<Ty>(ptr);
    }

    Pimpl(const Pimpl& other) = delete;
    Pimpl& operator=(const Pimpl& other) = delete;

    /*! \brief Allows construction from a Pimpl of a convertible type. */
    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, int> = 0>
    Pimpl(Pimpl<Ty>&& other) noexcept
        : m_ptr(nullptr),
          m_dtor(nullptr)
    {
        Reset<Ty>(other.Release());
    }

    /*! \brief Allows assign from a Pimpl of a convertible type. */
    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, int> = 0>
    Pimpl& operator=(Pimpl<Ty>&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        Reset<Ty>(other.Release());

        return *this;
    }

    Pimpl(Pimpl&& other) noexcept
        : m_ptr(other.m_ptr),
          m_dtor(other.m_dtor)
    {
        other.m_ptr = nullptr;
        other.m_dtor = nullptr;
    }

    Pimpl& operator=(Pimpl&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        if (m_ptr)
        {
            m_dtor(m_ptr);
        }

        m_ptr = other.m_ptr;
        m_dtor = other.m_dtor;

        other.m_ptr = nullptr;
        other.m_dtor = nullptr;

        return *this;
    }

    ~Pimpl()
    {
        Reset();
    }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return m_ptr != nullptr;
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return m_ptr == nullptr;
    }

    HYP_FORCE_INLINE bool operator==(const Pimpl& other) const
    {
        return m_ptr == other.m_ptr;
    }

    HYP_FORCE_INLINE bool operator==(std::nullptr_t) const
    {
        return m_ptr == nullptr;
    }

    HYP_FORCE_INLINE bool operator!=(const Pimpl& other) const
    {
        return m_ptr != other.m_ptr;
    }

    HYP_FORCE_INLINE bool operator!=(std::nullptr_t) const
    {
        return m_ptr != nullptr;
    }

    HYP_FORCE_INLINE T* Get() const
    {
        return m_ptr;
    }

    HYP_FORCE_INLINE T* operator->() const
    {
        return Get();
    }

    HYP_FORCE_INLINE T& operator*() const
    {
        return *Get();
    }

    HYP_FORCE_INLINE bool operator<(const Pimpl& other) const
    {
        return uintptr_t(m_ptr) < uintptr_t(other.m_ptr);
    }

    /*! \brief Takes ownership of {ptr}, dropping the reference to the currently held value,
        if any. */
    template <class Ty>
    HYP_FORCE_INLINE void Reset(Ty* ptr)
    {
        using TyN = NormalizedType<Ty>;
        static_assert(std::is_convertible_v<std::add_pointer_t<TyN>, std::add_pointer_t<T>>, "Ty must be convertible to T!");

        if (m_ptr)
        {
            m_dtor(m_ptr);
        }

        m_ptr = ptr;
        m_dtor = ptr ? &Memory::DestructAndFree<TyN> : nullptr;
    }

    HYP_FORCE_INLINE void Reset(std::nullptr_t)
    {
        Reset();
    }

    /*! \brief Destroys any currently held object.  */
    HYP_FORCE_INLINE void Reset()
    {
        if (m_ptr)
        {
            m_dtor(m_ptr);

            m_ptr = nullptr;
            m_dtor = nullptr;
        }
    }

    /*! \brief Like Reset(), but constructs the object in-place. */
    template <class... Args>
    HYP_FORCE_INLINE Pimpl& Emplace(Args&&... args)
    {
        return (*this = Construct(std::forward<Args>(args)...));
    }

    /*! \brief Like Emplace() but the first template parameter is specified as the type to construct. */
    template <class Ty, class... Args>
    HYP_FORCE_INLINE Pimpl& EmplaceAs(Args&&... args)
    {
        static_assert(std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, "Ty must be convertible to T!");

        return (*this = MakePimpl<Ty>(std::forward<Args>(args)...));
    }

    /*! \brief Releases the ptr to be managed externally. */
    HYP_NODISCARD HYP_FORCE_INLINE T* Release()
    {
        T* ptr = m_ptr;
        m_ptr = nullptr;
        m_dtor = nullptr;

        return ptr;
    }

    /*! \brief Constructs a Pimpl<T> from the given arguments. */
    template <class... Args>
    HYP_NODISCARD HYP_FORCE_INLINE static Pimpl Construct(Args&&... args)
    {
        static_assert(std::is_constructible_v<T, Args...>, "T must be constructible using the given args");

        Pimpl pimpl;
        pimpl.m_ptr = Memory::AllocateAndConstruct<T>(std::forward<Args>(args)...);
        pimpl.m_dtor = &Memory::DestructAndFree<T>;

        return pimpl;
    }

private:
    T* m_ptr;
    void (*m_dtor)(void*);
};

template <class T>
struct MakePimplHelper
{
    template <class... Args>
    static Pimpl<T> MakePimpl(Args&&... args)
    {
        return Pimpl<T>::Construct(std::forward<Args>(args)...);
    }
};

} // namespace memory

template <class T>
using Pimpl = memory::Pimpl<T>;

template <class T, class... Args>
HYP_FORCE_INLINE Pimpl<T> MakePimpl(Args&&... args)
{
    return memory::MakePimplHelper<T>::MakePimpl(std::forward<Args>(args)...);
}

} // namespace hyperion

#endif