/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_REF_COUNTED_PTR_HPP
#define HYPERION_REF_COUNTED_PTR_HPP

#include <core/Defines.hpp>

#include <core/utilities/ValueStorage.hpp>
#include <core/utilities/TypeID.hpp>

#include <core/memory/Memory.hpp>

#include <core/system/Debug.hpp>

#include <Types.hpp>
#include <Constants.hpp>

#include <atomic>
#include <cstdlib>

namespace hyperion {
namespace memory {

template <class T, class CountType = std::atomic<uint>>
class EnableRefCountedPtrFromThis;

template <class T, class CountType>
class RefCountedPtr;

template <class T, class CountType>
class WeakRefCountedPtr;

template <class CountType>
class WeakRefCountedPtrBase;

template <class CountType>
class EnableRefCountedPtrFromThisBase;

template <class CountType>
class RefCountedPtrBase;

#ifndef HYP_DEBUG_MODE
    #define EnsureUninitialized()
#endif

namespace detail {

template <class CountType>
struct RefCountData
{
    void        *value;
    TypeID      type_id;
    CountType   strong_count;
    CountType   weak_count;
    void        (*dtor)(void *);

    RefCountData()
    {
        value = nullptr;
        type_id = TypeID::Void();
        strong_count = 0;
        weak_count = 0;
        dtor = nullptr;
    }
    
#ifdef HYP_DEBUG_MODE
    ~RefCountData()
    {
        EnsureUninitialized();
    }
#endif

#ifdef HYP_DEBUG_MODE
    HYP_FORCE_INLINE void EnsureUninitialized() const
    {
        AssertThrow(this->value == nullptr);
        AssertThrow(this->type_id == TypeID::Void());
        AssertThrow(this->strong_count == 0);
        AssertThrow(this->weak_count == 0);
        AssertThrow(this->dtor == nullptr);
    }
#endif

    HYP_FORCE_INLINE void InitFromParams(void *value, TypeID type_id, void(*dtor)(void *))
    {
        EnsureUninitialized();

        this->value = value;
        this->type_id = type_id;
        this->dtor = dtor;
    }

    HYP_FORCE_INLINE bool HasValue() const
        { return value != nullptr; }
    
    HYP_FORCE_INLINE void IncRefCount_Strong()
    {
        if constexpr (std::is_integral_v<CountType>) {
            ++strong_count;
        } else {
            strong_count.fetch_add(1, std::memory_order_relaxed);
        }
    }
    
    HYP_FORCE_INLINE void IncRefCount_Weak()
    {
        if constexpr (std::is_integral_v<CountType>) {
            ++weak_count;
        } else {
            weak_count.fetch_add(1, std::memory_order_relaxed);
        }
    }

    HYP_FORCE_INLINE uint DecRefCount_Strong()
    {
        if constexpr (std::is_integral_v<CountType>) {
            return --strong_count;
        } else {
            return strong_count.fetch_sub(1, std::memory_order_acq_rel) - 1;
        }
    }

    HYP_FORCE_INLINE uint DecRefCount_Weak()
    {
        if constexpr (std::is_integral_v<CountType>) {
            return --weak_count;
        } else {
            return weak_count.fetch_sub(1, std::memory_order_acq_rel) - 1;
        }
    }

    HYP_FORCE_INLINE uint UseCount_Strong() const
    {
        if constexpr (std::is_integral_v<CountType>) {
            return strong_count;
        } else {
            return strong_count.load(std::memory_order_acquire);
        }
    }

    HYP_FORCE_INLINE uint UseCount_Weak() const
    {
        if constexpr (std::is_integral_v<CountType>) {
            return weak_count;
        } else {
            return weak_count.load(std::memory_order_acquire);
        }
    }

    template <class T>
    void InitStrong(T *ptr)
    {
        using Normalized = NormalizedType<T>;

        static_assert(!std::is_void_v<T>, "Cannot initialize RefCountedPtr data with void pointer");

        // Setup weak ptr for EnableRefCountedPtrFromThis
        if constexpr (std::is_base_of_v<EnableRefCountedPtrFromThisBase<CountType>, Normalized>) {
#ifdef HYP_DEBUG_MODE
            // Should already be set up from InitWeak() call.
            AssertThrow(value == ptr);
#endif
        } else {
            value = ptr;
        }

#ifdef HYP_DEBUG_MODE
        AssertThrowMsg(UseCount_Strong() == 0, "Initializing RefCountedPtr but ptr is already owned by another RefCountedPtr object!");
#endif

        // Override type_id, dtor for derived types
        type_id = TypeID::ForType<Normalized>();
        dtor = &Memory::Delete<Normalized>;

        IncRefCount_Strong();
    }

