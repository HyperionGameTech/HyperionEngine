/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_REF_COUNTED_PTR_HPP
#define HYPERION_REF_COUNTED_PTR_HPP

#include <core/Defines.hpp>

#include <core/utilities/ValueStorage.hpp>
#include <core/utilities/TypeID.hpp>

#include <core/memory/Memory.hpp>
#include <core/memory/AnyRef.hpp>

#include <core/threading/AtomicVar.hpp>

#include <core/object/HypObjectFwd.hpp>

#include <core/debug/Debug.hpp>

#include <Types.hpp>
#include <Constants.hpp>

#include <atomic>
#include <cstdlib>

namespace hyperion {

class HypClass;

extern HYP_API const HypClass *GetClass(TypeID type_id);
extern HYP_API bool IsInstanceOfHypClass(const HypClass *hyp_class, const void *ptr, TypeID type_id);

namespace memory {

template <class T, class CountType = AtomicVar<uint32>>
class EnableRefCountedPtrFromThis;

template <class T, class CountType>
class RefCountedPtr;

template <class T, class CountType>
class WeakRefCountedPtr;

template <class CountType>
class EnableRefCountedPtrFromThisBase;

template <class CountType>
class WeakRefCountedPtrBase;

template <class CountType>
class RefCountedPtrBase;

#ifndef HYP_DEBUG_MODE
    #define EnsureUninitialized()
#endif

namespace detail {

template <class T, class RefCountDataType>
static inline uint32 IncRefCount_Impl(void *ptr, RefCountDataType &ref_count_data, bool weak)
{
    uint32 count_value;

    if (!weak) {
        if constexpr (std::is_integral_v<typename RefCountDataType::Count>) {
            count_value = ++ref_count_data.strong_count;
        } else {
            count_value = ref_count_data.strong_count.Increment(1, MemoryOrder::ACQUIRE_RELEASE) + 1;
        }
        
        if constexpr (IsHypObject<T>::value) {
            HypObject_OnIncRefCount_Strong(static_cast<T *>(ptr), count_value);
        }
    } else {
        if constexpr (std::is_integral_v<typename RefCountDataType::Count>) {
            count_value = ++ref_count_data.weak_count;
        } else {
            count_value = ref_count_data.weak_count.Increment(1, MemoryOrder::ACQUIRE_RELEASE) + 1;
        }

        // if constexpr (IsHypObject<T>::value) {
        //     HypObject_OnIncRefCount_Weak(static_cast<T *>(ptr), count_value);
        // }
    }

    return count_value;
}

template <class T, class RefCountDataType>
static inline uint32 DecRefCount_Impl(void *ptr, RefCountDataType &ref_count_data, bool weak)
{
    uint32 count_value;

    if (!weak) {
        if constexpr (std::is_integral_v<typename RefCountDataType::Count>) {
            count_value = --ref_count_data.strong_count;
        } else {
            count_value = ref_count_data.strong_count.Decrement(1, MemoryOrder::ACQUIRE_RELEASE) - 1;
        }
        
        if constexpr (IsHypObject<T>::value) {
            HypObject_OnDecRefCount_Strong(static_cast<T *>(ptr), count_value);
        }
    } else {
        if constexpr (std::is_integral_v<typename RefCountDataType::Count>) {
            count_value = --ref_count_data.weak_count;
        } else {
            count_value = ref_count_data.weak_count.Decrement(1, MemoryOrder::ACQUIRE_RELEASE) - 1;
        }
        
        // if constexpr (IsHypObject<T>::value) {
        //     HypObject_OnDecRefCount_Weak(static_cast<T *>(ptr), count_value);
        // }
    }

    return count_value;
}

template <class CountType>
struct RefCountData
{
    using Count = CountType;

    void        *value;
    TypeID      type_id;
    Count       strong_count;
    Count       weak_count;
    void        (*dtor)(void *);

    uint32      (*inc_ref_count)(void *ptr, RefCountData &, bool weak);
    uint32      (*dec_ref_count)(void *ptr, RefCountData &, bool weak);

    RefCountData()
        : strong_count(0),
          weak_count(0)
    {
        value = nullptr;
        type_id = TypeID::Void();
        dtor = nullptr;
        inc_ref_count = nullptr;
        dec_ref_count = nullptr;
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
        AssertThrow(UseCount_Strong() == 0);
        AssertThrow(UseCount_Weak() == 0);
    }
#endif

    HYP_FORCE_INLINE bool HasValue() const
        { return value != nullptr; }

    HYP_FORCE_INLINE uint32 UseCount_Strong() const
    {
        if constexpr (std::is_integral_v<Count>) {
            return strong_count;
        } else {
            return strong_count.Get(MemoryOrder::ACQUIRE);
        }
    }

    HYP_FORCE_INLINE uint32 UseCount_Weak() const
    {
        if constexpr (std::is_integral_v<Count>) {
            return weak_count;
        } else {
            return weak_count.Get(MemoryOrder::ACQUIRE);
        }
    }

    template <class T>
    void Init(T *ptr)
    {
        using Normalized = NormalizedType<T>;

        static_assert(!std::is_void_v<T>, "Cannot initialize RefCountedPtr data with void pointer");

        // Setup weak ptr for EnableRefCountedPtrFromThis
        if constexpr (std::is_base_of_v<EnableRefCountedPtrFromThisBase<Count>, Normalized>) {
#ifdef HYP_DEBUG_MODE
            // Should already be set up from InitWeak() call.
            AssertThrow(value == ptr);
#endif
        } else {
#ifdef HYP_DEBUG_MODE
            AssertThrow(value == nullptr);
#endif

            value = ptr;
        }

// #ifdef HYP_DEBUG_MODE
//         AssertThrowMsg(UseCount_Strong() == 0, "Initializing RefCountedPtr but ptr is already owned by another RefCountedPtr object!");
// #endif

        // Override type_id, dtor for derived types
        type_id = TypeID::ForType<Normalized>();
        dtor = &Memory::Delete<Normalized>;
        inc_ref_count = &IncRefCount_Impl<T, RefCountData>;
        dec_ref_count = &DecRefCount_Impl<T, RefCountData>;
    }

