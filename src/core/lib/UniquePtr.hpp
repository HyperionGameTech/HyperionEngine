#ifndef HYPERION_V2_LIB_UNIQUE_PTR_HPP
#define HYPERION_V2_LIB_UNIQUE_PTR_HPP

#include <util/Defines.hpp>
#include <core/lib/TypeID.hpp>
#include <core/lib/CMemory.hpp>
#include <core/lib/RefCountedPtr.hpp>
#include <core/lib/Any.hpp>
#include <Types.hpp>
#include <Constants.hpp>

#include <atomic>
#include <cstdlib>

namespace hyperion {
namespace detail {

template <class T>
class UniquePtr;

struct UniquePtrHolder
{
    void *value;
    TypeID type_id;
    TypeID base_type_id;
    void (*dtor)(void *);

    UniquePtrHolder()
        : value(nullptr),
          type_id(TypeID::ForType<void>()),
          base_type_id(TypeID::ForType<void>()),
          dtor(nullptr)
    {
    }

    UniquePtrHolder(const UniquePtrHolder &other)
        : value(other.value),
          type_id(other.type_id),
          base_type_id(other.base_type_id),
          dtor(other.dtor)
    {
    }

    UniquePtrHolder &operator=(const UniquePtrHolder &other)
    {
        value = other.value;
        type_id = other.type_id;
        base_type_id = other.base_type_id;
        dtor = other.dtor;

        return *this;
    }

    UniquePtrHolder(UniquePtrHolder &&other) noexcept
        : value(other.value),
          type_id(other.type_id),
          base_type_id(other.base_type_id),
          dtor(other.dtor)
    {
        other.value = nullptr;
        other.type_id = TypeID::ForType<void>();
        other.base_type_id = TypeID::ForType<void>();
        other.dtor = nullptr;
    }

    UniquePtrHolder &operator=(UniquePtrHolder &&other) noexcept
    {
        value = other.value;
        type_id = other.type_id;
        base_type_id = other.base_type_id;
        dtor = other.dtor;

        other.value = nullptr;
        other.type_id = TypeID::ForType<void>();
        other.base_type_id = TypeID::ForType<void>();
        other.dtor = nullptr;

        return *this;
    }

    template <class Base, class Derived, class ...Args>
    void Construct(Args &&... args)
    {
        value = Memory::New<Derived>(std::forward<Args>(args)...);
        dtor = &Memory::Delete<Derived>;
        type_id = TypeID::ForType<Derived>();
        base_type_id = TypeID::ForType<Base>();
    }

    template <class Base, class Derived>
    void TakeOwnership(Derived *ptr)
    {
        value = ptr;
        dtor = &Memory::Delete<Derived>;
        type_id = TypeID::ForType<Derived>();
        base_type_id = TypeID::ForType<Base>();
    }

    void Destruct()
    {
        dtor(value);
    }

    HYP_FORCE_INLINE explicit operator bool() const
        { return value != nullptr; }
};

class UniquePtrBase
{
public:
    UniquePtrBase()
    {
    }

    UniquePtrBase(const UniquePtrBase &other) = delete;
    UniquePtrBase &operator=(const UniquePtrBase &other) = delete;

    UniquePtrBase(UniquePtrBase &&other) noexcept
        : m_holder(std::move(other.m_holder))
    {
    }

    UniquePtrBase &operator=(UniquePtrBase &&other) noexcept
    {
        Reset();

        std::swap(m_holder, other.m_holder);

        return *this;
    }

    ~UniquePtrBase()
    {
        Reset();
    }

    [[nodiscard]] HYP_FORCE_INLINE void *Get() const
        { return m_holder.value; }

    explicit operator bool() const
        { return Get() != nullptr; }

    bool operator!() const
        { return Get() == nullptr; }

    bool operator==(const UniquePtrBase &other) const
        { return m_holder.value == other.m_holder.value; }

    bool operator==(std::nullptr_t) const
        { return Get() == nullptr; }

    bool operator!=(const UniquePtrBase &other) const
        { return m_holder.value != other.m_holder.value; }

    bool operator!=(std::nullptr_t) const
        { return Get() != nullptr; }

