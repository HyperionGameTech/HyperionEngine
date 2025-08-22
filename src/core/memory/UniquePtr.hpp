/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/utilities/TypeId.hpp>

#include <core/memory/Memory.hpp>
#include <core/memory/Any.hpp>

#include <core/object/HypObjectFwd.hpp>

#include <core/Types.hpp>
#include <core/Constants.hpp>

#include <cstdlib>

namespace hyperion {

class HypClass;

extern HYP_API const HypClass* GetClass(TypeId typeId);
extern HYP_API bool IsA(const HypClass* hypClass, const void* ptr, TypeId typeId);

namespace memory {

template <class T>
class UniquePtr;

struct UniquePtrHolder
{
    void* value;
    TypeId typeId;
    void (*dtor)(void*);

    UniquePtrHolder()
        : value(nullptr),
          typeId(TypeId::ForType<void>()),
          dtor(nullptr)
    {
    }

    UniquePtrHolder(const UniquePtrHolder& other)
        : value(other.value),
          typeId(other.typeId),
          dtor(other.dtor)
    {
    }

    UniquePtrHolder& operator=(const UniquePtrHolder& other)
    {
        if (this == &other)
        {
            return *this;
        }

        value = other.value;
        typeId = other.typeId;
        dtor = other.dtor;

        return *this;
    }

    UniquePtrHolder(UniquePtrHolder&& other) noexcept
        : value(other.value),
          typeId(other.typeId),
          dtor(other.dtor)
    {
        other.value = nullptr;
        other.typeId = TypeId::ForType<void>();
        other.dtor = nullptr;
    }

    UniquePtrHolder& operator=(UniquePtrHolder&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        value = other.value;
        typeId = other.typeId;
        dtor = other.dtor;

        other.value = nullptr;
        other.typeId = TypeId::ForType<void>();
        other.dtor = nullptr;

        return *this;
    }

    template <class Base, class Derived, class... Args>
    void Construct(Args&&... args)
    {
        value = Memory::AllocateAndConstruct<Derived>(std::forward<Args>(args)...);
        dtor = &Memory::DestructAndFree<Derived>;
        typeId = TypeId::ForType<Derived>();
    }

    template <class Base, class Derived>
    void TakeOwnership(Derived* ptr)
    {
        value = ptr;
        dtor = &Memory::DestructAndFree<Derived>;
        typeId = TypeId::ForType<Derived>();
    }

    void Destruct()
    {
        dtor(value);
    }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return value != nullptr;
    }
};

class UniquePtrBase
{
public:
    UniquePtrBase() = default;

    UniquePtrBase(const UniquePtrBase& other) = delete;
    UniquePtrBase& operator=(const UniquePtrBase& other) = delete;

    UniquePtrBase(UniquePtrBase&& other) noexcept
        : m_holder(std::move(other.m_holder))
    {
    }

    UniquePtrBase& operator=(UniquePtrBase&& other) noexcept
    {
        Reset();

        m_holder = std::move(other.m_holder);

        return *this;
    }

    ~UniquePtrBase()
    {
        Reset();
    }

    HYP_FORCE_INLINE void* Get() const
    {
        return m_holder.value;
    }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return Get() != nullptr;
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return Get() == nullptr;
    }

    HYP_FORCE_INLINE bool operator==(const UniquePtrBase& other) const
    {
        return m_holder.value == other.m_holder.value;
    }

    HYP_FORCE_INLINE bool operator==(std::nullptr_t) const
    {
        return Get() == nullptr;
    }

    HYP_FORCE_INLINE bool operator!=(const UniquePtrBase& other) const
    {
        return m_holder.value != other.m_holder.value;
    }

    HYP_FORCE_INLINE bool operator!=(std::nullptr_t) const
    {
        return Get() != nullptr;
    }

    HYP_FORCE_INLINE TypeId GetTypeId() const
    {
        return m_holder.typeId;
    }

    /*! \brief Destroys any currently held object.  */
    HYP_FORCE_INLINE void Reset()
    {
        if (m_holder)
        {
            m_holder.Destruct();
            m_holder = UniquePtrHolder();
        }
    }