    template <class T>
    void InitWeak(T *ptr)
    {
        using Normalized = NormalizedType<T>;

        static_assert(std::is_base_of_v<EnableRefCountedPtrFromThisBase<Count>, Normalized>, "T must derive EnableRefCountedPtrFromThis<T, CountType> to use this method");

        value = ptr;

        // Override type_id, dtor for derived types
        type_id = TypeID::ForType<Normalized>();
        dtor = &Memory::Delete<Normalized>;
        inc_ref_count = &IncRefCount_Impl<T, RefCountData>;
        dec_ref_count = &DecRefCount_Impl<T, RefCountData>;
    }

    void Destruct()
    {
#ifdef HYP_DEBUG_MODE
        AssertThrow(value != nullptr);
        AssertThrow(UseCount_Strong() == 0);
#endif

        void *current_value = value;
        value = nullptr;

        void (*current_dtor)(void *) = dtor;
        dtor = nullptr;

        inc_ref_count = nullptr;
        dec_ref_count = nullptr;

        type_id = TypeID::Void();

        current_dtor(current_value);
    }

    uint32 IncRefCount_Strong(void *ptr)
    {
        return inc_ref_count(ptr, *this, false);
    }

    uint32 DecRefCount_Strong(void *ptr)
    {
        uint32 value;

        if ((value = dec_ref_count(ptr, *this, false)) == 0u) {
            Destruct();

            if (UseCount_Weak() == 0u) {
                delete this;
            }
        }

        return value;
    }

    uint32 IncRefCount_Weak(void *ptr)
    {
        return inc_ref_count(ptr, *this, true);
    }

    uint32 DecRefCount_Weak(void *ptr)
    {
        uint32 value;

        if ((value = dec_ref_count(ptr, *this, true)) == 0u && UseCount_Strong() == 0u) {
            delete this;
        }

        return value;
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
    
    static const RefCountDataType empty_ref_count_data;

    RefCountedPtrBase()
        : m_ref(nullptr),
          m_ptr(nullptr)
    {
    }

protected:

    RefCountedPtrBase(const RefCountedPtrBase &other)
        : m_ref(other.m_ref),
          m_ptr(other.m_ptr)
    {
        IncRefCount();
    }

    RefCountedPtrBase &operator=(const RefCountedPtrBase &other)
    {
        if (std::addressof(other) == this || m_ref == other.m_ref) {
            m_ptr = other.m_ptr;

            return *this;
        }

        DecRefCount();

        m_ref = other.m_ref;
        m_ptr = other.m_ptr;

        IncRefCount();

        return *this;
    }

    RefCountedPtrBase(RefCountedPtrBase &&other) noexcept
        : m_ref(other.m_ref),
          m_ptr(other.m_ptr)
    {
        // NOTE: Cast away constness -- modifying empty_ref_count_data or dereferencing its value (always nullptr) shouldn't happen anyway.
        other.m_ref = nullptr;
        other.m_ptr = nullptr;
    }

    RefCountedPtrBase &operator=(RefCountedPtrBase &&other) noexcept
    {
        if (std::addressof(other) == this || m_ref == other.m_ref) {
            m_ptr = other.m_ptr;
            other.m_ptr = nullptr;

            return *this;
        }

        DecRefCount();

        m_ref = other.m_ref;
        m_ptr = other.m_ptr;

        other.m_ref = nullptr;
        other.m_ptr = nullptr;

        return *this;
    }

public:
    ~RefCountedPtrBase()
    {
        DecRefCount();
    }

    HYP_FORCE_INLINE void *Get() const
        { return m_ptr; }
    
    HYP_FORCE_INLINE TypeID GetTypeID() const
        { return m_ref ? m_ref->type_id : TypeID::Void(); }

    /*! \brief Returns true if no value has been assigned to the reference counted pointer. */
    HYP_FORCE_INLINE bool Empty() const
        { return m_ref ? m_ref->UseCount_Strong() == 0 : true; }

    /*! \brief Returns true if a value has been assigned to the reference counted pointer. */
    HYP_FORCE_INLINE bool Any() const
        { return m_ref ? m_ref->UseCount_Strong() != 0 : false; }

    /*! \brief Returns true if the RefCountedPtr has been assigned a value. */
    HYP_FORCE_INLINE bool IsValid() const
        { return m_ref && m_ref->HasValue(); }

    /*! \brief Does an unsafe cast to the given type. Since we only hold a void pointer, no pointer adjustment is done. */
    template <class Ty>
    HYP_NODISCARD HYP_FORCE_INLINE RefCountedPtr<Ty, CountType> CastUnsafe_Internal() const &
    {
        RefCountedPtr<Ty, CountType> rc;
        rc.SetRefCountData_Internal(m_ptr, m_ref, /* inc_ref */ true);

        return rc;
    }

    /*! \brief Does an unsafe cast to the given type. Since we only hold a void pointer, no pointer adjustment is done. */
    template <class Ty>
    HYP_NODISCARD HYP_FORCE_INLINE RefCountedPtr<Ty, CountType> CastUnsafe_Internal() &&
    {
        RefCountedPtr<Ty, CountType> rc;
        Swap(rc.m_ptr, m_ptr);
        Swap(rc.m_ref, m_ref);

        return rc;
    }

    /*! \brief Drops the reference to the currently held value, if any.  */
    HYP_FORCE_INLINE void Reset()
    {
        DecRefCount();
    }
    
    template <class T>
    HYP_FORCE_INLINE void Reset(T *ptr)
    {
        DecRefCount();

        if (ptr) {
            m_ptr = ptr;

            if constexpr (std::is_base_of_v<EnableRefCountedPtrFromThisBase<CountType>, NormalizedType<T>>) {
                // T has EnableRefCountedPtrFromThisBase as a base class -- share the RefCountData and just increment the refcount

                m_ref = ptr->template EnableRefCountedPtrFromThisBase<CountType>::weak_this.GetRefCountData_Internal();
            } else {
                // Initialize new RefCountData
                m_ref = new RefCountDataType;
            }

            if (detail::IncRefCount_Impl<NormalizedType<T>, RefCountDataType>(ptr, *m_ref, /* weak */ false) == 1) {
                m_ref->template Init<NormalizedType<T>>(ptr);
            }
        }
    }
    
    HYP_NODISCARD HYP_FORCE_INLINE AnyRef ToRef() const
    {
        return AnyRef(GetTypeID(), m_ptr);
    }

    /*! \brief Used by objects inheriting from this class or marshaling data. Not ideal to use externally */
    HYP_FORCE_INLINE RefCountDataType *GetRefCountData_Internal() const
        { return m_ref; }

    /*! \brief Sets the internal reference to the given RefCountDataType. Only for internal use. */
    HYP_FORCE_INLINE void SetRefCountData_Internal(void *ptr, RefCountDataType * HYP_NOTNULL ref, bool inc_ref = true)
    {
        DecRefCount();

        m_ref = ref;
        m_ptr = ptr;

        if (inc_ref) {
            IncRefCount();
        }
    }

    HYP_NODISCARD HYP_FORCE_INLINE void *Release_Internal()
    {
        void *ptr = m_ptr;
        m_ref = nullptr;
        m_ptr = nullptr;

        return ptr;
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(m_ptr);
    }

protected:
    HYP_FORCE_INLINE void IncRefCount()
    {
        if (!IsValid()) {
            return;
        }

        m_ref->IncRefCount_Strong(m_ptr);
    }
    
    void DecRefCount()
    {
        if (IsValid()) {
            m_ref->DecRefCount_Strong(m_ptr);
        }

        m_ref = nullptr;
        m_ptr = nullptr;
    }

    void                *m_ptr;
    RefCountDataType    *m_ref;
};

template <class CountType>
const typename RefCountedPtrBase<CountType>::RefCountDataType RefCountedPtrBase<CountType>::empty_ref_count_data = { };

template <class CountType>
class WeakRefCountedPtrBase
{
    friend class RefCountedPtrBase<CountType>;

public:
    using RefCountDataType = detail::RefCountData<CountType>;