    [[nodiscard]] TypeID GetTypeID() const
        { return m_holder.type_id; }

    /*! \brief Destroys any currently held object.  */
    HYP_FORCE_INLINE void Reset()
    {
        if (m_holder) {
            m_holder.Destruct();
            m_holder = UniquePtrHolder();
        }
    }

    /*! \brief Releases the ptr to be managed externally.
        The value held within the UniquePtr will be unset,
        and the T* returned from this method will NEED to be deleted
        manually. */
    [[nodiscard]] HYP_FORCE_INLINE void *Release()
    {
        if (m_holder) {
            auto *ptr = m_holder.value;
            m_holder = UniquePtrHolder();
            return ptr;
        }

        return nullptr;
    }

    template <class T>
    HYP_FORCE_INLINE UniquePtr<T> CastUnsafe()
    {
        UniquePtr<T> unique;
        unique.m_holder = m_holder;
        m_holder = UniquePtrHolder();

        return unique;
    }

protected:
    explicit UniquePtrBase(UniquePtrHolder &&holder)
        : m_holder(std::move(holder))
    {
    }

    [[nodiscard]] TypeID GetBaseTypeID() const
        { return m_holder.base_type_id; }

    UniquePtrHolder m_holder;
};

/*! \brief A unique pointer with type erasure built in, so anything could be stored as UniquePtr<void>.
    You can also store a derived pointer in a UniquePtr<Base>, and it will be convertible back to UniquePtr<Derived>, as 
    well as having the destructor called correctly without needing it to be virtual.
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
    
        Ty may be a derived class of T, and the type ID of Ty will be stored, allowing
        for conversion back to UniquePtr<Ty> using Cast<Ty>().
    
        Do not delete the pointer passed to this,
        as it will be automatically deleted when this object or any object that takes ownership
        over from this object is destroyed. */

    template <class Ty>
    explicit UniquePtr(Ty *ptr)
        : Base()
    {
        using TyN = NormalizedType<Ty>;
        static_assert(std::is_convertible_v<std::add_pointer_t<TyN>, std::add_pointer_t<T>>, "Ty must be convertible to T!");

        Reset<Ty>(ptr);
    }

    explicit UniquePtr(const T &value)
        : Base()
    {
        Base::m_holder.template Construct<T>(value);
    }

    explicit UniquePtr(T &&value)
        : Base()
    {
        Base::m_holder.template Construct<T>(std::move(value));
    }

    UniquePtr(const UniquePtr &other) = delete;
    UniquePtr &operator=(const UniquePtr &other) = delete;

    /*! \brief Allows construction from a UniquePtr of a convertible type. */
    template <class Ty, std::enable_if_t<std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, int> = 0>
    UniquePtr(UniquePtr<Ty> &&other) noexcept
        : Base()
    {
        Reset<Ty>(other.Release());
    }

    /*! \brief Allows assign from a UniquePtr of a convertible type. */
    template <class Ty, std::enable_if_t<std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, int> = 0>
    UniquePtr &operator=(UniquePtr<Ty> &&other) noexcept
    {
        Reset<Ty>(other.Release());

        return *this;
    }

    UniquePtr(UniquePtr &&other) noexcept
        : Base(std::move(other))
    {
    }

    UniquePtr &operator=(UniquePtr &&other) noexcept
    {
        Base::operator=(std::move(other));

        return *this;
    }

    ~UniquePtr() = default;

    HYP_FORCE_INLINE T *Get() const
        { return static_cast<T *>(Base::m_holder.value); }

    HYP_FORCE_INLINE T *operator->() const
        { return Get(); }

    HYP_FORCE_INLINE T &operator*()
        { return *Get(); }

    HYP_FORCE_INLINE const T &operator*() const
        { return *Get(); }
    
    HYP_FORCE_INLINE bool operator<(const UniquePtr &other) const
        { return uintptr_t(Base::Get()) < uintptr_t(other.Base::Get()); }
    
