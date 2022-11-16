#ifndef HYPERION_V2_CORE_HANDLE_HPP
#define HYPERION_V2_CORE_HANDLE_HPP

#include <core/lib/RefCountedPtr.hpp>
#include <core/lib/TypeID.hpp>

#include <Types.hpp>
#include <Constants.hpp>
#include <HashCode.hpp>
#include <util/Defines.hpp>

namespace hyperion {

namespace v2 {

class Engine;

} // namespace v2

struct IDBase
{
    using ValueType = UInt32;
    
    HYP_FORCE_INLINE explicit constexpr operator ValueType() const
        { return value; }
    
    HYP_FORCE_INLINE constexpr ValueType Value() const
        { return value; }
    
    HYP_FORCE_INLINE explicit constexpr operator bool() const
        { return bool(value); }

    HYP_FORCE_INLINE constexpr bool operator==(const IDBase &other) const
        { return value == other.value; }

    HYP_FORCE_INLINE constexpr bool operator!=(const IDBase &other) const
        { return value != other.value; }

    HYP_FORCE_INLINE constexpr bool operator<(const IDBase &other) const
        { return value < other.value; }

    /*! \brief If the value is non-zero, returns the ID minus one,
        to be used as a storage index. If the value is zero (invalid state),
        zero is returned. Ideally a validation check would be performed before you use this,
        unless you are totally sure that 0 is a valid index. */
    UInt ToIndex() const
        { return value ? value - 1 : 0; }

    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(value);

        return hc;
    }

    ValueType value { 0 };
};

template <class T>
struct ComponentID : IDBase {};

template <class T>
struct HandleID : IDBase
{
    // TypeID type_id;

    constexpr HandleID() = default;

    explicit HandleID(const IDBase &other)
        : IDBase(other)
    {
    }

    constexpr HandleID(IDBase::ValueType value)
        : IDBase { value }
    {
    }

    constexpr HandleID(const HandleID &other)
        : IDBase(other)
    {
    }

    HandleID &operator=(const IDBase &other) = delete; // delete so we cannot assign a type's ID to a different type
    HandleID &operator=(const HandleID &other) = default;

    HYP_FORCE_INLINE operator bool() const
        { return IDBase::operator bool(); }

    HYP_FORCE_INLINE bool operator==(const HandleID &other) const
        { return IDBase::operator==(other); }

    HYP_FORCE_INLINE bool operator!=(const HandleID &other) const
        { return IDBase::operator!=(other); }

    HYP_FORCE_INLINE bool operator<(const HandleID &other) const
        { return IDBase::operator<(other); }

    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(IDBase::GetHashCode());

        return hc;
    }
};

template <class T>
class Handle;

template <class T>
class WeakHandle;

class WeakHandleBase;

class HandleBase : AtomicRefCountedPtr<void>
{
    friend class hyperion::v2::Engine;
    friend class WeakHandleBase;

protected:
    using Base = AtomicRefCountedPtr<void>;
    using Base::Get;

    template <class T>
    static inline HandleID<T> NextID()
    {
        static std::atomic<UInt> id_counter { 0u };

        return HandleID<T>(++id_counter);
    }

    /* Engine class uses these constructors */

    template <class T>
    explicit HandleBase(T *ptr)
        : Base()
    {
        if (ptr) {
            Base::m_ref = new typename Base::RefCountDataType;
            Base::m_ref->template TakeOwnership<T>(ptr);
            Base::m_ref->strong_count = 1u;
            Base::m_ref->weak_count = 0u;
        }
    }

    /*! \brief Initialize a Handle from an atomicaly reference counted pointer.
        The resource will be shared, the handle referencing the same resource. */
    template <class T>
    explicit HandleBase(const AtomicRefCountedPtr<T> &ref_counted_ptr)
        : Base()
    {
        Base::m_ref = ref_counted_ptr.GetRefCountData();

        if (Base::m_ref) {
            ++Base::m_ref->strong_count;
        }
    }

public:
    HandleBase()
        : Base()
    {
    }