    WeakRefCountedPtrBase()
        : m_ref(nullptr),
          m_ptr(nullptr)
    {
    }

    WeakRefCountedPtrBase(const RefCountedPtrBase<CountType> &other)
        : m_ref(other.m_ref),
          m_ptr(other.m_ptr)
    {
        IncRefCount();
    }

    WeakRefCountedPtrBase(const WeakRefCountedPtrBase &other)
        : m_ref(other.m_ref),
          m_ptr(other.m_ptr)
    {
        IncRefCount();
    }

    WeakRefCountedPtrBase &operator=(const RefCountedPtrBase<CountType> &other)
    {
        DecRefCount();

        m_ref = other.m_ref;
        m_ptr = other.m_ptr;
        
        IncRefCount();

        return *this;
    }

    WeakRefCountedPtrBase &operator=(const WeakRefCountedPtrBase &other)
    {
        if (std::addressof(other) == this || m_ref == other.m_ref) {
            m_ptr = other.m_ptr;
            return *this;
        }

        DecRefCount();

        m_ref = other.m_ref;
        m_ptr = other.m_ptr;

        IncRefCount();

        return *this;
    }

    WeakRefCountedPtrBase(WeakRefCountedPtrBase &&other) noexcept
        : m_ref(other.m_ref),
          m_ptr(other.m_ptr)
    {
        other.m_ref = nullptr;
        other.m_ptr = nullptr;
    }

    WeakRefCountedPtrBase &operator=(WeakRefCountedPtrBase &&other) noexcept
    {
        if (std::addressof(other) == this || m_ref == other.m_ref) {
            m_ptr = other.m_ptr;
            other.m_ptr = nullptr;

            return *this;
        }

        DecRefCount();

        m_ref = other.m_ref;
        other.m_ref = nullptr;

        m_ptr = other.m_ptr;
        other.m_ptr = nullptr;

        return *this;
    }

    ~WeakRefCountedPtrBase()
    {
        DecRefCount();
    }

    /*! \brief Drops the reference to the currently held value, if any.  */
    HYP_FORCE_INLINE void Reset()
        { DecRefCount(); }

    /*! \brief Returns true if no value has been assigned to the weak reference counted pointer. */
    HYP_FORCE_INLINE bool Empty() const
        { return m_ref ? m_ref->UseCount_Weak() == 0 : true; }

    /*! \brief Returns true if a value has been assigned to the weak reference counted pointer. */
    HYP_FORCE_INLINE bool Any() const
        { return m_ref ? m_ref->UseCount_Weak() != 0 : false; }

    /*! \brief Returns whether or not the WeakRefCountedPtr has been assigned a value.
     *  \note This does not check whether the referenced object still exists, just tests if any value has been assigned. */
    HYP_FORCE_INLINE bool IsValid() const
        { return m_ref && m_ref->HasValue(); }

    template <class T>
    HYP_NODISCARD HYP_FORCE_INLINE WeakRefCountedPtr<T, CountType> CastUnsafe_Internal() const
    {
        WeakRefCountedPtr<T, CountType> rc;
        rc.SetRefCountData_Internal(m_ptr, m_ref, true);
        return rc;
    }

    /*! \brief Used by objects inheriting from this class or marshaling data. Not ideal to use externally */
    HYP_FORCE_INLINE RefCountDataType *GetRefCountData_Internal() const
        { return m_ref; }

    /*! \brief Sets the internal reference to the given RefCountDataType. Only for internal use. */
    HYP_FORCE_INLINE void SetRefCountData_Internal(void *ptr, RefCountDataType * HYP_NOTNULL ref, bool inc_ref = true)
    {
        DecRefCount();
        
        m_ref = ref;
        m_ptr = ptr;

        if (inc_ref) {
            IncRefCount();
        }
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(m_ptr);
    }

protected:
    HYP_FORCE_INLINE void IncRefCount()
    {
        if (!IsValid()) {
            return;
        }

        m_ref->IncRefCount_Weak(m_ptr);
    }
    
    HYP_FORCE_INLINE void DecRefCount()
    {
        if (IsValid()) {
            m_ref->DecRefCount_Weak(m_ptr);
        }

        m_ref = nullptr;
        m_ptr = nullptr;
    }

    void                *m_ptr;
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