    template <class T>
    void InitWeak(T *ptr)
    {
        static_assert(std::is_base_of_v<EnableRefCountedPtrFromThisBase<CountType>, T>, "T must derive EnableRefCountedPtrFromThis<T, CountType> to use this method");

        value = ptr;

        // Weak count will be incremented - set to 1 on construction
        ptr->EnableRefCountedPtrFromThisBase<CountType>::weak.SetRefCountData_Internal(this, true);
    }

    void Destruct()
    {
#ifdef HYP_DEBUG_MODE
        AssertThrow(value != nullptr);
        AssertThrow(strong_count == 0);
#endif

        void *current_value = value;
        value = nullptr;

        void (*current_dtor)(void *) = dtor;
        dtor = nullptr;

        type_id = TypeID::ForType<void>();

        current_dtor(current_value);
    }
};

} // namespace detail

#ifndef HYP_DEBUG_MODE
    #undef EnsureUninitialized
#endif

template <class CountType>
class RefCountedPtrBase
{
    friend class WeakRefCountedPtrBase<CountType>;

public:
    using RefCountDataType = detail::RefCountData<CountType>;
    
    static const RefCountDataType null_ref__internal;

    RefCountedPtrBase()
        : m_ref(const_cast<RefCountDataType *>(&null_ref__internal))
    {
    }

protected:

    RefCountedPtrBase(const RefCountedPtrBase &other)
        : m_ref(other.m_ref)
    {
        IncRefCount();
    }

    RefCountedPtrBase &operator=(const RefCountedPtrBase &other)
    {
        if (std::addressof(other) == this) {
            return *this;
        }

        DropRefCount();

        m_ref = other.m_ref;

        IncRefCount();

        return *this;
    }

    RefCountedPtrBase(RefCountedPtrBase &&other) noexcept
        : m_ref(other.m_ref)
    {
        /// NOTE: Cast away constness -- modifying / dereferencing null_ref__internal
        /// of any type is already ill-formed
        other.m_ref = const_cast<RefCountDataType *>(&null_ref__internal);
    }

    RefCountedPtrBase &operator=(RefCountedPtrBase &&other) noexcept
    {
        if (std::addressof(other) == this) {
            return *this;
        }

        DropRefCount();

        m_ref = other.m_ref;
        other.m_ref = const_cast<RefCountDataType *>(&null_ref__internal);

        return *this;
    }

public:

    ~RefCountedPtrBase()
    {
        DropRefCount();
    }

    HYP_FORCE_INLINE void *Get() const
        { return m_ref->value; }
    
    HYP_FORCE_INLINE explicit operator bool() const
        { return m_ref->value != nullptr; }
    
    HYP_FORCE_INLINE bool operator!() const
        { return m_ref->value == nullptr; }
    
    HYP_FORCE_INLINE bool operator==(const RefCountedPtrBase &other) const
        { return Get() == other.Get(); }
    
    HYP_FORCE_INLINE bool operator==(std::nullptr_t) const
        { return Get() == nullptr; }
    
    HYP_FORCE_INLINE bool operator!=(const RefCountedPtrBase &other) const
        { return Get() != other.Get(); }
    
    HYP_FORCE_INLINE bool operator!=(std::nullptr_t) const
        { return Get() != nullptr; }
    
    HYP_FORCE_INLINE TypeID GetTypeID() const
        { return m_ref->type_id; }

    template <class T>
    HYP_NODISCARD HYP_FORCE_INLINE RefCountedPtr<T, CountType> CastUnsafe() const
    {
        RefCountedPtr<T, CountType> rc;
        rc.m_ref = m_ref;
        rc.IncRefCount();

        return rc;
    }

    /*! \brief Drops the reference to the currently held value, if any.  */
    HYP_FORCE_INLINE void Reset()
        { DropRefCount(); }
    
    template <class Ty>
    HYP_FORCE_INLINE void Reset(Ty *ptr)
    {
        using TyN = NormalizedType<Ty>;

        DropRefCount();

        if (ptr) {
            if constexpr (std::is_base_of_v<EnableRefCountedPtrFromThisBase<CountType>, TyN>) {
                m_ref = ptr->template EnableRefCountedPtrFromThisBase<CountType>::weak.GetRefCountData_Internal();
            } else {
                m_ref = new RefCountDataType;
            }

            m_ref->template InitStrong<TyN>(ptr);
        }
    }

