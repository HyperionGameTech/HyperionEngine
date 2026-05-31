/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/Memory/Memory.hpp>
#include <Core/Memory/Allocator/Allocator.hpp>

#include <Core/Reflection/TypeId.hpp>

#include <Core/Types.hpp>
#include <Core/Constants.hpp>

#include <cstdlib>

namespace Hyperion {

namespace memory {

struct DefaultUniquePtrAllocator; // opaque; signal to use new/delete (default)

template <class T, class AllocatorType = DefaultUniquePtrAllocator>
class UniquePtr;

template <class T, class AllocatorType>
class UniquePtr
{
    template <class Ty, class OtherAllocatorType>
    friend class UniquePtr;

public:
    UniquePtr()
        : m_ptr(nullptr)
    {
    }

    UniquePtr(std::nullptr_t)
        : m_ptr(nullptr)
    {
    }

    /*! \brief Takes ownership of ptr.

        Do not delete the pointer passed to this,
        as it will be automatically deleted when this object or any object that takes ownership
        over from this object is destroyed. */
    template <class Ty>
    explicit UniquePtr(Ty* ptr)
        : m_ptr(ptr)
    {
        static_assert(std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, "Ty must be convertible to T!");
    }

    UniquePtr(const UniquePtr& other) = delete;
    UniquePtr& operator=(const UniquePtr& other) = delete;

    /*! \brief Allows construction from a UniquePtr of a convertible type. */
    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, int> = 0>
    UniquePtr(UniquePtr<Ty, AllocatorType>&& other) noexcept
        : m_ptr(other.Release())
    {
    }

    /*! \brief Allows assign from a UniquePtr of a convertible type. */
    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, int> = 0>
    UniquePtr& operator=(UniquePtr<Ty, AllocatorType>&& other) noexcept
    {
        Reset(other.Release());

        return *this;
    }

    UniquePtr(UniquePtr&& other) noexcept
        : m_ptr(other.Release())
    {
    }

    UniquePtr& operator=(UniquePtr&& other) noexcept
    {
        if (this != &other)
        {
            Reset(other.Release());
        }

        return *this;
    }

    ~UniquePtr()
    {
        Reset();
    }

    HYP_FORCE_INLINE T* Get() const
    {
        return m_ptr;
    }

    HYP_FORCE_INLINE T* operator->() const
    {
        return m_ptr;
    }

    HYP_FORCE_INLINE T& operator*() const&
    {
        return *m_ptr;
    }

    HYP_FORCE_INLINE T operator*() &&
    {
        T result = std::move(*m_ptr);
        Reset();

        return result;
    }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return m_ptr != nullptr;
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return m_ptr == nullptr;
    }

    HYP_FORCE_INLINE bool operator==(const UniquePtr& other) const
    {
        return m_ptr == other.m_ptr;
    }

    HYP_FORCE_INLINE bool operator==(std::nullptr_t) const
    {
        return m_ptr == nullptr;
    }

    HYP_FORCE_INLINE bool operator!=(const UniquePtr& other) const
    {
        return m_ptr != other.m_ptr;
    }

    HYP_FORCE_INLINE bool operator!=(std::nullptr_t) const
    {
        return m_ptr != nullptr;
    }

    HYP_FORCE_INLINE bool operator<(const UniquePtr& other) const
    {
        return UIntPtr(m_ptr) < UIntPtr(other.m_ptr);
    }

    /*! \brief Drops any currently held valeu and constructs a new value using \p value.

        Ty may be a derived class of T, and the type Id of Ty will be stored, allowing
        for conversion back to UniquePtr<Ty> using Cast<Ty>(). */
    template <class Ty>
    HYP_FORCE_INLINE void Set(Ty&& value)
    {
        using TyN = NormalizedType<Ty>;
        static_assert(std::is_convertible_v<std::add_pointer_t<TyN>, std::add_pointer_t<T>>, "Ty must be convertible to T!");

        Reset();

        m_ptr = AllocateObject<TyN>(std::forward<Ty>(value));
    }

    /*! \brief Takes ownership of {ptr}, dropping the reference to the currently held value,
        if any.

        Ty may be a derived class of T, and the type Id of Ty will be stored, allowing
        for conversion back to UniquePtr<Ty> using Cast<Ty>().

        Note, do not delete the ptr after passing it to Reset(), as it will be deleted
        automatically. */
    template <class Ty>
    HYP_FORCE_INLINE void Reset(Ty* ptr)
    {
        static_assert(std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, "Ty must be convertible to T!");

        DestroyObject();
        m_ptr = ptr;
    }