    /*! \brief Takes ownership of ptr. Do not delete the pointer passed to this,
        as it will be automatically deleted when this object's ref count reaches zero. */
    template <class Ty>
    explicit RefCountedPtr(Ty *ptr)
        : Base()
    {
        using TyN = NormalizedType<Ty>;
        static_assert(std::is_convertible_v<std::add_pointer_t<TyN>, std::add_pointer_t<T>>, "Ty must be convertible to T!");

        Base::template Reset<Ty>(ptr);
    }

public:
    template <class... Args>
    static RefCountedPtr Construct(Args &&... args)
    {
        static_assert(std::is_constructible_v<T, Args...>, "T must be constructible using the given args");
    
        RefCountedPtr rc;

        if constexpr (IsHypObject<T>::value) {
            rc.Reset(Memory::AllocateAndConstructWithContext<T, HypObjectInitializerGuard<T>>(std::forward<Args>(args)...));
        } else {
            rc.Reset(Memory::AllocateAndConstruct<T>(std::forward<Args>(args)...));
        }
        
        return rc;
    }

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

    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, int> = 0>
    RefCountedPtr(const RefCountedPtr<Ty, CountType> &other)
        : Base(static_cast<const Base &>(other))
    {
        static_assert(std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, "Types not compatible for upcast!");
    }
    
    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> &&std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, int> = 0>
    RefCountedPtr &operator=(const RefCountedPtr<Ty, CountType> &other)
    {
        static_assert(std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, "Types not compatible for upcast!");

        Base::operator=(static_cast<const Base &>(other));

        return *this;
    }

    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, int> = 0>
    RefCountedPtr(RefCountedPtr<Ty, CountType> &&other) noexcept
        : Base(static_cast<Base &&>(std::move(other)))
    {
        static_assert(std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, "Types not compatible for upcast!");
    }
    
    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, int> = 0>
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
        { return static_cast<T *>(Base::m_ptr); }
    
    HYP_FORCE_INLINE T *operator->() const
        { return Get(); }
    
    HYP_FORCE_INLINE T &operator*() const
        { return *Get(); }
    
    HYP_FORCE_INLINE explicit operator bool() const
        { return Base::m_ptr != nullptr; }
    
    HYP_FORCE_INLINE bool operator!() const
        { return Base::m_ptr == nullptr; }
    
    HYP_FORCE_INLINE bool operator==(const RefCountedPtr &other) const
        { return Base::m_ptr == other.m_ptr; }
    
    HYP_FORCE_INLINE bool operator==(const WeakRefCountedPtr<T, CountType> &other) const
        { return Base::m_ptr == other.m_ptr; }
    
    HYP_FORCE_INLINE bool operator==(std::nullptr_t) const
        { return Base::m_ptr == nullptr; }
    
    HYP_FORCE_INLINE bool operator==(const T *ptr) const
        { return Base::m_ptr == ptr; }
    
    HYP_FORCE_INLINE bool operator==(T *ptr) const
        { return Base::m_ptr == ptr; }
    
    HYP_FORCE_INLINE bool operator!=(const RefCountedPtr &other) const
        { return Base::m_ptr != other.m_ptr; }
    
    HYP_FORCE_INLINE bool operator!=(const WeakRefCountedPtr<T, CountType> &other) const
        { return Base::m_ptr != other.m_ptr; }
    
    HYP_FORCE_INLINE bool operator!=(std::nullptr_t) const
        { return Base::m_ptr != nullptr; }
    
    HYP_FORCE_INLINE bool operator!=(T *ptr) const
        { return Base::m_ptr != ptr; }
    
    HYP_FORCE_INLINE bool operator!=(const T *ptr) const
        { return Base::m_ptr != ptr; }

    HYP_FORCE_INLINE bool operator<(const RefCountedPtr &other) const
        { return Base::m_ptr < other.m_ptr; }

    HYP_FORCE_INLINE bool operator<(const WeakRefCountedPtr<T, CountType> &other) const
        { return Base::m_ptr < other.m_ptr; }

    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, int> = 0>
    HYP_FORCE_INLINE explicit operator RefCountedPtr<Ty, CountType> &()
    {
        static_assert(std::is_convertible_v<std::add_pointer_t<T>, std::add_pointer_t<Ty>>, "T must be convertible to Ty!");

        return reinterpret_cast<RefCountedPtr<Ty, CountType> &>(*this);
    }

    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, int> = 0>
    HYP_FORCE_INLINE explicit operator const RefCountedPtr<Ty, CountType> &() const
    {
        static_assert(std::is_convertible_v<std::add_pointer_t<T>, std::add_pointer_t<Ty>>, "T must be convertible to Ty!");

        return reinterpret_cast<const RefCountedPtr<Ty, CountType> &>(*this);
    }

    /*! \brief Takes ownership of {ptr}, dropping the reference to the currently held value,
        if any. Note, do not delete the ptr after passing it to Reset(), as it will be deleted
        automatically. */
    template <class Ty>
    HYP_FORCE_INLINE void Reset(Ty *ptr)
    {
        static_assert(std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, "Ty must be convertible to T!");

        Base::template Reset<Ty>(ptr);
    }

    /*! \brief Drops the reference to the currently held value, if any. */
    HYP_FORCE_INLINE void Reset()
        { Base::Reset(); }

    /*! \brief Like Reset(), but constructs the object in-place. */
    template <class... Args>
    HYP_FORCE_INLINE RefCountedPtr &Emplace(Args &&... args)
    {
        return (*this = Construct(std::forward<Args>(args)...));
    }

    /*! \brief Like Emplace() but the first template parameter is specified as the type to construct. */
    template <class Ty, class... Args>
    HYP_FORCE_INLINE RefCountedPtr &EmplaceAs(Args &&... args)
    {
        static_assert(std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, "Ty must be convertible to T!");

        return (*this = RefCountedPtr<Ty, CountType>::Construct(std::forward<Args>(args)...));
    }

    /*! \brief Returns a boolean indicating whether the type of this RefCountedPtr is the same as the given type, or if the given type is convertible to the type of this RefCountedPtr. */
    template <class Ty>
    HYP_FORCE_INLINE bool Is() const
    {
        if constexpr (std::is_void_v<Ty>) {
            return true;
        } else {
            static_assert(
                std::is_convertible_v<std::add_pointer_t<T>, std::add_pointer_t<Ty>> || std::is_base_of_v<T, Ty>,
                "Is<T> check is invalid; will never be true"
            );

            constexpr TypeID type_id = TypeID::ForType<Ty>();

            return Base::GetTypeID() == type_id
                || std::is_convertible_v<std::add_pointer_t<T>, std::add_pointer_t<Ty>>
                || IsInstanceOfHypClass(GetClass(type_id), Base::m_ptr, Base::GetTypeID())
                || (std::is_polymorphic_v<Ty> && dynamic_cast<Ty *>(static_cast<T *>(Base::m_ptr)) != nullptr);
        }
    }