    /*! \brief Releases the ptr to be managed externally.
        The value held within the UniquePtr will be unset,
        and the T* returned from this method will NEED to be deleted
        manually. */
    HYP_NODISCARD HYP_FORCE_INLINE void* Release()
    {
        if (m_holder)
        {
            void* ptr = m_holder.value;
            m_holder = UniquePtrHolder();

            return ptr;
        }

        return nullptr;
    }

protected:
    explicit UniquePtrBase(UniquePtrHolder&& holder)
        : m_holder(std::move(holder))
    {
    }

    UniquePtrHolder m_holder;
};

/*! \brief A unique pointer with type erasure built in, so anything could be stored as UniquePtr<void> as
    well as having the correct destructor called without needing it to be virtual.
*/
template <class T>
class UniquePtr : public UniquePtrBase
{
protected:
    using Base = UniquePtrBase;

public:
    UniquePtr()
        : Base()
    {
    }

    UniquePtr(std::nullptr_t)
        : Base()
    {
    }

    /*! \brief Takes ownership of ptr.

        Do not delete the pointer passed to this,
        as it will be automatically deleted when this object or any object that takes ownership
        over from this object is destroyed. */

    template <class Ty>
    explicit UniquePtr(Ty* ptr)
        : Base()
    {
        using TyN = NormalizedType<Ty>;
        static_assert(std::is_convertible_v<std::add_pointer_t<TyN>, std::add_pointer_t<T>>, "Ty must be convertible to T!");

        Reset<Ty>(ptr);
    }

    UniquePtr(const UniquePtr& other) = delete;
    UniquePtr& operator=(const UniquePtr& other) = delete;

    /*! \brief Allows construction from a UniquePtr of a convertible type. */
    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, int> = 0>
    UniquePtr(UniquePtr<Ty>&& other) noexcept
        : Base()
    {
        Reset<Ty>(other.Release());
    }

    /*! \brief Allows assign from a UniquePtr of a convertible type. */
    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, int> = 0>
    UniquePtr& operator=(UniquePtr<Ty>&& other) noexcept
    {
        Reset<Ty>(other.Release());

        return *this;
    }

    UniquePtr(UniquePtr&& other) noexcept
        : Base(std::move(other))
    {
    }

    UniquePtr& operator=(UniquePtr&& other) noexcept
    {
        Base::operator=(std::move(other));

        return *this;
    }

    ~UniquePtr() = default;

    HYP_FORCE_INLINE T* Get() const
    {
        return static_cast<T*>(Base::m_holder.value);
    }

    HYP_FORCE_INLINE T* operator->() const
    {
        return static_cast<T*>(Base::m_holder.value);
    }

    HYP_FORCE_INLINE T& operator*() const&
    {
        return *static_cast<T*>(Base::m_holder.value);
    }

    HYP_FORCE_INLINE T operator*() &&
    {
        T result = std::move(*static_cast<T*>(Base::m_holder.value));
        Reset();

        return result;
    }

    HYP_FORCE_INLINE bool operator<(const UniquePtr& other) const
    {
        return uintptr_t(Base::Get()) < uintptr_t(other.Base::Get());
    }

    /*! \brief Drops any currently held valeu and constructs a new value using \ref{value}.

        Ty may be a derived class of T, and the type Id of Ty will be stored, allowing
        for conversion back to UniquePtr<Ty> using Cast<Ty>(). */
    template <class Ty>
    HYP_FORCE_INLINE void Set(Ty&& value)
    {
        using TyN = NormalizedType<Ty>;
        static_assert(std::is_convertible_v<std::add_pointer_t<TyN>, std::add_pointer_t<T>>, "Ty must be convertible to T!");

        Base::Reset();

        Base::m_holder.template Construct<T, TyN>(std::forward<Ty>(value));
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
        using TyN = NormalizedType<Ty>;
        static_assert(std::is_convertible_v<std::add_pointer_t<TyN>, std::add_pointer_t<T>>, "Ty must be convertible to T!");

        Base::Reset();

        if (ptr)
        {
            Base::m_holder.template TakeOwnership<T, TyN>(ptr);
        }
    }

    HYP_FORCE_INLINE void Reset(std::nullptr_t)
    {
        Reset();
    }

    /*! \brief Destroys any currently held object.  */
    HYP_FORCE_INLINE void Reset()
    {
        Base::Reset();
    }

    /*! \brief Like Reset(), but constructs the object in-place. */
    template <class... Args>
    HYP_FORCE_INLINE UniquePtr& Emplace(Args&&... args)
    {
        return (*this = Construct(std::forward<Args>(args)...));
    }