    /*! \brief Used by objects inheriting from this class or marshaling data. Not ideal to use externally */
    HYP_FORCE_INLINE RefCountDataType *GetRefCountData_Internal() const
        { return m_ref; }

    /*! \brief Sets the internal reference to the given RefCountDataType. Only for internal use. */
    HYP_FORCE_INLINE void SetRefCountData_Internal(RefCountDataType *ref, bool inc_ref = true)
    {
        DropRefCount();
        m_ref = ref;

        if (inc_ref) {
            IncRefCount();
        }
    }

    /*! \brief Releases the reference to the currently held value, if any, and returns it.
        * The caller is responsible for handling the reference count of the returned value.
    */
    HYP_NODISCARD HYP_FORCE_INLINE RefCountDataType *Release()
    {
        RefCountDataType *ref = m_ref;
        m_ref = const_cast<RefCountDataType *>(&null_ref__internal);

        return ref;
    }

protected:
    explicit RefCountedPtrBase(RefCountDataType *ref)
        : m_ref(ref)
    {
    }
    
    HYP_FORCE_INLINE void IncRefCount()
    {
#ifdef HYP_DEBUG_MODE
        AssertThrow(m_ref != nullptr);
#endif

        if (!m_ref->HasValue()) {
            return;
        }

        m_ref->IncRefCount_Strong();
    }
    
    void DropRefCount()
    {
        if (m_ref->HasValue()) {
#ifdef HYP_DEBUG_MODE
            AssertThrow(m_ref != nullptr);
#endif
            if (m_ref->DecRefCount_Strong() == 0u) {
                m_ref->Destruct();

                if (m_ref->UseCount_Weak() == 0u) {
                    delete m_ref;
                }
            }

            m_ref = const_cast<RefCountDataType *>(&null_ref__internal);
        }
    }

    RefCountDataType    *m_ref;
};

template <class CountType>
const typename RefCountedPtrBase<CountType>::RefCountDataType RefCountedPtrBase<CountType>::null_ref__internal = { };

template <class CountType>
class WeakRefCountedPtrBase
{
public:
    using RefCountDataType = detail::RefCountData<CountType>;

    WeakRefCountedPtrBase()
        : m_ref(const_cast<RefCountDataType *>(&RefCountedPtrBase<CountType>::null_ref__internal))
    {
    }

    WeakRefCountedPtrBase(const RefCountedPtrBase<CountType> &other)
        : m_ref(other.m_ref)
    {
        IncRefCount();
    }

    WeakRefCountedPtrBase(const WeakRefCountedPtrBase &other)
        : m_ref(other.m_ref)
    {
        IncRefCount();
    }

    WeakRefCountedPtrBase &operator=(const RefCountedPtrBase<CountType> &other)
    {
        DropRefCount();

        m_ref = other.m_ref;
        
        IncRefCount();

        return *this;
    }

    WeakRefCountedPtrBase &operator=(const WeakRefCountedPtrBase &other)
    {
        DropRefCount();

        m_ref = other.m_ref;

        IncRefCount();

        return *this;
    }

    WeakRefCountedPtrBase(WeakRefCountedPtrBase &&other) noexcept
        : m_ref(other.m_ref)
    {
        other.m_ref = const_cast<RefCountDataType *>(&RefCountedPtrBase<CountType>::null_ref__internal);
    }

    WeakRefCountedPtrBase &operator=(WeakRefCountedPtrBase &&other) noexcept
    {
        if (std::addressof(other) == this) {
            return *this;
        }

        DropRefCount();

        m_ref = other.m_ref;
        other.m_ref = const_cast<RefCountDataType *>(&RefCountedPtrBase<CountType>::null_ref__internal);

        return *this;
    }

    ~WeakRefCountedPtrBase()
    {
        DropRefCount();
    }
    
    HYP_FORCE_INLINE bool operator==(const RefCountedPtrBase<CountType> &other) const
        { return m_ref == other.m_ref; }
    
    HYP_FORCE_INLINE bool operator!=(const RefCountedPtrBase<CountType> &other) const
        { return m_ref != other.m_ref; }
    
    HYP_FORCE_INLINE bool operator==(const WeakRefCountedPtrBase &other) const
        { return m_ref == other.m_ref; }
    