    template <class Ty>
    HYP_NODISCARD HYP_FORCE_INLINE RefCountedPtr<Ty, CountType> CastUnsafe() const &
    {
        RefCountedPtr<Ty, CountType> rc;

        if (Base::IsValid()) {
            rc.SetRefCountData_Internal(static_cast<Ty *>(Get()), Base::m_ref, /* inc_ref */ true);
        }

        return rc;
    }

    template <class Ty>
    HYP_NODISCARD HYP_FORCE_INLINE RefCountedPtr<Ty, CountType> CastUnsafe() &&
    {
        RefCountedPtr<Ty, CountType> rc;

        if (Base::IsValid()) {
            rc.SetRefCountData_Internal(static_cast<Ty *>(Get()), Base::m_ref, /* inc_ref */ false);

            Base::m_ref = nullptr;
            Base::m_ptr = nullptr;
        }

        return rc;
    }

    /*! \brief Attempts to cast the pointer directly to the given type.
        If the types are not compatible (Derived -> Base) or equal (or T is not void, in the case of a void pointer),
        no cast is performed and a null RefCountedPtr is returned. Otherwise, a new RefCountedPtr is returned,
        with the currently held pointer casted to Ty as the held value. */
    template <class Ty>
    HYP_NODISCARD HYP_FORCE_INLINE RefCountedPtr<Ty, CountType> Cast() const
    {
        if (Is<Ty>()) {
            RefCountedPtr<Ty, CountType> rc;
            rc.SetRefCountData_Internal(static_cast<Ty *>(Get()), Base::m_ref, /* inc_ref */ true);

            return rc;
        }

        return RefCountedPtr<Ty, CountType>();
    }

    /*! \brief Releases the reference to the currently held value, if any, and returns it.
        * The caller is responsible for handling the reference count of the returned value.
    */
    HYP_NODISCARD HYP_FORCE_INLINE T *Release()
    {
        return static_cast<T *>(Base::Release_Internal());
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
    RefCountedPtr(const Base &)                 = delete;
    RefCountedPtr &operator=(const Base &)      = delete;
    RefCountedPtr(Base &&) noexcept             = delete;
    RefCountedPtr &operator=(Base &&) noexcept  = delete;

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
        { return Base::m_ptr != nullptr; }
    
    HYP_FORCE_INLINE bool operator!() const
        { return Base::m_ptr == nullptr; }
    
    HYP_FORCE_INLINE bool operator==(const RefCountedPtr &other) const
        { return Base::m_ptr == other.m_ptr; }
    
    HYP_FORCE_INLINE bool operator==(const WeakRefCountedPtr<void, CountType> &other) const
        { return Base::m_ptr == other.m_ptr; }
    
    HYP_FORCE_INLINE bool operator==(std::nullptr_t) const
        { return Base::m_ptr == nullptr; }
    
    HYP_FORCE_INLINE bool operator==(void *ptr) const
        { return Base::m_ptr == ptr; }
    
    HYP_FORCE_INLINE bool operator==(const void *ptr) const
        { return Base::m_ptr == ptr; }
    
    HYP_FORCE_INLINE bool operator!=(const RefCountedPtr &other) const
        { return Base::m_ptr != other.m_ptr; }
    
    HYP_FORCE_INLINE bool operator!=(const WeakRefCountedPtr<void, CountType> &other) const
        { return Base::m_ptr != other.m_ptr; }
    
    HYP_FORCE_INLINE bool operator!=(std::nullptr_t) const
        { return Base::m_ptr != nullptr; }
    
    HYP_FORCE_INLINE bool operator!=(void *ptr) const
        { return Base::m_ptr != ptr; }
    
    HYP_FORCE_INLINE bool operator!=(const void *ptr) const
        { return Base::m_ptr != ptr; }

    HYP_FORCE_INLINE bool operator<(const RefCountedPtr &other) const
        { return Base::m_ptr < other.m_ptr; }

    HYP_FORCE_INLINE bool operator<(const WeakRefCountedPtr<void, CountType> &other) const
        { return Base::m_ptr < other.m_ptr; }

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
        constexpr TypeID type_id = TypeID::ForType<Ty>();

        return std::is_same_v<Ty, void>
            || Base::GetTypeID() == type_id
            || IsInstanceOfHypClass(GetClass(type_id), Base::m_ptr, Base::GetTypeID());
    }

    /*! \brief Releases the reference to the currently held value, if any, and returns it.
     *  The caller is responsible for managing the control block of the returned value.
     *  Reference count is not modified.
     *  \internal For internal use only, used for script bindings */
    HYP_NODISCARD HYP_FORCE_INLINE void *Release()
    {
        return Base::Release_Internal();
    }

    HYP_NODISCARD HYP_FORCE_INLINE WeakRefCountedPtr<void, CountType> ToWeak() const
    {
        return WeakRefCountedPtr<void, CountType>(*this);
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(Base::m_ref.Get());
    }
};

// weak ref counters
template <class T, class CountType = AtomicVar<uint32>>
class WeakRefCountedPtr : public WeakRefCountedPtrBase<CountType>
{
    friend class RefCountedPtr<T, CountType>;

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
        { return Base::m_ptr == other.m_ptr; }

    HYP_FORCE_INLINE bool operator==(const RefCountedPtr<T, CountType> &other) const
        { return Base::m_ptr == other.m_ptr; }

    HYP_FORCE_INLINE bool operator!=(const WeakRefCountedPtr &other) const
        { return Base::m_ptr != other.m_ptr; }

    HYP_FORCE_INLINE bool operator!=(const RefCountedPtr<T, CountType> &other) const
        { return Base::m_ptr != other.m_ptr; }

    HYP_FORCE_INLINE bool operator<(const RefCountedPtr<T, CountType> &other) const
        { return Base::m_ptr < other.m_ptr; }