    HandleBase(const HandleBase &other)
        : Base(static_cast<const Base &>(other))
    {
    }

    HandleBase &operator=(const HandleBase &other)
    {
        if (std::addressof(other) == this) {
            return *this;
        }

        Base::operator=(static_cast<const Base &>(other));

        return *this;
    }

    HandleBase(HandleBase &&other) noexcept
        : Base(static_cast<Base &&>(std::move(other)))
    {
    }

    HandleBase &operator=(HandleBase &&other) noexcept
    {
        if (std::addressof(other) == this) {
            return *this;
        }

        Base::operator=(static_cast<Base &&>(std::move(other)));

        return *this;
    }

    ~HandleBase() = default;

    HYP_FORCE_INLINE void *Get() const
        { return Base::Get(); }

    HYP_FORCE_INLINE bool IsValid() const
        { return Base::operator bool(); }

    HYP_FORCE_INLINE operator bool() const
        { return Base::operator bool(); }

    HYP_FORCE_INLINE bool operator!() const
        { return Base::operator!(); }

    HYP_FORCE_INLINE bool operator==(const HandleBase &other) const
        { return Base::operator==(other); }

    HYP_FORCE_INLINE bool operator==(std::nullptr_t) const
        { return Base::operator==(nullptr); }

    HYP_FORCE_INLINE bool operator!=(const HandleBase &other) const
        { return Base::operator!=(other); }

    HYP_FORCE_INLINE bool operator!=(std::nullptr_t) const
        { return Base::operator!=(nullptr); }

    /*! \brief Drops the reference to the currently held value, if any.  */
    HYP_FORCE_INLINE void Reset()
        { Base::Reset(); }

    template <class T>
    Handle<T> Cast()
    {
        Handle<T> h;
        h.m_ref = Base::m_ref;

        if (Base::m_ref) {
            ++Base::m_ref->strong_count;
        }

        return h;
    }
};

template <class T>
class Handle : public HandleBase
{
    friend class hyperion::v2::Engine;
    friend class HandleBase;

    using Base = HandleBase;

protected:
    /* Engine class uses these constructors in CreateHandle<T> */

    explicit Handle(T *ptr)
        : Base(ptr)
    {
    }

    /*! \brief Initialize a Handle from an atomicaly reference counted pointer.
        The resource will be shared, the handle referencing the same resource. */
    explicit Handle(const AtomicRefCountedPtr<T> &ref_counted_ptr)
        : Base(ref_counted_ptr)
    {
    }

public:
    using ID = HandleID<T>;

    using Base::operator bool;
    using Base::operator!;

    static const Handle empty;

    Handle()
        : Base()
    {
    }

    Handle(const Handle &other)
        : Base(static_cast<const Base &>(other))
    {
    }

    Handle &operator=(const Handle &other)
    {
        Base::operator=(static_cast<const Base &>(other));

        return *this;
    }

    Handle(Handle &&other) noexcept
        : Base(static_cast<Base &&>(std::move(other)))
    {
        AssertThrow(other.Get() == nullptr);
    }

    Handle &operator=(Handle &&other) noexcept
    {
        Base::operator=(static_cast<Base &&>(std::move(other)));

        return *this;
    }

    ~Handle() = default;

    HYP_FORCE_INLINE bool operator==(const Handle &other) const
        { return Base::operator==(other); }

    HYP_FORCE_INLINE bool operator==(std::nullptr_t) const
        { return Base::operator==(nullptr); }

    HYP_FORCE_INLINE bool operator!=(const Handle &other) const
        { return Base::operator!=(other); }

    HYP_FORCE_INLINE bool operator!=(std::nullptr_t) const
        { return Base::operator!=(nullptr); }

    HYP_FORCE_INLINE T *operator->() const
        { return Get(); }

    HYP_FORCE_INLINE T &operator*()
        { return *Get(); }

    HYP_FORCE_INLINE const T &operator*() const
        { return *Get(); }

    HYP_FORCE_INLINE T *Get() const
        { return static_cast<T *>(Base::Get()); }
};