    HYP_FORCE_INLINE bool operator!=(const WeakRefCountedPtrBase &other) const
        { return m_ref != other.m_ref; }

    /*! \brief Drops the reference to the currently held value, if any.  */
    HYP_FORCE_INLINE void Reset()
        { DropRefCount(); }
        
    /*! \brief Releases the reference to the currently held value, if any, and returns it.
        * The caller is responsible for handling the reference count of the returned value.
    */
    HYP_NODISCARD HYP_FORCE_INLINE RefCountDataType *Release()
    {
        RefCountDataType *ref = m_ref;
        m_ref = const_cast<RefCountDataType *>(&RefCountedPtrBase<CountType>::null_ref__internal);

        return ref;
    }

    template <class T>
    HYP_NODISCARD HYP_FORCE_INLINE WeakRefCountedPtr<T, CountType> CastUnsafe() const
    {
        WeakRefCountedPtr<T, CountType> rc;
        rc.SetRefCountData_Internal(m_ref, true);
        return rc;
    }

    /*! \brief Used by objects inheriting from this class or marshaling data. Not ideal to use externally */
    HYP_FORCE_INLINE RefCountDataType *GetRefCountData_Internal() const
        { return m_ref; }

    /*! \brief Sets the internal reference to the given RefCountDataType. Only for internal use. */
    HYP_FORCE_INLINE void SetRefCountData_Internal(RefCountDataType *ref, bool inc_ref = true)
    {
        DropRefCount();
        m_ref = ref;

        if (inc_ref) {
            IncRefCount();
        }
    }

protected:
    explicit WeakRefCountedPtrBase(RefCountDataType *ref)
        : m_ref(ref)
    {
    }

    HYP_FORCE_INLINE void IncRefCount()
    {
#ifdef HYP_DEBUG_MODE
        AssertThrow(m_ref != nullptr);
#endif

        if (!m_ref->HasValue()) {
            return;
        }

        m_ref->IncRefCount_Weak();
    }
    
    HYP_FORCE_INLINE void DropRefCount()
    {
        if (m_ref->HasValue()) {
            if (m_ref->DecRefCount_Weak() == 0u && m_ref->UseCount_Strong() == 0u) {
                delete m_ref;
            }

            m_ref = const_cast<RefCountDataType *>(&RefCountedPtrBase<CountType>::null_ref__internal);
        }
    }

    RefCountDataType    *m_ref;
};

/*! \brief A simple ref counted pointer class.
    Not atomic by default, but using AtomicRefCountedPtr allows it to be. */
template <class T, class CountType>
class RefCountedPtr : public RefCountedPtrBase<CountType>
{
    friend class WeakRefCountedPtr<std::remove_const_t<T>, CountType>;
    friend class WeakRefCountedPtr<std::add_const_t<T>, CountType>;

protected:
    using Base = RefCountedPtrBase<CountType>;

public:
    template <class ...Args>
    static RefCountedPtr Construct(Args &&... args)
    {
        return RefCountedPtr(new T(std::forward<Args>(args)...));
    }

    RefCountedPtr()
        : Base()
    {
    }

    RefCountedPtr(std::nullptr_t)
        : Base()
    {
    }

    /*! \brief Takes ownership of ptr. Do not delete the pointer passed to this,
        as it will be automatically deleted when this object's ref count reaches zero. */
    template <class Ty>
    explicit RefCountedPtr(Ty *ptr)
        : Base()
    {
        using TyN = NormalizedType<Ty>;
        static_assert(std::is_convertible_v<std::add_pointer_t<TyN>, std::add_pointer_t<T>>, "Ty must be convertible to T!");

        Reset<Ty>(ptr);
    }
    
    // delete parent constructors
    RefCountedPtr(const Base &) = delete;
    RefCountedPtr &operator=(const Base &) = delete;
    RefCountedPtr(Base &&) noexcept = delete;
    RefCountedPtr &operator=(Base &&) noexcept = delete;

    RefCountedPtr(const RefCountedPtr &other)
        : Base(static_cast<const Base &>(other))
    {
    }

    RefCountedPtr &operator=(const RefCountedPtr &other)
    {
        Base::operator=(static_cast<const Base &>(other));

        return *this;
    }

    RefCountedPtr(RefCountedPtr &&other) noexcept
        : Base(static_cast<Base &&>(std::move(other)))
    {
    }

    RefCountedPtr &operator=(RefCountedPtr &&other) noexcept
    {
        Base::operator=(static_cast<Base &&>(std::move(other)));

        return *this;
    }