    HYP_FORCE_INLINE bool operator<(const WeakRefCountedPtr &other) const
        { return Base::m_ptr < other.m_ptr; }

    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, int> = 0>
    HYP_FORCE_INLINE explicit operator WeakRefCountedPtr<Ty, CountType> &()
    {
        static_assert(std::is_convertible_v<std::add_pointer_t<T>, std::add_pointer_t<Ty>>, "T must be convertible to Ty!");

        return reinterpret_cast<WeakRefCountedPtr<Ty, CountType> &>(*this);
    }

    template <class Ty, std::enable_if_t<!std::is_same_v<Ty, T> && std::is_convertible_v<std::add_pointer_t<Ty>, std::add_pointer_t<T>>, int> = 0>
    HYP_FORCE_INLINE explicit operator const WeakRefCountedPtr<Ty, CountType> &() const
    {
        static_assert(std::is_convertible_v<std::add_pointer_t<T>, std::add_pointer_t<Ty>>, "T must be convertible to Ty!");

        return reinterpret_cast<const WeakRefCountedPtr<Ty, CountType> &>(*this);
    }

    /*! \brief Gets a pointer to the value held by the reference counted pointer.
     *  \note Use sparringly. This method does not lock the reference -- the object may be deleted from another thread while using it */
    HYP_FORCE_INLINE T *GetUnsafe() const
        { return static_cast<T *>(Base::m_ptr); }

    HYP_FORCE_INLINE RefCountedPtr<T, CountType> Lock() const
    {
        RefCountedPtr<T, CountType> rc;

        if (Base::IsValid()) {
            if (Base::m_ref->UseCount_Strong() == 0) {
                return rc;
            }

            rc.SetRefCountData_Internal(Base::m_ptr, Base::m_ref, true /* inc_ref */);
        }

        return rc;
    }
};

// Weak<void> specialization
// Cannot be locked directly; must be cast to another type using `Cast<T>()` first.
template <class CountType>
class WeakRefCountedPtr<void, CountType> : public WeakRefCountedPtrBase<CountType>
{
    friend class RefCountedPtr<void, CountType>;

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
        { return Base::m_ptr == other.m_ptr; }

    HYP_FORCE_INLINE bool operator==(const RefCountedPtr<void, CountType> &other) const
        { return Base::m_ptr == other.m_ptr; }

    HYP_FORCE_INLINE bool operator!=(const WeakRefCountedPtr &other) const
        { return Base::m_ptr != other.m_ptr; }

    HYP_FORCE_INLINE bool operator!=(const RefCountedPtr<void, CountType> &other) const
        { return Base::m_ptr != other.m_ptr; }

    HYP_FORCE_INLINE bool operator<(const RefCountedPtr<void, CountType> &other) const
        { return Base::m_ptr < other.m_ptr; }

    HYP_FORCE_INLINE bool operator<(const WeakRefCountedPtr &other) const
        { return Base::m_ptr < other.m_ptr; }

    /*! \brief Gets a pointer to the value held by the reference counted pointer.
     *  \note Use sparringly. This method does not lock the reference -- the object may be deleted from another thread while using it */
    HYP_FORCE_INLINE void *GetUnsafe() const
        { return Base::m_ptr; }
};

template <class CountType>
class EnableRefCountedPtrFromThisBase
{
    friend struct detail::RefCountData<CountType>;

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

    // Needs to be virtual to allow static_cast to this class from void *
    virtual ~EnableRefCountedPtrFromThisBase() = default;

public:
    WeakRefCountedPtr<void, CountType>  weak_this;
};

/*! \brief A wrapper for T that allows an object to be created without dynamic allocation, but still be used as a Weak<T> in other places
 *  without causing destruction once the last locked RC<T> is destroyed. This comes with a caveat, however -- all references to object will be invalidated
 *  upon destruction of this object, meaning all RC<T> and Weak<T> will be invalid.
 * 
 *  T must be a subclass of EnableRefCountedPtrFromThis<T> to use this class.
 * 
 *  \note Reassigning the OwningRefCountedPtr will reassign the underlying T object, meaning all RC<T> will continue pointing to the current data - be careful. */
template <class T, class CountType = AtomicVar<uint32>, typename = std::enable_if_t<std::is_base_of_v<EnableRefCountedPtrFromThisBase<CountType>, T>>>
class OwningRefCountedPtr
{
    HYP_FORCE_INLINE detail::RefCountData<CountType> *GetRefCountData_Internal() const
        { return static_cast<EnableRefCountedPtrFromThisBase<CountType> *>(m_value.GetPointer())->weak_this.GetRefCountData_Internal(); }

public:
    OwningRefCountedPtr()
        : m_is_initialized(false)
    {
        if constexpr (std::is_default_constructible_v<T>) {
            if constexpr (IsHypObject<T>::value) {
                Memory::ConstructWithContext<T, HypObjectInitializerGuard<T>>(m_value.GetPointer());
            } else {
                Memory::Construct<T>(m_value.GetPointer());
            }

            auto *ref_count_data = GetRefCountData_Internal();
            ref_count_data->IncRefCount_Strong(m_value.GetPointer());
            
            m_is_initialized = true;
        }
    }

    template <class Arg0, class... Args>
    OwningRefCountedPtr(Arg0 &&arg0, Args &&... args)
        : m_is_initialized(true)
    {
        if constexpr (IsHypObject<T>::value) {
            Memory::ConstructWithContext<T, HypObjectInitializerGuard<T>>(m_value.GetPointer(), std::forward<Arg0>(arg0), std::forward<Args>(args)...);
        } else {
            Memory::Construct<T>(m_value.GetPointer(), std::forward<Arg0>(arg0), std::forward<Args>(args)...);
        }

        auto *ref_count_data = GetRefCountData_Internal();
        ref_count_data->IncRefCount_Strong(m_value.GetPointer());
    }

