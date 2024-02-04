#ifndef HYPERION_V2_LIB_REF_COUNTED_PTR_HPP
#define HYPERION_V2_LIB_REF_COUNTED_PTR_HPP

#include <util/Defines.hpp>
#include <core/lib/ValueStorage.hpp>
#include <core/lib/TypeID.hpp>
#include <core/lib/CMemory.hpp>
#include <core/lib/Any.hpp>
#include <Types.hpp>
#include <Constants.hpp>
#include <system/Debug.hpp>

#include <atomic>
#include <cstdlib>

namespace hyperion {
namespace detail {

template <class CountType = UInt>
struct RefCountData
{
    void        *value;
    TypeID      type_id;
    CountType   strong_count;
    CountType   weak_count;
    void        (*dtor)(void *);

#ifdef HYP_DEBUG_MODE
    ~RefCountData()
    {
        AssertThrow(dtor == nullptr);
        AssertThrow(value == nullptr);
        AssertThrow(strong_count == 0);
        AssertThrow(weak_count == 0);
        AssertThrow(type_id == TypeID::ForType<void>());
    }
#endif

    auto UseCount() const
    {
        if constexpr (std::is_integral_v<CountType>) {
            return strong_count;
        } else {
            return strong_count.load(std::memory_order_acquire);
        }
    }

    template <class T, class ...Args>
    void Construct(Args &&... args)
    {
        value = Memory::New<T>(std::forward<Args>(args)...);
        dtor = &Memory::Delete<T>;
        type_id = TypeID::ForType<T>();
    }

