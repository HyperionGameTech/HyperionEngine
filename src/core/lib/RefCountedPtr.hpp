#ifndef HYPERION_V2_LIB_REF_COUNTED_PTR_HPP
#define HYPERION_V2_LIB_REF_COUNTED_PTR_HPP

#include <util/Defines.hpp>
#include <core/lib/TypeID.hpp>
#include <core/lib/CMemory.hpp>
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
    void *value;
    TypeID type_id;
    CountType strong_count;
    CountType weak_count;
    void (*dtor)(void *);

#if HYP_DEBUG_MODE
    ~RefCountData()
    {
        AssertThrow(dtor == nullptr);
        AssertThrow(value == nullptr);
        AssertThrow(strong_count == 0);
        AssertThrow(weak_count == 0);
        AssertThrow(type_id == TypeID::ForType<void>());
    }
#endif

    template <class T, class ...Args>
    void Construct(Args &&... args)
    {
        using Normalized = NormalizedType<T>;

        value = Memory::AllocateAndConstruct<Normalized>(std::forward<Args>(args)...);
        dtor = &Memory::DestructAndFree<Normalized>;
        type_id = TypeID::ForType<Normalized>();
    }

    template <class T>
    void TakeOwnership(T *ptr)
    {
        using Normalized = NormalizedType<T>;

        value = ptr;
        dtor = &Memory::DestructAndFree<Normalized>;
        type_id = TypeID::ForType<T>();
    }

    void Destruct()
    {
#if HYP_DEBUG_MODE
        AssertThrow(value != nullptr);
        AssertThrow(strong_count == 0);
#endif

        dtor(value);
        type_id = TypeID::ForType<void>();
        value = nullptr;
        dtor = nullptr;
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

protected:
    using RefCountDataType = RefCountData<CountType>;

public:
    RefCountedPtrBase()
        : m_ref(nullptr)
    {
    }

protected:
    RefCountedPtrBase(const RefCountedPtrBase &other)
        : m_ref(other.m_ref)
    {
        if (m_ref) {
            ++m_ref->strong_count;
        }
    }

    RefCountedPtrBase &operator=(const RefCountedPtrBase &other)
    {
        DropRefCount();

        m_ref = other.m_ref;

        if (m_ref) {
            ++m_ref->strong_count;
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

    HYP_FORCE_INLINE void *Get() const
        { return m_ref ? m_ref->value : nullptr; }

    explicit operator bool() const
        { return Get() != nullptr; }

    bool operator!() const
        { return Get() == nullptr; }

    bool operator==(const RefCountedPtrBase &other) const
        { return m_ref == other.m_ref; }

    bool operator==(std::nullptr_t) const
        { return Get() == nullptr; }

    bool operator!=(const RefCountedPtrBase &other) const
        { return m_ref != other.m_ref; }

    bool operator!=(std::nullptr_t) const
        { return Get() != nullptr; }

    const TypeID &GetTypeID() const
        { return m_ref ? m_ref->type_id : TypeID::ForType<void>(); }

    /*! \brief Creates a new RefCountedPtr of T from the existing RefCountedPtr.
     * If the types are not exact, or T is not equal to void (in the case of a void pointer),
     * no conversion is performed and an empty RefCountedPtr is returned.
     */
    template <class T>
    HYP_FORCE_INLINE RefCountedPtr<T, CountType> Cast()
    {
        if (GetTypeID() == TypeID::ForType<T>() || std::is_same_v<T, void>) {
            return CastUnsafe<T>();
        }

        return RefCountedPtr<T, CountType>();
    }

    /*! \brief Drops the reference to the currently held value, if any.  */
    void Reset()
        { DropRefCount(); }

    /*! \brief Used by objects inheriting from this class or marshaling data. Not ideal to use externally */
    RefCountDataType *GetRefCountData() const
        { return m_ref; }

    template <class T>
    RefCountedPtr<T, CountType> CastUnsafe()
    {
        RefCountedPtr<T, CountType> rc;
        rc.m_ref = m_ref;

        if (m_ref) {
            ++m_ref->strong_count;
        }

        return rc;
    }

protected:
    explicit RefCountedPtrBase(RefCountDataType *ref)
        : m_ref(ref)
    {
    }

    void DropRefCount()
    {
        if (m_ref) {
            if (--m_ref->strong_count == 0u) {
                m_ref->Destruct();

                if (m_ref->weak_count == 0u) {
                    delete m_ref;
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
protected:
    using Base = RefCountedPtrBase<CountType>;

public:
    using Base::operator==;
    using Base::operator!=;
    using Base::operator!;
    using Base::operator bool;

    RefCountedPtr()
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

    explicit RefCountedPtr(const T &value)
        : Base(new typename Base::RefCountDataType)
    {
        Base::m_ref->template Construct<T>(value);
        Base::m_ref->strong_count = 1u;
        Base::m_ref->weak_count = 0u;
    }

    explicit RefCountedPtr(T &&value)
        : Base(new typename Base::RefCountDataType)
    {
        Base::m_ref->template Construct<T>(std::move(value));
        Base::m_ref->strong_count = 1u;
        Base::m_ref->weak_count = 0u;
    }

    RefCountedPtr(const RefCountedPtr &other)
        : Base(other)
    {
    }

    RefCountedPtr &operator=(const RefCountedPtr &other)
    {
        Base::operator=(other);

        return *this;
    }

    RefCountedPtr(RefCountedPtr &&other) noexcept
        : Base(std::move(other))
    {
    }

    RefCountedPtr &operator=(RefCountedPtr &&other) noexcept
    {
        Base::operator=(std::move(other));

        return *this;
    }

    ~RefCountedPtr() = default;

    HYP_FORCE_INLINE T *Get() const
        { return Base::m_ref ? static_cast<T *>(Base::m_ref->value) : nullptr; }

    HYP_FORCE_INLINE T *operator->() const
        { return Get(); }

    HYP_FORCE_INLINE T &operator*()
        { return *Get(); }

    HYP_FORCE_INLINE const T &operator*() const
        { return *Get(); }

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
protected:
    using Base = RefCountedPtrBase<CountType>;

public:
    using Base::operator==;
    using Base::operator!=;
    using Base::operator!;
    using Base::operator bool;

    RefCountedPtr()
        : Base()
    {
    }

    RefCountedPtr(const Base &other)
        : Base(other)
    {
    }

    RefCountedPtr &operator=(const Base &other)
    {
        Base::operator=(other);

        return *this;
    }

    RefCountedPtr(Base &&other) noexcept
        : Base(std::move(other))
    {
    }

    RefCountedPtr &operator=(Base &&other) noexcept
    {
        Base::operator=(std::move(other));

        return *this;
    }

    ~RefCountedPtr() = default;

    HYP_FORCE_INLINE void *Get() const
        { return Base::Get(); }

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

    HYP_FORCE_INLINE void *Get() const
        { return m_ref ? m_ref->value : nullptr; }

    explicit operator bool() const
        { return Get() != nullptr; }

    bool operator!() const
        { return Get() == nullptr; }

    bool operator==(const RefCountedPtrBase<CountType> &other) const
        { return m_ref == other.m_ref; }

    bool operator==(const WeakRefCountedPtrBase &other) const
        { return m_ref == other.m_ref; }

    bool operator==(std::nullptr_t) const
        { return Get() == nullptr; }

    bool operator!=(const RefCountedPtrBase<CountType> &other) const
        { return m_ref != other.m_ref; }

    bool operator!=(const WeakRefCountedPtrBase &other) const
        { return m_ref != other.m_ref; }

    bool operator!=(std::nullptr_t) const
        { return Get() != nullptr; }

    const TypeID &GetTypeID() const
        { return m_ref ? m_ref->type_id : TypeID::ForType<void>(); }

    /*! \brief Drops the reference to the currently held value, if any.  */
    void Reset()
        { DropRefCount(); }

    /*! \brief Used by objects inheriting from this class or marshaling data. Not ideal to use externally */
    RefCountDataType *GetRefCountData() const
        { return m_ref; }

protected:
    explicit WeakRefCountedPtrBase(RefCountDataType *ref)
        : m_ref(ref)
    {
    }

    void DropRefCount()
    {
        if (m_ref) {
            if (--m_ref->weak_count == 0u) {
                if (m_ref->strong_count == 0u) {
                    delete m_ref;
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

    HYP_FORCE_INLINE T *Get() const
        { return Base::m_ref ? static_cast<T *>(Base::m_ref->value) : nullptr; }

    RefCountedPtr<T, CountType> Lock()
    {
        RefCountedPtr<T, CountType> rc;
        rc.m_ref = Base::m_ref;

        if (Base::m_ref) {
            ++Base::m_ref->strong_count;
        }

        return rc;
    }
};

} // namespace detail

template <class T>
using AtomicRefCountedPtr = detail::RefCountedPtr<T, std::atomic<UInt>>;

template <class T>
using RefCountedPtr = detail::RefCountedPtr<T, UInt>;

template <class T>
using WeakAtomicRefCountedPtr = detail::WeakRefCountedPtr<T, std::atomic<UInt>>;

template <class T>
using WeakRefCountedPtr = detail::WeakRefCountedPtr<T, UInt>; 

} // namespace hyperion

#endif