    /*! \brief Drops any currently held valeu and constructs a new value using \ref{value}.
        
        Ty may be a derived class of T, and the type ID of Ty will be stored, allowing
        for conversion back to UniquePtr<Ty> using Cast<Ty>(). */
    template <class Ty>
    void Set(const T &value)
    {
        using TyN = NormalizedType<Ty>;
        static_assert(std::is_convertible_v<std::add_pointer_t<TyN>, std::add_pointer_t<T>>, "Ty must be convertible to T!");

        Base::Reset();
        
        Base::m_holder.template Construct<T, TyN>(value);
    }
    
    /*! \brief Drops any currently held valeu and constructs a new value using \ref{value}.
        
        Ty may be a derived class of T, and the type ID of Ty will be stored, allowing
        for conversion back to UniquePtr<Ty> using Cast<Ty>(). */
    template <class Ty>
    void Set(Ty &&value)
    {
        using TyN = NormalizedType<Ty>;
        static_assert(std::is_convertible_v<std::add_pointer_t<TyN>, std::add_pointer_t<T>>, "Ty must be convertible to T!");

        Base::Reset();
        
        Base::m_holder.template Construct<T, TyN>(std::move(value));
    }

    /*! \brief Takes ownership of {ptr}, dropping the reference to the currently held value,
        if any.
        
        Ty may be a derived class of T, and the type ID of Ty will be stored, allowing
        for conversion back to UniquePtr<Ty> using Cast<Ty>().
        
        Note, do not delete the ptr after passing it to Reset(), as it will be deleted
        automatically. */
    template <class Ty>
    void Reset(Ty *ptr)
    {
        using TyN = NormalizedType<Ty>;
        static_assert(std::is_convertible_v<std::add_pointer_t<TyN>, std::add_pointer_t<T>>, "Ty must be convertible to T!");

        Base::Reset();

        if (ptr) {
            Base::m_holder.template TakeOwnership<T, TyN>(ptr);
        }
    }

    /*! \brief Destroys any currently held object.  */
    HYP_FORCE_INLINE void Reset()
        { Base::Reset(); }

    /*! \brief Releases the ptr to be managed externally.
        The value held within the UniquePtr will be unset,
        and the T* returned from this method will NEED to be deleted
        manually. */
    [[nodiscard]] T *Release()
        { return static_cast<T *>(Base::Release()); }

    /*! \brief Constructs a new RefCountedPtr from this object.
        The value held within this UniquePtr will be unset,
        the RefCountedPtr taking over management of the pointer. */
    template <class CountType = UInt>
    [[nodiscard]] RefCountedPtr<T, CountType> MakeRefCounted()
    {
        RefCountedPtr<T, CountType> rc;
        rc.Reset(Release());

        return rc;
    }

    /**
     * \brief Constructs a new UniquePtr<T> from the given arguments.
     */
    template <class ...Args>
    [[nodiscard]] static UniquePtr Construct(Args &&... args)
    {
        return UniquePtr(new T(std::forward<Args>(args)...));
    }

    /**
     * \brief Returns a boolean indicating whether the type of this UniquePtr is the same as the given type, or if the given type is a base class of the type of this UniquePtr.
     */
    template <class Ty>
    [[nodiscard]] bool Is() const
    {
        return GetTypeID() == TypeID::ForType<Ty>()
            || GetBaseTypeID() == TypeID::ForType<Ty>()
            || std::is_convertible_v<std::add_pointer_t<T>, std::add_pointer_t<Ty>>
            || std::is_same_v<Ty, void>;
    }

    /*! \brief Attempts to cast the pointer directly to the given type.
        If the types are not compatible (Derived -> Base) or equal (or T is not void, in the case of a void pointer),
        no cast is performed and a null UniquePtr is returned. Otherwise, the
        value currently held in the UniquePtr being casted is std::move'd to the returned value. */
    template <class Ty>
    [[nodiscard]] HYP_FORCE_INLINE UniquePtr<Ty> Cast()
    {
        if (Is<Ty>()) {
            return Base::CastUnsafe<Ty>();
        }

        return UniquePtr<Ty>();
    }