    HYP_FORCE_INLINE void Reset(std::nullptr_t)
    {
        Reset();
    }

    /*! \brief Destroys any currently held object.  */
    HYP_FORCE_INLINE void Reset()
    {
        DestroyObject();
        m_ptr = nullptr;
    }

    /*! \brief Like Reset(), but constructs the object in-place. */
    template <class... Args>
    HYP_FORCE_INLINE UniquePtr& Emplace(Args&&... args)
    {
        Reset();
        m_ptr = AllocateObject<T>(std::forward<Args>(args)...);

        return *this;
    }

    /*! \brief Like Emplace() but the first template parameter is specified as the type to construct. */
    template <class Ty, class... Args>
    HYP_FORCE_INLINE UniquePtr& EmplaceAs(Args&&... args)
    {
        static_assert(std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, "Ty must be convertible to T!");

        Reset();
        m_ptr = AllocateObject<Ty>(std::forward<Args>(args)...);

        return *this;
    }

    /*! \brief Releases the ptr to be managed externally.
        The value held within the UniquePtr will be unset,
        and the T* returned from this method will NEED to be deleted
        manually. */
    HYP_NODISCARD HYP_FORCE_INLINE T* Release()
    {
        T* ptr = m_ptr;
        m_ptr = nullptr;

        return ptr;
    }

    /*! \brief Constructs a new UniquePtr<T> from the given arguments. */
    template <class... Args>
    HYP_NODISCARD HYP_FORCE_INLINE static UniquePtr Construct(Args&&... args)
    {
        static_assert(std::is_constructible_v<T, Args...>, "T must be constructible using the given args");

        UniquePtr ptr;
        ptr.m_ptr = ptr.AllocateObject<T>(std::forward<Args>(args)...);

        return ptr;
    }

    /*! \brief Constructs a new UniquePtr<T> from the given arguments, using the provided allocator. */
    template <class... Args>
    HYP_NODISCARD HYP_FORCE_INLINE static UniquePtr ConstructWithAllocator(AllocatorType allocator, Args&&... args)
    {
        static_assert(std::is_constructible_v<T, Args...>, "T must be constructible using the given args");

        UniquePtr ptr(std::move(allocator));
        ptr.m_ptr = ptr.AllocateObject<T>(std::forward<Args>(args)...);

        return ptr;
    }

private:
    template <class Ty, class... Args>
    HYP_NODISCARD HYP_FORCE_INLINE Ty* AllocateObject(Args&&... args)
    {
        if constexpr (CONSTEXPR_TYPE_ID(AllocatorType) == CONSTEXPR_TYPE_ID(DefaultUniquePtrAllocator))
        {
            return new Ty(std::forward<Args>(args)...);
        }
        else
        {
            void* mem = GetDefaultAllocatorInstance<AllocatorType>()->Allocate(sizeof(Ty), alignof(Ty));
            return new (mem) Ty(std::forward<Args>(args)...);
        }
    }

    HYP_FORCE_INLINE void DestroyObject()
    {
        if (m_ptr)
        {
            if constexpr (CONSTEXPR_TYPE_ID(AllocatorType) == CONSTEXPR_TYPE_ID(DefaultUniquePtrAllocator))
            {
                delete m_ptr;
            }
            else
            {
                m_ptr->~T();
                GetDefaultAllocatorInstance<AllocatorType>()->Free(static_cast<void*>(m_ptr));
            }
        }
    }

    T* m_ptr;
};

} // namespace memory

template <class T, class AllocatorType = memory::DefaultUniquePtrAllocator>
using UniquePtr = memory::UniquePtr<T, AllocatorType>;

template <class T, class... Args>
HYP_FORCE_INLINE UniquePtr<T> MakeUnique(Args&&... args)
{
    return UniquePtr<T>::Construct(std::forward<Args>(args)...);
}

template <class T, class AllocatorType, class... Args>
HYP_FORCE_INLINE UniquePtr<T, AllocatorType> MakeUniqueWithAllocator(AllocatorType allocator, Args&&... args)
{
    return UniquePtr<T, AllocatorType>::ConstructWithAllocator(std::move(allocator), std::forward<Args>(args)...);
}

} // namespace Hyperion