    template <class T>
    void TakeOwnership(T *ptr)
    {
        value = ptr;
        dtor = &Memory::Delete<T>;
        type_id = TypeID::ForType<T>();
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

template <class T, class CountType>
class RefCountedPtr;

template <class CountType>
class WeakRefCountedPtrBase;

template <class T, class CountType>
class WeakRefCountedPtr;

template <class CountType = UInt>
class RefCountedPtrBase
{
    friend class WeakRefCountedPtrBase<CountType>;

public:
    using RefCountDataType = RefCountData<CountType>;

    RefCountedPtrBase()
        : m_ref(nullptr)
    {
    }

protected:
    RefCountedPtrBase(const RefCountedPtrBase &other)
        : m_ref(other.m_ref)
    {
        if (m_ref) {
            IncRefCount();
        }
    }

    RefCountedPtrBase &operator=(const RefCountedPtrBase &other)
    {
        if (std::addressof(other) == this) {
            return *this;
        }

        DropRefCount();

        m_ref = other.m_ref;

        if (m_ref) {
            IncRefCount();
        }

        return *this;
    }

    RefCountedPtrBase(RefCountedPtrBase &&other) noexcept
        : m_ref(other.m_ref)
    {
        other.m_ref = nullptr;
    }

    RefCountedPtrBase &operator=(RefCountedPtrBase &&other) noexcept
    {
        if (std::addressof(other) == this) {
            return *this;
        }

        DropRefCount();

        m_ref = other.m_ref;
        other.m_ref = nullptr;

        return *this;
    }

public:

    ~RefCountedPtrBase()
    {
        DropRefCount();
    }

    HYP_FORCE_INLINE
    void *Get() const
        { return m_ref ? m_ref->value : nullptr; }
    
    HYP_FORCE_INLINE
    explicit operator bool() const
        { return Get() != nullptr; }
    
    HYP_FORCE_INLINE
    bool operator!() const
        { return Get() == nullptr; }
    
    HYP_FORCE_INLINE
    bool operator==(const RefCountedPtrBase &other) const
        { return Get() == other.Get(); }
    
    HYP_FORCE_INLINE
    bool operator==(std::nullptr_t) const
        { return Get() == nullptr; }
    
    HYP_FORCE_INLINE
    bool operator!=(const RefCountedPtrBase &other) const
        { return Get() != other.Get(); }
    
    HYP_FORCE_INLINE
    bool operator!=(std::nullptr_t) const
        { return Get() != nullptr; }
    
    HYP_FORCE_INLINE
    TypeID GetTypeID() const
        { return m_ref ? m_ref->type_id : TypeID::ForType<void>(); }

    /*! \brief Creates a new RefCountedPtr of T from the existing RefCountedPtr.
     * If the types are not exact, or T is not equal to void (in the case of a void pointer),
     * no conversion is performed and an empty RefCountedPtr is returned.
     */
    template <class T>
    HYP_FORCE_INLINE
    RefCountedPtr<T, CountType> Cast()
    {
        if (GetTypeID() == TypeID::ForType<T>() || std::is_same_v<T, void>) {
            return CastUnsafe<T>();
        }

        return RefCountedPtr<T, CountType>();
    }

    /*! \brief Drops the reference to the currently held value, if any.  */
    HYP_FORCE_INLINE
    void Reset()
        { DropRefCount(); }

    /*! \brief Used by objects inheriting from this class or marshaling data. Not ideal to use externally */
    HYP_FORCE_INLINE
    RefCountDataType *GetRefCountData() const
        { return m_ref; }

    /*! \brief Sets the internal reference to the given RefCountDataType. Only for internal use. */
    HYP_FORCE_INLINE
    void SetRefCountData(RefCountDataType *ref, bool inc_ref = true)
    {
        DropRefCount();
        m_ref = ref;

        if (inc_ref && m_ref != nullptr) {
            IncRefCount();
        }
    }

    template <class T>
    HYP_FORCE_INLINE
    RefCountedPtr<T, CountType> CastUnsafe()
    {
        RefCountedPtr<T, CountType> rc;
        rc.m_ref = m_ref;

        if (m_ref) {
            IncRefCount();
        }

        return rc;
    }

    /*! \brief Releases the reference to the currently held value, if any, and returns it.
        * The caller is responsible for handling the reference count of the returned value.
    */
    RefCountDataType *Release()
    {
        RefCountDataType *ref = m_ref;
        m_ref = nullptr;

        return ref;
    }

protected:
    explicit RefCountedPtrBase(RefCountDataType *ref)
        : m_ref(ref)
    {
    }
    
    HYP_FORCE_INLINE
    void IncRefCount()
    {
#ifdef HYP_DEBUG_MODE
        AssertThrow(m_ref != nullptr);
#endif

        if constexpr (std::is_integral_v<CountType>) {
            ++m_ref->strong_count;
        } else {
            m_ref->strong_count.fetch_add(1u, std::memory_order_relaxed);
        }
    }
    
    HYP_FORCE_INLINE
    void DropRefCount()
    {
        if (m_ref) {
            if constexpr (std::is_integral_v<CountType>) {
                if (--m_ref->strong_count == 0u) {
                    m_ref->Destruct();

                    if (m_ref->weak_count == 0u) {
                        delete m_ref;
                    }
                }
            } else {
                if (m_ref->strong_count.fetch_sub(1) == 1) {
                    m_ref->Destruct();

                    if (m_ref->weak_count.load() == 0u) {
                        delete m_ref;
                    }
                }
            }

            m_ref = nullptr;
        }
    }

    RefCountDataType *m_ref;
};

/*! \brief A simple ref counted pointer class.
    Not atomic by default, but using AtomicRefCountedPtr allows it to be. */
template <class T, class CountType = UInt>
class RefCountedPtr : public RefCountedPtrBase<CountType>
{
    friend class WeakRefCountedPtr<std::remove_const_t<T>, CountType>;
    friend class WeakRefCountedPtr<std::add_const_t<T>, CountType>;

protected:
    using Base = RefCountedPtrBase<CountType>;

public:
    template <class ...Args>
    [[nodiscard]] static RefCountedPtr Construct(Args &&... args)
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
    explicit RefCountedPtr(T *ptr)
        : Base()
    {
        Reset(ptr);
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

    HYP_FORCE_INLINE
    T *Get() const
        { return Base::m_ref ? static_cast<T *>(Base::m_ref->value) : nullptr; }

    HYP_FORCE_INLINE
    T *operator->() const
        { return Get(); }

    HYP_FORCE_INLINE
    T &operator*()
        { return *Get(); }

    HYP_FORCE_INLINE
    const T &operator*() const
        { return *Get(); }

    HYP_FORCE_INLINE
    operator bool() const
        { return Base::operator bool(); }

    HYP_FORCE_INLINE
    bool operator!() const
        { return Base::operator!(); }

    HYP_FORCE_INLINE
    bool operator==(const RefCountedPtr &other) const
        { return Base::operator==(other); }

    HYP_FORCE_INLINE
    bool operator==(std::nullptr_t) const
        { return Base::operator==(nullptr); }

    HYP_FORCE_INLINE
    bool operator!=(const RefCountedPtr &other) const
        { return Base::operator!=(other); }

    HYP_FORCE_INLINE
    bool operator!=(std::nullptr_t) const
        { return Base::operator!=(nullptr); }

    void Set(const T &value)
    {
        Base::DropRefCount();

        Base::m_ref = new typename Base::RefCountDataType;
        Base::m_ref->template Construct<T>(value);
        Base::m_ref->strong_count = 1u;
        Base::m_ref->weak_count = 0u;
    }

    void Set(T &&value)
    {
        Base::DropRefCount();

        Base::m_ref = new typename Base::RefCountDataType;
        Base::m_ref->template Construct<T>(std::move(value));
        Base::m_ref->strong_count = 1u;
        Base::m_ref->weak_count = 0u;
    }

    /*! \brief Takes ownership of {ptr}, dropping the reference to the currently held value,
        if any. Note, do not delete the ptr after passing it to Reset(), as it will be deleted
        automatically. */
    void Reset(T *ptr)
    {
        Base::DropRefCount();

        if (ptr) {
            Base::m_ref = new typename Base::RefCountDataType;
            Base::m_ref->template TakeOwnership<T>(ptr);
            Base::m_ref->strong_count = 1u;
            Base::m_ref->weak_count = 0u;
        }
    }

    /*! \brief Drops the reference to the currently held value, if any.  */
    void Reset()
        { Base::Reset(); }
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

    HYP_FORCE_INLINE
    void *Get() const
        { return Base::Get(); }
    
    HYP_FORCE_INLINE
    operator bool() const
        { return Base::operator bool(); }

    HYP_FORCE_INLINE
    bool operator!() const
        { return Base::operator!(); }

    HYP_FORCE_INLINE
    bool operator==(const RefCountedPtr &other) const
        { return Base::operator==(other); }

    HYP_FORCE_INLINE
    bool operator==(std::nullptr_t) const
        { return Base::operator==(nullptr); }

    HYP_FORCE_INLINE
    bool operator!=(const RefCountedPtr &other) const
        { return Base::operator!=(other); }

    HYP_FORCE_INLINE
    bool operator!=(std::nullptr_t) const
        { return Base::operator!=(nullptr); }

    template <class T>
    void Set(const T &value)
    {
        Base::DropRefCount();

        Base::m_ref = new typename Base::RefCountDataType;
        Base::m_ref->template Construct<T>(value);
        Base::m_ref->strong_count = 1u;
        Base::m_ref->weak_count = 0u;
    }

    template <class T>
    void Set(T &&value)
    {
        Base::DropRefCount();

        Base::m_ref = new typename Base::RefCountDataType;
        Base::m_ref->template Construct<T>(std::move(value));
        Base::m_ref->strong_count = 1u;
        Base::m_ref->weak_count = 0u;
    }

    /*! \brief Drops the reference to the currently held value, if any.  */
    void Reset()
        { Base::Reset(); }

    /*! \brief Takes ownership of {ptr}, dropping the reference to the currently held value,
        if any. Note, do not delete the ptr after passing it to Reset(), as it will be deleted
        automatically. */
    template <class T>
    void Reset(T *ptr)
    {
        Base::DropRefCount();

        if (ptr) {
            Base::m_ref = new typename Base::RefCountDataType;
            Base::m_ref->template TakeOwnership<T>(ptr);
            Base::m_ref->strong_count = 1u;
            Base::m_ref->weak_count = 0u;
        }
    }
};

// weak ref counters

template <class CountType = UInt>
class WeakRefCountedPtrBase
{
protected:
    using RefCountDataType = RefCountData<CountType>;

public:
    WeakRefCountedPtrBase()
        : m_ref(nullptr)
    {
    }

    WeakRefCountedPtrBase(const RefCountedPtrBase<CountType> &other)
        : m_ref(other.m_ref)
    {
        if (m_ref) {
            ++m_ref->weak_count;
        }
    }

    WeakRefCountedPtrBase(const WeakRefCountedPtrBase &other)
        : m_ref(other.m_ref)
    {
        if (m_ref) {
            ++m_ref->weak_count;
        }
    }

    WeakRefCountedPtrBase &operator=(const RefCountedPtrBase<CountType> &other)
    {
        DropRefCount();

        m_ref = other.m_ref;

        if (m_ref) {
            ++m_ref->weak_count;
        }

        return *this;
    }

    WeakRefCountedPtrBase &operator=(const WeakRefCountedPtrBase &other)
    {
        DropRefCount();

        m_ref = other.m_ref;

        if (m_ref) {
            ++m_ref->weak_count;
        }

        return *this;
    }

    WeakRefCountedPtrBase(WeakRefCountedPtrBase &&other) noexcept
        : m_ref(other.m_ref)
    {
        other.m_ref = nullptr;
    }

    WeakRefCountedPtrBase &operator=(WeakRefCountedPtrBase &&other) noexcept
    {
        DropRefCount();

        m_ref = other.m_ref;
        other.m_ref = nullptr;

        return *this;
    }

    ~WeakRefCountedPtrBase()
    {
        DropRefCount();
    }

    HYP_FORCE_INLINE
    void *Get() const
        { return m_ref ? m_ref->value : nullptr; }
    
    HYP_FORCE_INLINE
    explicit operator bool() const
        { return Get() != nullptr; }
    
    HYP_FORCE_INLINE
    bool operator!() const
        { return Get() == nullptr; }
    
    HYP_FORCE_INLINE
    bool operator==(const RefCountedPtrBase<CountType> &other) const
        { return m_ref == other.m_ref; }
    
    HYP_FORCE_INLINE
    bool operator==(const WeakRefCountedPtrBase &other) const
        { return m_ref == other.m_ref; }
    
    HYP_FORCE_INLINE
    bool operator==(std::nullptr_t) const
        { return Get() == nullptr; }
    
    HYP_FORCE_INLINE
    bool operator!=(const RefCountedPtrBase<CountType> &other) const
        { return m_ref != other.m_ref; }
    
    HYP_FORCE_INLINE
    bool operator!=(const WeakRefCountedPtrBase &other) const
        { return m_ref != other.m_ref; }
    
    HYP_FORCE_INLINE
    bool operator!=(std::nullptr_t) const
        { return Get() != nullptr; }
    
    HYP_FORCE_INLINE
    TypeID GetTypeID() const
        { return m_ref ? m_ref->type_id : TypeID::ForType<void>(); }

    /*! \brief Drops the reference to the currently held value, if any.  */
    HYP_FORCE_INLINE
    void Reset()
        { DropRefCount(); }

    /*! \brief Used by objects inheriting from this class or marshaling data. Not ideal to use externally */
    HYP_FORCE_INLINE
    RefCountDataType *GetRefCountData() const
        { return m_ref; }


protected:
    explicit WeakRefCountedPtrBase(RefCountDataType *ref)
        : m_ref(ref)
    {
    }
    
    HYP_FORCE_INLINE
    void DropRefCount()
    {
        if (m_ref) {
            if constexpr (std::is_integral_v<CountType>) {
                if (--m_ref->weak_count == 0u) {
                    if (m_ref->strong_count == 0u) {
                        delete m_ref;
                    }
                }
            } else {
                if (m_ref->weak_count.fetch_sub(1) == 1) {
                    if (m_ref->strong_count.load() == 0) {
                        delete m_ref;
                    }
                }
            }

            m_ref = nullptr;
        }
    }

    RefCountDataType *m_ref;
};

template <class T, class CountType = UInt>
class WeakRefCountedPtr : public WeakRefCountedPtrBase<CountType>
{
protected:
    using Base = WeakRefCountedPtrBase<CountType>;

public:
    using Base::operator==;
    using Base::operator!=;
    using Base::operator!;
    using Base::operator bool;

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

    HYP_FORCE_INLINE
    T *Get() const
        { return Base::m_ref ? static_cast<T *>(Base::m_ref->value) : nullptr; }
    
    HYP_FORCE_INLINE
    RefCountedPtr<T, CountType> Lock() const
    {
        RefCountedPtr<T, CountType> rc;
        rc.m_ref = Base::m_ref;

        if (Base::m_ref) {
            ++Base::m_ref->strong_count;
        }

        return rc;
    }
};

template <class T, class CountType = UInt>
class RefCountedRef
{
    struct Data
    {
        ValueStorage<T> storage;
        CountType       count { 0 };

        template <class ...Args>
        void Construct(Args &&... args)
        {
            storage.Construct(std::forward<Args>(args)...);
        }

        void Destruct()
        {
            storage.Destruct();
        }

        void IncRef()
        {
            if constexpr (std::is_integral_v<CountType>) {
                ++count;
            } else {
                count.fetch_add(1u, std::memory_order_relaxed);
            }
        }

        bool DecRef()
        {
            SizeType remaining = 0;

            if constexpr (std::is_integral_v<CountType>) {
                remaining = count--;
            } else {
                remaining = count.fetch_sub(1u);
            }

            if (remaining == 1) {
                Destruct();

                return true;
            }

            return false;
        }
    };

    Data *m_data;

public:

    template <class ...Args>
    RefCountedRef(Args &&... args)
        : m_data(new Data)
    {
        m_data->IncRef();
        m_data->Construct(std::forward<Args>(args)...);
    }

    RefCountedRef(const RefCountedRef &other)
        : m_data(other.m_data)
    {
        if (m_data) {
            m_data->IncRef();
        }
    }

    RefCountedRef &operator=(const RefCountedRef &other) noexcept
    {
        if (m_data) {
            if (m_data->DecRef()) {
                delete m_data;
            }

            m_data = nullptr;
        }

        m_data = other.m_data;

        if (m_data) {
            m_data->IncRef();
        }

        return *this;
    }


    RefCountedRef(RefCountedRef &&other) noexcept
        : m_data(other.m_data)
    {
        other.m_data = nullptr;
    }

    RefCountedRef &operator=(RefCountedRef &&other) noexcept
    {
        if (m_data) {
            if (m_data->DecRef()) {
                delete m_data;
            }

            m_data = nullptr;
        }

        std::swap(m_data, other.m_data);

        return *this;
    }

    ~RefCountedRef()
    {
        if (m_data) {
            if (m_data->DecRef()) {
                delete m_data;
            }
        }
    }

    T &Get()
    {
        AssertThrow(m_data != nullptr);
        
        return m_data->storage.Get();
    }

    const T &Get() const
    {
        AssertThrow(m_data != nullptr);
        
        return m_data->storage.Get();
    }

    bool IsValid() const
        { return m_data != nullptr; }

    bool operator==(const RefCountedRef &other) const
        { return m_data == other.m_data; }

    bool operator!=(const RefCountedRef &other) const
        { return m_data != other.m_data; }

    bool operator==(std::nullptr_t) const
        { return !IsValid(); }

    bool operator!=(std::nullptr_t) const
        { return IsValid(); }
};

} // namespace detail

template <class T>
using AtomicRefCountedPtr = detail::RefCountedPtr<T, std::atomic<UInt>>;

template <class T>
using RefCountedPtr = detail::RefCountedPtr<T, UInt>;


template <class T, class CountType = std::atomic<UInt>>
using Weak = detail::WeakRefCountedPtr<T, CountType>; 

template <class T, class CountType = std::atomic<UInt>>
using RC = detail::RefCountedPtr<T, CountType>;

template <class T, class CountType = std::atomic<UInt>>
using Ref = detail::RefCountedRef<T, CountType>;

// template <class T, class CountType = std::atomic<UInt>>
// class EnableRefCountedPtrFromThis
// {
// public:
//     EnableRefCountedPtrFromThis() = default;

//     EnableRefCountedPtrFromThis(const EnableRefCountedPtrFromThis &)
//     {
//         // Do not modify weak ptr
//     }

//     EnableRefCountedPtrFromThis &operator=(const EnableRefCountedPtrFromThis &)
//     {
//         // Do not modify weak ptr

//         return *this;
//     }

//     ~EnableRefCountedPtrFromThis() = default;

// protected:
//     RC<T, CountType> RefCountedPtrFromThis()
//         { return weak.Lock(); }

//     RC<const T, CountType> RefCountedPtrFromThis() const
//         { return weak.Lock(); }

//     Weak<T, CountType> WeakFromThis()
//         { return weak; }

// private:
//     Weak<T, CountType> weak;
// };

} // namespace hyperion

#endif