    /*! \brief Conversion to base class UniquePtr. Performs a Cast(). */
    //template <class Ty, typename = typename std::enable_if_t<std::is_convertible_v<std::add_pointer_t<T>, std::add_pointer_t<Ty>>>>
    //operator UniquePtr<Ty>()
    //{
    //    return CastUnsafe<Ty>();
    //}
};

/*! \brief A UniquePtr<void> cannot be constructed except for being moved from an existing
    UniquePtr<T>. This is because UniquePtr<T> may hold a derived pointer, while still being convertible to
    the derived type. In order to maintain that functionality on UniquePtr<void>, we need to store the TypeID of T.
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

    explicit UniquePtr(Any &&value)
        : Base()
    {
        Base::m_holder.type_id = value.m_type_id;
        Base::m_holder.base_type_id = value.m_type_id;
        Base::m_holder.value = value.m_ptr;
        Base::m_holder.dtor = value.m_delete_function;

        value.m_ptr = nullptr;
        value.m_delete_function = nullptr;
        value.m_type_id = TypeID::ForType<void>();
    }

    UniquePtr(const Base &other) = delete;
    UniquePtr &operator=(const Base &other) = delete;
    UniquePtr(const UniquePtr &other) = delete;
    UniquePtr &operator=(const UniquePtr &other) = delete;

    UniquePtr(Base &&other) noexcept
        : Base(std::move(other))
    {
    }

    UniquePtr &operator=(Base &&other) noexcept
    {
        Base::operator=(std::move(other));

        return *this;
    }

    UniquePtr(UniquePtr &&other) noexcept
        : Base(std::move(other))
    {
    }

    UniquePtr &operator=(UniquePtr &&other) noexcept
    {
        Base::operator=(std::move(other));

        return *this;
    }

    ~UniquePtr() = default;

    HYP_FORCE_INLINE void *Get() const
        { return Base::Get(); }
    
    HYP_FORCE_INLINE bool operator<(const UniquePtr &other) const
        { return uintptr_t(Base::Get()) < uintptr_t(other.Base::Get()); }

    // template <class T>
    // void Set(const T &value)
    // {
    //     Base::Reset();
        
    //     Base::m_holder.template Construct<T>(value);
    // }

    // template <class T>
    // void Set(T &&value)
    // {
    //     Base::Reset();
        
    //     Base::m_holder.template Construct<T>(std::move(value));
    // }

    // /*! \brief Takes ownership of {ptr}, dropping the reference to the currently held value,
    //     if any. Note, do not delete the ptr after passing it to Reset(), as it will be deleted
    //     automatically. */
    // template <class T>
    // void Reset(T *ptr)
    // {
    //     Base::Reset();

    //     if (ptr) {
    //         Base::m_holder.template TakeOwnership<T>(ptr);
    //     }
    // }

    /*! \brief Destroys any currently held object.  */
    HYP_FORCE_INLINE void Reset()
        { Base::Reset(); }

    template <class Ty>
    [[nodiscard]] bool Is() const
    {
        return GetTypeID() == TypeID::ForType<Ty>()
            || GetBaseTypeID() == TypeID::ForType<Ty>()
            || std::is_same_v<Ty, void>;
    }

    /*! \brief Attempts to cast the pointer directly to the given type.
        If the types are not EXACT (or T is not void, in the case of a void pointer),
        no cast is performed and a null UniquePtr is returned. Otherwise, the
        value currently held in the UniquePtr being casted is std::move'd to the returned value.
        
        Please remember, casting from UniquePtr<void> -> UniquePtr<T> will not be able to do any base class checking
        so you can only go from UniquePtr<void> to the exact specified type! */
    template <class Ty>
    [[nodiscard]] HYP_FORCE_INLINE UniquePtr<Ty> Cast()
    {
        if (Is<Ty>()) {
            return Base::CastUnsafe<Ty>();
        }

        return UniquePtr<Ty>();
    }

    /*! \brief Constructs a new RefCountedPtr from this object.
        The value held within this UniquePtr will be unset,
        the RefCountedPtr taking over management of the pointer. */
    template <class CountType = UInt>
    [[nodiscard]] RefCountedPtr<void, CountType> MakeRefCounted()
    {
        RefCountedPtr<void, CountType> rc;
        rc.Reset(Base::Release());

        return rc;
    }
};

} // namespace detail

template <class T>
using UniquePtr = detail::UniquePtr<T>;

using AnyPtr = UniquePtr<void>;

} // namespace hyperion

#endif