    template <class Ty, std::enable_if_t<std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, int> = 0>
    RefCountedPtr(const RefCountedPtr<Ty, CountType> &other)
        : Base(static_cast<const Base &>(other))
    {
        static_assert(std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, "Types not compatible for upcast!");
    }
    
    template <class Ty, std::enable_if_t<std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, int> = 0>
    RefCountedPtr &operator=(const RefCountedPtr<Ty, CountType> &other)
    {
        static_assert(std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, "Types not compatible for upcast!");

        Base::operator=(static_cast<const Base &>(other));

        return *this;
    }

    template <class Ty, std::enable_if_t<std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, int> = 0>
    RefCountedPtr(RefCountedPtr<Ty, CountType> &&other) noexcept
        : Base(static_cast<Base &&>(std::move(other)))
    {
        static_assert(std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, "Types not compatible for upcast!");
    }
    
    template <class Ty, std::enable_if_t<std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, int> = 0>
    RefCountedPtr &operator=(RefCountedPtr<Ty, CountType> &&other) noexcept
    {
        static_assert(std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, "Types not compatible for upcast!");

        Base::operator=(static_cast<Base &&>(std::move(other)));

        return *this;
    }

    ~RefCountedPtr() = default;
    
    HYP_FORCE_INLINE operator T *() const
        { return Get(); }

    HYP_FORCE_INLINE T *Get() const
        { return static_cast<T *>(Base::m_ref->value); }
    
    HYP_FORCE_INLINE T *operator->() const
        { return Get(); }
    
    HYP_FORCE_INLINE T &operator*() const
        { return *Get(); }
    
    HYP_FORCE_INLINE explicit operator bool() const
        { return Base::operator bool(); }
    
    HYP_FORCE_INLINE bool operator!() const
        { return Base::operator!(); }
    
    HYP_FORCE_INLINE bool operator==(const RefCountedPtr &other) const
        { return Base::operator==(other); }
    
    HYP_FORCE_INLINE bool operator==(std::nullptr_t) const
        { return Base::operator==(nullptr); }
    
    HYP_FORCE_INLINE bool operator!=(const RefCountedPtr &other) const
        { return Base::operator!=(other); }

    HYP_FORCE_INLINE bool operator!=(std::nullptr_t) const
        { return Base::operator!=(nullptr); }

    /*! \brief Takes ownership of {ptr}, dropping the reference to the currently held value,
        if any. Note, do not delete the ptr after passing it to Reset(), as it will be deleted
        automatically. */
    template <class Ty>
    HYP_FORCE_INLINE void Reset(Ty *ptr)
    {
        using TyN = NormalizedType<Ty>;
        static_assert(std::is_convertible_v<std::add_pointer_t<TyN>, std::add_pointer_t<T>>, "Ty must be convertible to T!");

        Base::template Reset<Ty>(ptr);
    }

    /*! \brief Drops the reference to the currently held value, if any. */
    HYP_FORCE_INLINE void Reset()
        { Base::Reset(); }

    /*! \brief Returns a boolean indicating whether the type of this RefCountedPtr is the same as the given type, or if the given type is convertible to the type of this RefCountedPtr. */
    template <class Ty>
    HYP_FORCE_INLINE bool Is() const
    {
        return Base::GetTypeID() == TypeID::ForType<Ty>()
            || std::is_convertible_v<std::add_pointer_t<T>, std::add_pointer_t<Ty>>
            || std::is_same_v<Ty, void>;
    }

    /*! \brief Attempts to cast the pointer directly to the given type.
        If the types are not compatible (Derived -> Base) or equal (or T is not void, in the case of a void pointer),
        no cast is performed and a null RefCountedPtr is returned. Otherwise, a new RefCountedPtr is returned,
        with the currently held pointer casted to Ty as the held value. */
    template <class Ty>
    HYP_NODISCARD HYP_FORCE_INLINE RefCountedPtr<Ty, CountType> Cast() const
    {
        if (Is<Ty>()) {
            return Base::template CastUnsafe<Ty>();
        }

        return RefCountedPtr<Ty, CountType>();
    }

    HYP_NODISCARD HYP_FORCE_INLINE WeakRefCountedPtr<T, CountType> ToWeak() const
    {
        return WeakRefCountedPtr<T, CountType>(*this);
    }
};

// void pointer specialization -- just uses base class, but with Set() and Reset()
template <class CountType>
class RefCountedPtr<void, CountType> : public RefCountedPtrBase<CountType>
{
    friend class WeakRefCountedPtr<void, CountType>;

protected:
    using Base = RefCountedPtrBase<CountType>;

public:
    RefCountedPtr()
        : Base()
    {
    }