    /*! \brief Like Emplace() but the first template parameter is specified as the type to construct. */
    template <class Ty, class... Args>
    HYP_FORCE_INLINE UniquePtr& EmplaceAs(Args&&... args)
    {
        static_assert(std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, "Ty must be convertible to T!");

        return (*this = MakeUnique<Ty>(std::forward<Args>(args)...));
    }

    /*! \brief Releases the ptr to be managed externally.
        The value held within the UniquePtr will be unset,
        and the T* returned from this method will NEED to be deleted
        manually. */
    HYP_NODISCARD HYP_FORCE_INLINE T* Release()
    {
        return static_cast<T*>(Base::Release());
    }

    /*! \brief Constructs a new UniquePtr<T> from the given arguments. */
    template <class... Args>
    HYP_NODISCARD HYP_FORCE_INLINE static UniquePtr Construct(Args&&... args)
    {
        UniquePtr ptr;
        ptr.Reset(Memory::AllocateAndConstruct<T>(std::forward<Args>(args)...));

        return ptr;
    }

    /*! \brief Returns a boolean indicating whether the type of this UniquePtr is the same as the given type, or if the given type is a base class of the type of this UniquePtr.
     *  If T has a HypClass registered, this function will also return true if the held object is a subclass of T or if IsA() would return true. */
    template <class Ty>
    HYP_FORCE_INLINE bool Is() const
    {
        constexpr TypeId typeId = TypeId::ForType<Ty>();

        return std::is_convertible_v<std::add_pointer_t<T>, std::add_pointer_t<Ty>>
            || std::is_same_v<Ty, void>
            || GetTypeId() == typeId
            || IsA(GetClass(typeId), m_holder.value, GetTypeId());
    }
};

/*! \brief A UniquePtr<void> cannot be constructed except for being moved from an existing
    UniquePtr<T>. This is because UniquePtr<T> may hold a derived pointer, while still being convertible to
    the derived type. In order to maintain that functionality on UniquePtr<void>, we need to store the TypeId of T.
    We would not be able to do that with UniquePtr<void>.*/
// void pointer specialization
template <>
class UniquePtr<void> : public UniquePtrBase
{
protected:
    using Base = UniquePtrBase;

public:
    UniquePtr()
        : Base()
    {
    }

    UniquePtr(const Base& other) = delete;
    UniquePtr& operator=(const Base& other) = delete;
    UniquePtr(const UniquePtr& other) = delete;
    UniquePtr& operator=(const UniquePtr& other) = delete;

    UniquePtr(Base&& other) noexcept
        : Base(std::move(other))
    {
    }

    UniquePtr& operator=(Base&& other) noexcept
    {
        Base::operator=(std::move(other));

        return *this;
    }

    UniquePtr(UniquePtr&& other) noexcept
        : Base(std::move(other))
    {
    }

    UniquePtr& operator=(UniquePtr&& other) noexcept
    {
        Base::operator=(std::move(other));

        return *this;
    }

    ~UniquePtr() = default;

    HYP_FORCE_INLINE void* Get() const
    {
        return Base::Get();
    }

    HYP_FORCE_INLINE bool operator<(const UniquePtr& other) const
    {
        return uintptr_t(Base::Get()) < uintptr_t(other.Base::Get());
    }

    /*! \brief Destroys any currently held object.  */
    HYP_FORCE_INLINE void Reset()
    {
        Base::Reset();
    }

    template <class Ty>
    HYP_FORCE_INLINE bool Is() const
    {
        constexpr TypeId typeId = TypeId::ForType<Ty>();

        return std::is_same_v<Ty, void>
            || GetTypeId() == typeId
            || IsA(GetClass(typeId), m_holder.value, GetTypeId());
    }
};

template <class T>
struct MakeUniqueHelper
{
    template <class... Args>
    static UniquePtr<T> MakeUnique(Args&&... args)
    {
        return UniquePtr<T>::Construct(std::forward<Args>(args)...);
    }
};

} // namespace memory

template <class T>
using UniquePtr = memory::UniquePtr<T>;

template <class T, class... Args>
HYP_FORCE_INLINE UniquePtr<T> MakeUnique(Args&&... args)
{
    return memory::MakeUniqueHelper<T>::MakeUnique(std::forward<Args>(args)...);
}

} // namespace hyperion
