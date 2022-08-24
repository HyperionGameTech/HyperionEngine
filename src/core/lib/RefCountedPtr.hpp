#ifndef HYPERION_V2_LIB_REF_COUNTED_PTR_HPP
#define HYPERION_V2_LIB_REF_COUNTED_PTR_HPP

#include <core/lib/TypeID.hpp>
#include <Types.hpp>
#include <Constants.hpp>

#include <atomic>
#include <cstdlib>

namespace hyperion {
namespace detail {

template <class CountType = UInt>
struct RefCountData
{
    using Destructor = std::add_pointer_t<void(void *)>;

    void *value;
    TypeID type_id;
    CountType strong_count;
    CountType weak_count;
    Destructor dtor;

    template <class T, class ...Args>
    void Construct(Args &&... args)
    {
        using Normalized = NormalizedType<T>;

        value = std::aligned_alloc(alignof(Normalized), sizeof(Normalized));
        type_id = TypeID::ForType<Normalized>();
        new (static_cast<Normalized *>(value)) Normalized(std::forward<Args>(args)...);
        dtor = [](void *ptr) { static_cast<Normalized *>(ptr)->~T(); std::free(ptr); };
    }

    template <class T>
    void TakeOwnership(T *ptr)
    {
        value = ptr;
        type_id = TypeID::ForType<T>();
        dtor = [](void *ptr) { delete static_cast<T *>(ptr); };
    }

    void Destruct()
    {
        dtor(value);
        type_id = TypeID::ForType<void>();
        value = nullptr;
        dtor = nullptr;
    }
};

template <class T, class CountType>
class RefCountedPtr;

template <class CountType = UInt>
class RefCountedPtrBase
{
protected:
    using RefCountDataType = RefCountData<CountType>;

public:
    RefCountedPtrBase()
        : m_ref(nullptr)
    {
    }

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

    ~RefCountedPtrBase()
    {
        DropRefCount();
    }

    void *Get()
        { return m_ref ? m_ref->value : nullptr; }

    const void *Get() const
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

    template <class T>
    RefCountedPtr<T, CountType> Cast()
    {
        RefCountedPtr<T, CountType> rc;
        rc.m_ref = m_ref;

        if (m_ref) {
            ++m_ref->strong_count;
        }

        return rc;
    }

    /*! \brief Drops the reference to the currently held value, if any.  */
    void Reset()
        { DropRefCount(); }

    /*! \brief Used by objects inheriting from this class or marshaling data. Not ideal to use externally */
    RefCountDataType *GetRefCountData() const
        { return m_ref; }

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

    T *Get()
        { return Base::m_ref ? static_cast<T *>(Base::m_ref->value) : nullptr; }

    const T *Get() const
        { return const_cast<RefCountedPtr *>(this)->Get(); }

    T *operator->()
        { return Get(); }

    const T *operator->() const
        { return Get(); }

    T &operator*()
        { return *Get(); }

    const T &operator*() const
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

    void *Get()
        { return Base::Get(); }

    const void *Get() const
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

    void *Get()
        { return m_ref ? m_ref->value : nullptr; }

    const void *Get() const
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

    T *Get()
        { return Base::m_ref ? static_cast<T *>(Base::m_ref->value) : nullptr; }

    const T *Get() const
        { return const_cast<WeakRefCountedPtr *>(this)->Get(); }

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