template <class T>
const Handle<T> Handle<T>::empty = Handle();

class WeakHandleBase : WeakAtomicRefCountedPtr<void>
{
    using Base = WeakAtomicRefCountedPtr<void>;

protected:
    using Base::m_ref;

public:
    using Base::operator==;
    using Base::operator!=;
    using Base::operator!;
    using Base::operator bool;
    using Base::Get;

    WeakHandleBase()
        : Base()
    {
    }

    WeakHandleBase(const WeakHandleBase &other)
        : Base(other)
    {
    }

    WeakHandleBase &operator=(const WeakHandleBase &other)
    {
        Base::operator=(other);

        return *this;
    }

    WeakHandleBase(const HandleBase &other)
        : Base(other)
    {
    }

    WeakHandleBase &operator=(const HandleBase &other)
    {
        Base::operator=(other);

        return *this;
    }

    WeakHandleBase(WeakHandleBase &&other) noexcept
        : Base(std::move(other))
    {
    }

    WeakHandleBase &operator=(WeakHandleBase &&other) noexcept
    {
        Base::operator=(std::move(other));

        return *this;
    }

    ~WeakHandleBase() = default;

    HYP_FORCE_INLINE bool operator==(const HandleBase &other) const
        { return Base::Get() == other.Get(); }

    HYP_FORCE_INLINE bool operator!=(const HandleBase &other) const
        { return Base::Get() != other.Get(); }

    template <class T>
    HYP_FORCE_INLINE Handle<T> Lock()
    {
        return (Base::GetTypeID() == TypeID::ForType<T>())
            ? LockUnsafe<T>()
            : Handle<T>();
    }

protected:
    template <class T>
    Handle<T> LockUnsafe()
    {
        Handle<T> h;
        h.m_ref = Base::m_ref;

        if (Base::m_ref) {
            ++Base::m_ref->strong_count;
        }

        return h;
    }
};

template <class T>
class WeakHandle : public WeakHandleBase
{
    using Base = WeakHandleBase;

public:
    using Base::operator bool;
    using Base::operator!;

    WeakHandle()
        : Base()
    {
    }

    WeakHandle(const WeakHandle &other)
        : Base(other)
    {
    }

    WeakHandle &operator=(const WeakHandle &other)
    {
        Base::operator=(other);

        return *this;
    }

    WeakHandle(const Handle<T> &other)
        : Base(other)
    {
    }

    WeakHandle &operator=(const Handle<T> &other)
    {
        Base::operator=(other);

        return *this;
    }

    WeakHandle(WeakHandle &&other) noexcept
        : Base(std::move(other))
    {
    }

    WeakHandle &operator=(WeakHandle &&other) noexcept
    {
        Base::operator=(std::move(other));

        return *this;
    }

    ~WeakHandle() = default;

    HYP_FORCE_INLINE bool operator==(const WeakHandle &other) const
        { return Base::operator==(other); }

    HYP_FORCE_INLINE bool operator==(std::nullptr_t) const
        { return Base::operator==(nullptr); }

    HYP_FORCE_INLINE bool operator!=(const WeakHandle &other) const
        { return Base::operator!=(other); }

    HYP_FORCE_INLINE bool operator!=(std::nullptr_t) const
        { return Base::operator!=(nullptr); }

    HYP_FORCE_INLINE T *operator->() const
        { return Get(); }

    HYP_FORCE_INLINE T &operator*()
        { return *Get(); }

    HYP_FORCE_INLINE const T &operator*() const
        { return *Get(); }

    HYP_FORCE_INLINE T *Get() const
        { return static_cast<T *>(Base::Get()); }

    HYP_FORCE_INLINE Handle<T> Lock()
        { return Base::template LockUnsafe<T>(); }
};

static_assert(sizeof(Handle<int>) == sizeof(HandleBase), "No members may be added to Handle<T>");
static_assert(sizeof(WeakHandle<int>) == sizeof(WeakHandleBase), "No members may be added to WeakHandle<T>");

} // namespace hyperion

#endif