    OwningRefCountedPtr(const OwningRefCountedPtr &other)
        : m_is_initialized(other.m_is_initialized)
    {
        if (m_is_initialized) {
            if constexpr (IsHypObject<T>::value) {
                Memory::ConstructWithContext<T, HypObjectInitializerGuard<T>>(m_value.GetPointer(), other.m_value.Get());
            } else {
                Memory::Construct<T>(m_value.GetPointer(), other.m_value.Get());
            }

            auto *ref_count_data = GetRefCountData_Internal();
            ref_count_data->IncRefCount_Strong(m_value.GetPointer());
        }
    }

#if 0
    // Deleted to prevent accidental assignment while other RefCountedPtr and WeakRefCountedPtr are using this.
    OwningRefCountedPtr &operator=(const OwningRefCountedPtr &other) = delete;
#else
    OwningRefCountedPtr &operator=(const OwningRefCountedPtr &other)
    {
        if (this == &other) {
            return *this;
        }

        if constexpr (std::is_copy_assignable_v<T>) {
            if (m_is_initialized) {
                m_value.Get() = other.m_value.Get();

                return *this;
            }
        }

        if (m_is_initialized) {
            auto *ref_count_data = GetRefCountData_Internal();
            ref_count_data->DecRefCount_Strong(m_value.GetPointer(), false);
            
            Memory::Destruct<T>(m_value.GetPointer());
        }

        if (other.m_is_initialized) {
            if constexpr (IsHypObject<T>::value) {
                Memory::ConstructWithContext<T, HypObjectInitializerGuard<T>>(m_value.GetPointer(), other.m_value.Get());
            } else {
                Memory::Construct<T>(m_value.GetPointer(), other.m_value.Get());
            }

            auto *ref_count_data = GetRefCountData_Internal();
            ref_count_data->IncRefCount_Strong(m_value.GetPointer());
        }

        m_is_initialized = other.m_is_initialized;

        return *this;
    }
#endif

    OwningRefCountedPtr(OwningRefCountedPtr &&other) noexcept
        : m_is_initialized(other.m_is_initialized)
    {
        if (other.m_is_initialized) {
            if constexpr (IsHypObject<T>::value) {
                Memory::ConstructWithContext<T, HypObjectInitializerGuard<T>>(m_value.GetPointer(), std::move(other.m_value.Get()));
            } else {
                Memory::Construct<T>(m_value.GetPointer(), std::move(other.m_value.Get()));
            }

            auto *ref_count_data = GetRefCountData_Internal();
            ref_count_data->IncRefCount_Strong(m_value.GetPointer());
        }

        other.m_is_initialized = false;
    }

#if 0
    // Deleted to prevent accidental assignment while other RefCountedPtr and WeakRefCountedPtr are using this.
    OwningRefCountedPtr &operator=(OwningRefCountedPtr &&other) noexcept = delete;
#else
    OwningRefCountedPtr &operator=(OwningRefCountedPtr &&other) noexcept
    {
        if (this == &other) {
            return *this;
        }

        if constexpr (std::is_move_assignable_v<T> || std::is_copy_assignable_v<T>) {
            if (m_is_initialized) {
                m_value.Get() = std::move(other.m_value.Get());
                return *this;
            }
        }

        if (m_is_initialized) {
            auto *ref_count_data = GetRefCountData_Internal();
            ref_count_data->DecRefCount_Strong(m_value.GetPointer());
            
            Memory::Destruct<T>(m_value.GetPointer());
        }

        if (other.m_is_initialized) {
            if constexpr (IsHypObject<T>::value) {
                Memory::ConstructWithContext<T, HypObjectInitializerGuard<T>>(m_value.GetPointer(), std::move(other.m_value.Get()));
            } else {
                Memory::Construct<T>(m_value.GetPointer(), std::move(other.m_value.Get()));
            }

            auto *ref_count_data = GetRefCountData_Internal();
            ref_count_data->DecRefCount_Strong(m_value.GetPointer());
        }

        m_is_initialized = other.m_is_initialized;
        other.m_is_initialized = false;

        return *this;
    }
#endif

    ~OwningRefCountedPtr()
    {
        if (m_is_initialized) {
            auto *ref_count_data = GetRefCountData_Internal();
            ref_count_data->DecRefCount_Strong(m_value.GetPointer());
            
            Memory::Destruct<T>(m_value.GetPointer());
        }
    }
    
    HYP_FORCE_INLINE constexpr TypeID GetTypeID() const
        { return TypeID::ForType<T>(); }

    HYP_FORCE_INLINE bool Empty() const
        { return !m_is_initialized; }

    HYP_FORCE_INLINE bool Any() const
        { return m_is_initialized; }

    HYP_FORCE_INLINE operator bool() const
        { return m_is_initialized; }

    HYP_FORCE_INLINE bool operator!() const
        { return !m_is_initialized; }

    HYP_FORCE_INLINE T *Get() const
        { return m_is_initialized ? &m_value.Get() : nullptr; }

    HYP_FORCE_INLINE T *operator->() const
        { return m_is_initialized ? static_cast<T *>(m_value.GetPointer()) : nullptr; }

    HYP_FORCE_INLINE T &operator*() const
    {
        if (!m_is_initialized) {
            HYP_FAIL("Dereferenced uninitialized pointer");
        }

        return m_value.Get();
    }

    HYP_FORCE_INLINE operator T *() const
        { return m_is_initialized ? &m_value.Get() : nullptr; }

    HYP_FORCE_INLINE operator const T *() const
        { return m_is_initialized ? &m_value.Get() : nullptr; }

    HYP_FORCE_INLINE bool operator==(const OwningRefCountedPtr &other) const
        { return Get() == other.Get(); }

    HYP_FORCE_INLINE bool operator!=(const OwningRefCountedPtr &other) const
        { return Get() != other.Get(); }

    HYP_FORCE_INLINE bool operator==(std::nullptr_t) const
        { return !m_is_initialized; }

    HYP_FORCE_INLINE bool operator!=(std::nullptr_t) const
        { return m_is_initialized; }

    HYP_FORCE_INLINE bool operator==(const T *ptr) const
        { return Get() == ptr; }

    HYP_FORCE_INLINE bool operator!=(const T *ptr) const
        { return Get() != ptr; }

    HYP_FORCE_INLINE bool operator==(T *ptr) const
        { return Get() == ptr; }

    HYP_FORCE_INLINE bool operator!=(T *ptr) const
        { return Get() != ptr; }

    HYP_FORCE_INLINE bool operator<(const OwningRefCountedPtr &other) const
        { return Get() < other.Get(); }

    HYP_FORCE_INLINE bool operator<(const T *ptr) const
        { return Get() < ptr; }

    HYP_FORCE_INLINE bool operator<(T *ptr) const
        { return Get() < ptr; }