    RefCountedPtr(std::nullptr_t)
        : Base()
    {
    }
    
    // delete parent constructors
    RefCountedPtr(const Base &) = delete;
    RefCountedPtr &operator=(const Base &) = delete;
    RefCountedPtr(Base &&) noexcept = delete;
    RefCountedPtr &operator=(Base &&) noexcept = delete;

    template <class Ty>
    RefCountedPtr(const RefCountedPtr<Ty, CountType> &other)
        : Base(static_cast<const Base &>(other))
    {
    }

    template <class Ty>
    RefCountedPtr &operator=(const RefCountedPtr<Ty, CountType> &other)
    {
        Base::operator=(static_cast<const Base &>(other));

        return *this;
    }
    
    template <class Ty>
    RefCountedPtr(RefCountedPtr<Ty, CountType> &&other) noexcept
        : Base(std::move(other))
    {
    }
    
    template <class Ty>
    RefCountedPtr &operator=(RefCountedPtr<Ty, CountType> &&other) noexcept
    {
        Base::operator=(std::move(other));

        return *this;
    }

    ~RefCountedPtr() = default;

    HYP_FORCE_INLINE operator void *() const
        { return Base::Get(); }
    
    HYP_FORCE_INLINE void *Get() const
        { return Base::Get(); }
    
    HYP_FORCE_INLINE explicit operator bool() const
        { return Base::operator bool(); }
    
    HYP_FORCE_INLINE bool operator!() const
        { return Base::operator!(); }
    
    HYP_FORCE_INLINE bool operator==(const RefCountedPtr &other) const
        { return Base::operator==(other); }
    
    HYP_FORCE_INLINE bool operator==(std::nullptr_t) const
        { return Base::operator==(nullptr); }
    
    HYP_FORCE_INLINE bool operator!=(const RefCountedPtr &other) const
        { return Base::operator!=(other); }
    
    HYP_FORCE_INLINE bool operator!=(std::nullptr_t) const
        { return Base::operator!=(nullptr); }

    /*! \brief Drops the reference to the currently held value, if any. */
    HYP_FORCE_INLINE void Reset()
        { Base::Reset(); }

    /*! \brief Takes ownership of {ptr}, dropping the reference to the currently held value,
        if any. Note, do not delete the ptr after passing it to Reset(), as it will be deleted
        automatically. */
    template <class Ty>
    HYP_FORCE_INLINE void Reset(Ty *ptr)
    {
        using TyN = NormalizedType<Ty>;

        Base::template Reset<Ty>(ptr);
    }

    /*! \brief Returns a boolean indicating whether the type of this RefCountedPtr is the same as the given type,
     *  or if the given type is convertible to the type of this RefCountedPtr. */
    template <class Ty>
    HYP_FORCE_INLINE bool Is() const
    {
        return Base::GetTypeID() == TypeID::ForType<Ty>()
            || std::is_same_v<Ty, void>;
    }

    /*! \brief Attempts to cast the pointer directly to the given type.
        If the types are not compatible (Derived -> Base) or equal (or T is not void, in the case of a void pointer),
        no cast is performed and a null RefCountedPtr is returned. Otherwise, a new RefCountedPtr is returned,
        with the currently held pointer casted to Ty as the held value. */
    template <class Ty>
    HYP_NODISCARD HYP_FORCE_INLINE RefCountedPtr<Ty, CountType> Cast() const
    {
        if (Is<Ty>()) {
            return Base::template CastUnsafe<Ty>();
        }

        return RefCountedPtr<Ty, CountType>();
    }

    HYP_NODISCARD HYP_FORCE_INLINE WeakRefCountedPtr<void, CountType> ToWeak() const
    {
        return WeakRefCountedPtr<void, CountType>(*this);
    }
};

// weak ref counters
template <class T, class CountType = std::atomic<uint>>
class WeakRefCountedPtr : public WeakRefCountedPtrBase<CountType>
{
protected:
    using Base = WeakRefCountedPtrBase<CountType>;

public:
    WeakRefCountedPtr()
        : Base()
    {
    }

    WeakRefCountedPtr(const WeakRefCountedPtr &other)
        : Base(other)
    {
    }

