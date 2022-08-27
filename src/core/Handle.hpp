#ifndef HYPERION_V2_CORE_HANDLE_HPP
#define HYPERION_V2_CORE_HANDLE_HPP

#include <core/lib/RefCountedPtr.hpp>
#include <core/lib/TypeID.hpp>

#include <Types.hpp>
#include <Constants.hpp>
#include <HashCode.hpp>
#include <util/Defines.hpp>

namespace hyperion {

struct IDBase {
    using ValueType = UInt;
    
    HYP_FORCE_INLINE explicit constexpr operator ValueType() const { return value; }
    HYP_FORCE_INLINE constexpr ValueType Value() const             { return value; }
    
    HYP_FORCE_INLINE explicit constexpr operator bool() const      { return bool(value); }

    HYP_FORCE_INLINE constexpr bool operator==(const IDBase &other) const
        { return value == other.value; }

    HYP_FORCE_INLINE constexpr bool operator!=(const IDBase &other) const
        { return value != other.value; }

    HYP_FORCE_INLINE constexpr bool operator<(const IDBase &other) const
        { return value < other.value; }

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

struct HandleID : IDBase {
    TypeID type_id;

    constexpr HandleID() = default;

    constexpr HandleID(TypeID type_id, IDBase::ValueType value)
        : IDBase { value },
          type_id(type_id)
    {
    }

    constexpr HandleID(const HandleID &other)
        : IDBase(other),
          type_id(other.type_id)
    {
    }

    HYP_FORCE_INLINE operator bool() const
        { return IDBase::operator bool(); }

    HYP_FORCE_INLINE bool operator==(const HandleID &other) const
        { return IDBase::operator==(other) && type_id == other.type_id; }

    HYP_FORCE_INLINE bool operator!=(const HandleID &other) const
        { return IDBase::operator!=(other) || type_id != other.type_id; }

    HYP_FORCE_INLINE bool operator<(const HandleID &other) const
        { return IDBase::operator<(other) || (!IDBase::operator<(other) && type_id < other.type_id); }
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
    using RefCountBase = Base::Base;
    using Base::Get;

    template <class T>
    static inline HandleID NextID()
    {
        static std::atomic<UInt> id_counter { 0u };

        return HandleID(TypeID::ForType<T>(), ++id_counter);
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

            if (ptr->GetId()) {
                // ptr already has ID set,
                // we take that ID
                m_id = ptr->GetId();//HandleID(TypeID::ForType<NormalizedType<T>>(), ptr->GetId().value);
            }/* else {// temp
                m_id = NextID<T>();
                ptr->SetId(m_id);
            }*/
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

            if (auto *ptr = static_cast<T *>(Base::m_ref->value)) {
                if (ptr->GetId()) {
                    // ptr already has ID set,
                    // we take that ID
                    m_id = ptr->GetId();HandleID(TypeID::ForType<NormalizedType<T>>(), ptr->GetId().value);
                } /*else { // temp
                    m_id = NextID<T>();
                    ptr->SetId(m_id);
                }*/
            }
        }
    }

public:
    HandleBase()
        : Base()
    {
    }

    HandleBase(const HandleBase &other)
        : Base(other),
          m_id(other.m_id)
    {
    }

    HandleBase &operator=(const HandleBase &other)
    {
        Base::operator=(other);

        m_id = other.m_id;

        return *this;
    }

    HandleBase(HandleBase &&other) noexcept
        : Base(std::move(other)),
          m_id(other.m_id)
    {
        other.m_id = HandleID { };
    }

    HandleBase &operator=(HandleBase &&other) noexcept
    {
        Base::operator=(std::move(other));

        m_id = other.m_id;

        other.m_id = HandleID { };

        return *this;
    }

    ~HandleBase() = default;

    HYP_FORCE_INLINE void *Get() const
        { return Base::Get(); }

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

    HYP_FORCE_INLINE const HandleID &GetID() const
        { return m_id; }

    /*! \brief Drops the reference to the currently held value, if any.  */
    HYP_FORCE_INLINE void Reset()
        { Base::Reset(); m_id = HandleID();  }

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

protected:
    HandleID m_id;
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
    using ID = HandleID;

    using Base::GetID;
    using Base::operator bool;
    using Base::operator!;

    static const inline Handle empty = Handle();

    Handle()
        : Base()
    {
    }

    Handle(const Handle &other)
        : Base(other)
    {
    }

    Handle &operator=(const Handle &other)
    {
        Base::operator=(other);

        return *this;
    }

    Handle(Handle &&other) noexcept
        : Base(std::move(other))
    {
    }

    Handle &operator=(Handle &&other) noexcept
    {
        Base::operator=(std::move(other));

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

    /*! \brief Used by ComponentSystem. */
    void SetID(const ID &id)
    {
        Base::m_id = id;
        if (auto *ptr = Get()) {
            ptr->SetId(m_id);
        }
    }

private:
};

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
        : Base(other),
          m_id(other.m_id)
    {
    }

    WeakHandleBase &operator=(const WeakHandleBase &other)
    {
        Base::operator=(other);
        m_id = other.m_id;

        return *this;
    }

    WeakHandleBase(const HandleBase &other)
        : Base(other),
          m_id(other.m_id)
    {
    }

    WeakHandleBase &operator=(const HandleBase &other)
    {
        Base::operator=(other);
        m_id = other.m_id;

        return *this;
    }

    WeakHandleBase(WeakHandleBase &&other) noexcept
        : Base(std::move(other)),
          m_id(std::move(other.m_id))
    {
        other.m_id = HandleID();
    }

    WeakHandleBase &operator=(WeakHandleBase &&other) noexcept
    {
        Base::operator=(std::move(other));
        m_id = std::move(other.m_id);
        other.m_id = HandleID();

        return *this;
    }

    ~WeakHandleBase() = default;

    HYP_FORCE_INLINE bool operator==(const HandleBase &other) const
        { return m_ref == other.m_ref; }

    HYP_FORCE_INLINE bool operator!=(const HandleBase &other) const
        { return m_ref != other.m_ref; }

    HYP_FORCE_INLINE const HandleID &GetID() const
        { return m_id; }

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
        h.m_id = m_id;

        if (Base::m_ref) {
            ++Base::m_ref->strong_count;
        }

        return h;
    }

    HandleID m_id;
};

template <class T>
class WeakHandle : public WeakHandleBase
{
    using Base = WeakHandleBase;

public:
    using ID = HandleID;

    using Base::GetID;
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