    HYP_NODISCARD HYP_FORCE_INLINE WeakRefCountedPtr<T, CountType> ToWeak() const
        { return m_is_initialized ? static_cast<EnableRefCountedPtrFromThisBase<CountType> *>(m_value.GetPointer())->weak_this : WeakRefCountedPtr<T, CountType>(); }

    void Reset()
    {
        if (m_is_initialized) {
            auto *ref_count_data = GetRefCountData_Internal();
            ref_count_data->DecRefCount_Strong(m_value.GetPointer());
            
            Memory::Destruct<T>(m_value.GetPointer());

            m_is_initialized = false;
        }
    }

private:
    // Keep it mutable to allow the same interface as RefCountedPtr
    mutable ValueStorage<T> m_value;
    bool                    m_is_initialized;
};

/*! \brief Helper struct to convert raw pointers to RefCountedPtrs.
 *  For internal use only. */

template <class T, class CountType = AtomicVar<uint32>>
HYP_FORCE_INLINE RefCountedPtr<T, CountType> ToRefCountedPtr(T *ptr)
{
    static_assert(std::is_base_of_v<EnableRefCountedPtrFromThisBase<CountType>, T>, "T must derive EnableRefCountedPtrFromThisBase<CountType>");

    if (!ptr) {
        return RefCountedPtr<T, CountType>();
    }

    if constexpr (std::is_base_of_v<EnableRefCountedPtrFromThis<T, CountType>, T>) {
        return ptr->RefCountedPtrFromThis();
    } else {
        return ptr->RefCountedPtrFromThis().template CastUnsafe<T>();
    }
}

template <class T, class CountType = AtomicVar<uint32>>
HYP_FORCE_INLINE RefCountedPtr<const T, CountType> ToRefCountedPtr(const T *ptr)
{
    static_assert(std::is_base_of_v<EnableRefCountedPtrFromThisBase<CountType>, T>, "T must derive EnableRefCountedPtrFromThisBase<CountType>");

    if (!ptr) {
        return RefCountedPtr<const T, CountType>();
    }

    if constexpr (std::is_base_of_v<EnableRefCountedPtrFromThis<T, CountType>, T>) {
        return ptr->RefCountedPtrFromThis();
    } else {
        return ptr->RefCountedPtrFromThis().template CastUnsafe<T>();
    }
}

/*! \brief Helper struct to convert raw pointers to WeakRefCountedPtrs.
 *  For internal use only. */
template <class T, class CountType = AtomicVar<uint32>>
HYP_FORCE_INLINE WeakRefCountedPtr<T, CountType> ToWeakRefCountedPtr(T *ptr)
{
    static_assert(std::is_base_of_v<EnableRefCountedPtrFromThisBase<CountType>, T>, "T must derive EnableRefCountedPtrFromThisBase<CountType>");

    if (!ptr) {
        return WeakRefCountedPtr<T, CountType>();
    }

    if constexpr (std::is_base_of_v<EnableRefCountedPtrFromThis<T, CountType>, T>) {
        return ptr->WeakRefCountedPtrFromThis();
    } else {
        return ptr->WeakRefCountedPtrFromThis().template CastUnsafe<T>();
    }
}

template <class T, class CountType = AtomicVar<uint32>>
HYP_FORCE_INLINE WeakRefCountedPtr<const T, CountType> ToWeakRefCountedPtr(const T *ptr)
{
    static_assert(std::is_base_of_v<EnableRefCountedPtrFromThisBase<CountType>, T>, "T must derive EnableRefCountedPtrFromThisBase<CountType>");

    if (!ptr) {
        return WeakRefCountedPtr<const T, CountType>();
    }
    
    if constexpr (std::is_base_of_v<EnableRefCountedPtrFromThis<T, CountType>, T>) {
        return ptr->WeakRefCountedPtrFromThis();
    } else {
        return ptr->WeakRefCountedPtrFromThis().template CastUnsafe<T>();
    }
}

template <class T, class CountType>
class EnableRefCountedPtrFromThis : public EnableRefCountedPtrFromThisBase<CountType>
{
    using Base = EnableRefCountedPtrFromThisBase<CountType>;

public:
    EnableRefCountedPtrFromThis()
    {
        detail::RefCountData<CountType> *ref_count_data = new detail::RefCountData<CountType>;
        ref_count_data->template InitWeak<T>(static_cast<T *>(this));

        EnableRefCountedPtrFromThisBase<CountType>::weak_this.SetRefCountData_Internal(static_cast<T *>(this), ref_count_data, true);
    }

    virtual ~EnableRefCountedPtrFromThis() override = default;

    RefCountedPtr<T, CountType> RefCountedPtrFromThis() const
        { return Base::weak_this.template CastUnsafe_Internal<T>().Lock(); }

    WeakRefCountedPtr<T, CountType> WeakRefCountedPtrFromThis() const
        { return Base::weak_this.template CastUnsafe_Internal<T>(); }

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

template <class T, class CountType = AtomicVar<uint32>>
struct MakeRefCountedPtrHelper
{
    template <class... Args>
    static RefCountedPtr<T, CountType> MakeRefCountedPtr(Args &&... args)
    {
        return RefCountedPtr<T, CountType>::Construct(std::forward<Args>(args)...);
    }
};

} // namespace memory

template <class T, class CountType = AtomicVar<uint32>>
using Weak = memory::WeakRefCountedPtr<T, CountType>; 

template <class T, class CountType = AtomicVar<uint32>>
using RC = memory::RefCountedPtr<T, CountType>;

template <class T, class CountType = AtomicVar<uint32>>
using OwningRC = memory::OwningRefCountedPtr<T, CountType>;

template <class T, class... Args>
HYP_FORCE_INLINE RC<T> MakeRefCountedPtr(Args &&... args)
{
    return memory::MakeRefCountedPtrHelper<T, AtomicVar<uint32>>::template MakeRefCountedPtr(std::forward<Args>(args)...);
}

template <class CountType = AtomicVar<uint32>>
using EnableRefCountedPtrFromThisBase = memory::EnableRefCountedPtrFromThisBase<CountType>;

template <class T, class CountType = AtomicVar<uint32>>
using EnableRefCountedPtrFromThis = memory::EnableRefCountedPtrFromThis<T, CountType>;

using memory::ToRefCountedPtr;
using memory::ToWeakRefCountedPtr;

} // namespace hyperion

#endif