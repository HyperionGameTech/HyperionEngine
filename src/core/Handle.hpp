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

    HandleID() = default;

    HandleID(TypeID type_id, IDBase::ValueType value)
        : IDBase { value },
          type_id(type_id)
    {
    }

    HandleID(const HandleID &other)
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

class HandleBase : AtomicRefCountedPtr<void>
{
protected:
    using Base = AtomicRefCountedPtr<void>;
    using Base::Get;

    template <class T>
    static inline HandleID NextID()
    {
        static std::atomic<UInt> id_counter { 0u };

        return HandleID(TypeID::ForType<T>(), ++id_counter);
    }

public:
    HandleBase()
        : Base()
    {
    }

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
                m_id = HandleID(TypeID::ForType<NormalizedType<T>>(), ptr->GetId().value);
            } else {
                m_id = NextID<T>();
                ptr->SetId(typename T::ID { m_id.value });
            }
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
                    m_id = HandleID(TypeID::ForType<NormalizedType<T>>(), ptr->GetId().value);
                } else {
                    m_id = NextID<T>();
                    ptr->SetId(typename T::ID { m_id.value });
                }
            }
        }
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

    HYP_FORCE_INLINE void *Get()
        { return Base::Get(); }

    HYP_FORCE_INLINE const void *Get() const
        { return Base::Get(); }

    HYP_FORCE_INLINE operator bool() const
        { return Base::operator bool() && m_id; }

    HYP_FORCE_INLINE bool operator!() const
        { return Base::operator!() || !m_id; }

    HYP_FORCE_INLINE bool operator==(const HandleBase &other) const
        { return Base::operator==(other) && m_id == other.m_id; }

    HYP_FORCE_INLINE bool operator==(std::nullptr_t) const
        { return Base::operator==(nullptr); }

    HYP_FORCE_INLINE bool operator!=(const HandleBase &other) const
        { return Base::operator!=(other) || m_id != other.m_id; }

    HYP_FORCE_INLINE bool operator!=(std::nullptr_t) const
        { return Base::operator!=(nullptr); }

    HYP_FORCE_INLINE const HandleID &GetID() const
        { return m_id; }

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

    /*! \brief Drops the reference to the currently held value, if any.  */
    HYP_FORCE_INLINE void Reset()
        { Base::Reset(); }

protected:
    HandleID m_id;
};

template <class T>
class Handle : public HandleBase
{
    friend class HandleBase;

public:
    using ID = HandleID;

    using HandleBase::GetID;
    using HandleBase::operator bool;
    using HandleBase::operator!;

    static const inline Handle empty = Handle();

    Handle()
        : HandleBase()
    {
    }

    explicit Handle(T *ptr)
        : HandleBase(ptr)
    {
    }

    /*! \brief Initialize a Handle from an atomicaly reference counted pointer.
        The resource will be shared, the handle referencing the same resource. */
    explicit Handle(const AtomicRefCountedPtr<T> &ref_counted_ptr)
        : HandleBase(ref_counted_ptr)
    {
    }

    Handle(const Handle &other)
        : HandleBase(other)
    {
    }

    Handle &operator=(const Handle &other)
    {
        HandleBase::operator=(other);

        return *this;
    }

    Handle(Handle &&other) noexcept
        : HandleBase(std::move(other))
    {
    }

    Handle &operator=(Handle &&other) noexcept
    {
        HandleBase::operator=(std::move(other));

        return *this;
    }

    ~Handle() = default;

    HYP_FORCE_INLINE bool operator==(const Handle &other) const
        { return HandleBase::operator==(other); }

    HYP_FORCE_INLINE bool operator==(std::nullptr_t) const
        { return HandleBase::operator==(nullptr); }

    HYP_FORCE_INLINE bool operator!=(const Handle &other) const
        { return HandleBase::operator!=(other); }

    HYP_FORCE_INLINE bool operator!=(std::nullptr_t) const
        { return HandleBase::operator!=(nullptr); }

    HYP_FORCE_INLINE T *operator->()
        { return Get(); }

    HYP_FORCE_INLINE const T *operator->() const
        { return Get(); }

    HYP_FORCE_INLINE T &operator*()
        { return *Get(); }

    HYP_FORCE_INLINE const T &operator*() const
        { return *Get(); }

    HYP_FORCE_INLINE T *Get()
        { return static_cast<T *>(HandleBase::Get()); }

    HYP_FORCE_INLINE const T *Get() const
        { return const_cast<Handle *>(this)->Get(); }
};

#if 0
class WeakHandleBase : WeakAtomicRefCountedPtr<void>
{
protected:
    using Base = WeakAtomicRefCountedPtr<void>;

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
};

template <class T>
class WeakHandle : public WeakHandleBase
{
    
};
#endif

} // namespace hyperion

#endif