    WeakRefCountedPtr &operator=(const WeakRefCountedPtr &other)
    {
        Base::operator=(other);

        return *this;
    }

    WeakRefCountedPtr(const RefCountedPtr<T, CountType> &other)
        : Base(other)
    {
    }

    WeakRefCountedPtr &operator=(const RefCountedPtr<T, CountType> &other)
    {
        Base::operator=(other);

        return *this;
    }

    WeakRefCountedPtr(RefCountedPtr<T, CountType> &&other) noexcept = delete;
    WeakRefCountedPtr &operator=(RefCountedPtr<T, CountType> &&other) noexcept = delete;

    WeakRefCountedPtr(WeakRefCountedPtr &&other) noexcept
        : Base(std::move(other))
    {
    }

    WeakRefCountedPtr &operator=(WeakRefCountedPtr &&other) noexcept
    {
        Base::operator=(std::move(other));

        return *this;
    }

    ~WeakRefCountedPtr() = default;

    HYP_FORCE_INLINE bool operator==(const WeakRefCountedPtr &other) const
        { return Base::operator==(other); }

    HYP_FORCE_INLINE bool operator!=(const WeakRefCountedPtr &other) const
        { return Base::operator!=(other); }

    HYP_FORCE_INLINE bool operator==(const T *ptr) const
        { return Base::m_ref->value == ptr; }

    HYP_FORCE_INLINE bool operator!=(const T *ptr) const
        { return Base::m_ref->value != ptr; }

    HYP_FORCE_INLINE RefCountedPtr<T, CountType> Lock() const
    {
        RefCountedPtr<T, CountType> rc;

        if (Base::m_ref->UseCount_Strong() == 0) { // atomic, acquire for atomic RC pointers
            Base::m_ref->InitStrong(static_cast<T *>(Base::m_ref->value));

            // InitStrong increments reference count already
            rc.SetRefCountData_Internal(Base::m_ref, false /* inc_ref */);
        } else {
            rc.SetRefCountData_Internal(Base::m_ref, true /* inc_ref */);
        }

        return rc;
    }
};

// Weak<void> specialization
// Cannot be locked directly; must be cast to another type using `Cast<T>()` first.
template <class CountType>
class WeakRefCountedPtr<void, CountType> : public WeakRefCountedPtrBase<CountType>
{
protected:
    using Base = WeakRefCountedPtrBase<CountType>;

public:
    WeakRefCountedPtr()
        : Base()
    {
    }

    WeakRefCountedPtr(const WeakRefCountedPtr &other)
        : Base(other)
    {
    }

    WeakRefCountedPtr &operator=(const WeakRefCountedPtr &other)
    {
        Base::operator=(other);

        return *this;
    }

    WeakRefCountedPtr(const RefCountedPtr<void, CountType> &other)
        : Base(other)
    {
    }

    WeakRefCountedPtr &operator=(const RefCountedPtr<void, CountType> &other)
    {
        Base::operator=(other);

        return *this;
    }

    WeakRefCountedPtr(RefCountedPtr<void, CountType> &&other) noexcept              = delete;
    WeakRefCountedPtr &operator=(RefCountedPtr<void, CountType> &&other) noexcept   = delete;

    WeakRefCountedPtr(WeakRefCountedPtr &&other) noexcept
        : Base(std::move(other))
    {
    }

    WeakRefCountedPtr &operator=(WeakRefCountedPtr &&other) noexcept
    {
        Base::operator=(std::move(other));

        return *this;
    }

    ~WeakRefCountedPtr() = default;

    HYP_FORCE_INLINE bool operator==(const WeakRefCountedPtr &other) const
        { return Base::operator==(other); }

    HYP_FORCE_INLINE bool operator!=(const WeakRefCountedPtr &other) const
        { return Base::operator!=(other); }

    HYP_FORCE_INLINE bool operator==(const void *ptr) const
        { return Base::m_ref->value == ptr; }

    HYP_FORCE_INLINE bool operator!=(const void *ptr) const
        { return Base::m_ref->value != ptr; }
};

template <class CountType>
class EnableRefCountedPtrFromThisBase
{
public:
    friend struct detail::RefCountData<CountType>;
    friend class RefCountedPtrBase<CountType>;
    
protected:
    EnableRefCountedPtrFromThisBase() = default;

    EnableRefCountedPtrFromThisBase(const EnableRefCountedPtrFromThisBase &)
    {
        // Do not modify weak ptr
    }

    EnableRefCountedPtrFromThisBase &operator=(const EnableRefCountedPtrFromThisBase &)
    {
        // Do not modify weak ptr

        return *this;
    }

    ~EnableRefCountedPtrFromThisBase()
    {
        // // Delete control block
        // weak.SetRefCountData_Internal(const_cast<RefCountData<CountType> *>(&RefCountedPtrBase<CountType>::null_ref__internal), false);
    }

public:
    WeakRefCountedPtr<void, CountType>  weak;
};

/*! \brief Helper struct to convert raw pointers to RefCountedPtrs.
 *  For internal use only. */
template <class T, class CountType>
struct RawPtrToRefCountedPtrHelper
{
    HYP_NODISCARD HYP_FORCE_INLINE RefCountedPtr<T, CountType> operator()(T *ptr)
    {
        if (!ptr) {
            return RefCountedPtr<T, CountType>();
        }

        return ptr->RefCountedPtrFromThis();
    }

    HYP_NODISCARD HYP_FORCE_INLINE RefCountedPtr<const T, CountType> operator()(const T *ptr)
    {
        if (!ptr) {
            return RefCountedPtr<const T, CountType>();
        }

        return ptr->RefCountedPtrFromThis();
    }
};

/*! \brief Helper struct to convert raw pointers to WeakRefCountedPtrs.
 *  For internal use only. */
template <class T, class CountType>
struct RawPtrToWeakRefCountedPtrHelper
{
    HYP_NODISCARD HYP_FORCE_INLINE WeakRefCountedPtr<T, CountType> operator()(T *ptr)
    {
        if (!ptr) {
            return WeakRefCountedPtr<T, CountType>();
        }

        return ptr->WeakRefCountedPtrFromThis();
    }

    HYP_NODISCARD HYP_FORCE_INLINE WeakRefCountedPtr<const T, CountType> operator()(const T *ptr)
    {
        if (!ptr) {
            return WeakRefCountedPtr<const T, CountType>();
        }

        return ptr->WeakRefCountedPtrFromThis();
    }
};

template <class T, class CountType>
class EnableRefCountedPtrFromThis : public EnableRefCountedPtrFromThisBase<CountType>
{
    using Base = EnableRefCountedPtrFromThisBase<CountType>;

public:
    template <class OtherT, class OtherCountType>
    friend struct RawPtrToRefCountedPtrHelper;

    template <class OtherT, class OtherCountType>
    friend struct RawPtrToWeakRefCountedPtrHelper;

    EnableRefCountedPtrFromThis()
    {
        detail::RefCountData<CountType> *ref_count_data = new detail::RefCountData<CountType>;
        ref_count_data->template InitWeak<T>(static_cast<T *>(this));
    }

    ~EnableRefCountedPtrFromThis() = default;

    RefCountedPtr<T, CountType> RefCountedPtrFromThis()
        { return Base::weak.template CastUnsafe<T>().Lock(); }

    RefCountedPtr<const T, CountType> RefCountedPtrFromThis() const
        { return Base::weak.template CastUnsafe<const T>().Lock(); }

    WeakRefCountedPtr<T, CountType> WeakRefCountedPtrFromThis()
        { return Base::weak.template CastUnsafe<T>(); }

private:
    EnableRefCountedPtrFromThis(const EnableRefCountedPtrFromThis &)
    {
        // Do not modify weak ptr
    }

    EnableRefCountedPtrFromThis &operator=(const EnableRefCountedPtrFromThis &)
    {
        // Do not modify weak ptr

        return *this;
    }
};

} // namespace memory

template <class T, class CountType = std::atomic<uint>>
using Weak = memory::WeakRefCountedPtr<T, CountType>; 

template <class T, class CountType = std::atomic<uint>>
using RC = memory::RefCountedPtr<T, CountType>;

template <class CountType = std::atomic<uint>>
using EnableRefCountedPtrFromThisBase = memory::EnableRefCountedPtrFromThisBase<CountType>;

template <class T, class CountType = std::atomic<uint>>
using EnableRefCountedPtrFromThis = memory::EnableRefCountedPtrFromThis<T, CountType>;

template <class T, class CountType = std::atomic<uint>>
using RawPtrToRefCountedPtrHelper = memory::RawPtrToRefCountedPtrHelper<T, CountType>;

template <class T, class CountType = std::atomic<uint>>
using RawPtrToWeakRefCountedPtrHelper = memory::RawPtrToWeakRefCountedPtrHelper<T, CountType>;

} // namespace hyperion

#endif