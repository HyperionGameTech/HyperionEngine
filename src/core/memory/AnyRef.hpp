/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ANY_REF_HPP
#define HYPERION_ANY_REF_HPP

#include <core/utilities/TypeID.hpp>
#include <core/Defines.hpp>

#include <Types.hpp>

#include <type_traits>

namespace hyperion {

class HypClass;

extern HYP_API const HypClass *GetClass(TypeID type_id);
extern HYP_API bool IsInstanceOfHypClass(const HypClass *hyp_class, const void *ptr, TypeID type_id);

namespace memory {

class Any;
class Copyable;

namespace detail {

class AnyBase;

/*! \brief A non-owning reference to an object of any type.
 *  Type enforcement is done at runtime. */
class AnyRefBase
{
    using PointerType = void *;

public:
    friend class detail::AnyBase;
    friend class Any;
    friend class CopyableAny;

    AnyRefBase(TypeID type_id, PointerType ptr)
        : m_type_id(type_id),
          m_ptr(ptr)
    {
    }

    AnyRefBase(const AnyRefBase &other)
        : m_type_id(other.m_type_id),
          m_ptr(other.m_ptr)
    {
    }

    AnyRefBase &operator=(const AnyRefBase &other)
    {
        if (this == &other) {
            return *this;
        }

        m_type_id = other.m_type_id;
        m_ptr = other.m_ptr;

        return *this;
    }

    AnyRefBase(AnyRefBase &&other) noexcept
        : m_type_id(std::move(other.m_type_id)),
          m_ptr(other.m_ptr)
    {
        other.m_type_id = TypeID::ForType<void>();
        other.m_ptr = nullptr;
    }

    AnyRefBase &operator=(AnyRefBase &&other) noexcept
    {
        if (this == &other) {
            return *this;
        }

        m_type_id = other.m_type_id;
        m_ptr = other.m_ptr;

        other.m_type_id = TypeID::ForType<void>();
        other.m_ptr = nullptr;
        
        return *this;
    }

    ~AnyRefBase() = default;

    HYP_FORCE_INLINE explicit operator bool() const
        { return m_ptr != nullptr; }

    HYP_FORCE_INLINE bool operator!() const
        { return !m_ptr; }

    HYP_FORCE_INLINE bool operator==(const AnyRefBase &other) const
        { return m_ptr == other.m_ptr; }

    template <class OtherType>
    HYP_FORCE_INLINE bool operator==(const OtherType *ptr) const
        { return m_ptr == ptr; }

    HYP_FORCE_INLINE bool operator==(std::nullptr_t) const
        { return m_ptr == nullptr; }

    HYP_FORCE_INLINE bool operator!=(const AnyRefBase &other) const
        { return m_ptr != other.m_ptr; }

    template <class OtherType>
    HYP_FORCE_INLINE bool operator!=(const OtherType *ptr) const
        { return m_ptr != ptr; }

    HYP_FORCE_INLINE bool operator!=(std::nullptr_t) const
        { return m_ptr != nullptr; }

    /*! \brief Returns true if the AnyRef has a value. */
    HYP_FORCE_INLINE bool HasValue() const
        { return m_ptr != nullptr; }

    /*! \brief Returns the TypeID of the held object. */
    HYP_FORCE_INLINE TypeID GetTypeID() const
        { return m_type_id; }

    /*! \brief Returns the HypClass of the held object, if one is registered. */
    HYP_FORCE_INLINE const HypClass *GetHypClass() const
        { return GetClass(m_type_id); }

    /*! \brief Returns true if the held object is of type T.
     *  If T has a HypClass registered, this function will also return true if the held object is a subclass of T. */
    template <class T>
    HYP_FORCE_INLINE bool Is() const
    {
        constexpr TypeID type_id = TypeID::ForType<NormalizedType<T>>();

        return m_type_id == type_id
            || IsInstanceOfHypClass(GetClass(type_id), m_ptr, m_type_id);
    }

    /*! \brief Returns true if the held object is of type \ref{type_id}.
     *  If the type with the given ID has a HypClass registered, this function will also return true if the held object is a subclass of the type. */
    HYP_FORCE_INLINE bool Is(TypeID type_id) const
    {
        return m_type_id == type_id
            || IsInstanceOfHypClass(GetClass(type_id), m_ptr, m_type_id);
    }

    /*! \brief Resets the current value held in the AnyRef. */
    HYP_FORCE_INLINE void Reset()
    {
        m_type_id = TypeID::ForType<void>();
        m_ptr = nullptr;
    }

protected:
    TypeID      m_type_id;
    PointerType m_ptr;
};

} // namespace detail

class AnyRef : public detail::AnyRefBase
{
public:
    friend class detail::AnyBase;
    friend class Any;
    friend class CopyableAny;

    AnyRef()
        : AnyRefBase(TypeID::ForType<void>(), nullptr)
    {
    }

    AnyRef(TypeID type_id, void *ptr)
        : AnyRefBase(type_id, ptr)
    {
    }

    template <class T, typename = std::enable_if_t< !std::is_pointer_v< NormalizedType<T> > && !std::is_base_of_v< AnyRefBase, NormalizedType<T> > && !std::is_base_of_v< detail::AnyBase, NormalizedType<T> > > >
    explicit AnyRef(T &value)
        : AnyRefBase(TypeID::ForType<NormalizedType<T>>(), &value)
    {
    }

    template <class T, typename = std::enable_if_t< !std::is_pointer_v< NormalizedType<T> > && !std::is_base_of_v< AnyRefBase, NormalizedType<T> > && !std::is_base_of_v< detail::AnyBase, NormalizedType<T> > > >
    AnyRef &operator=(T &value)
    {
        const TypeID new_type_id = TypeID::ForType<NormalizedType<T>>();

        m_type_id = new_type_id;
        m_ptr = &value;

        return *this;
    }

    template <class T, typename = std::enable_if_t< !std::is_const_v< T > > >
    AnyRef(T *value)
        : AnyRefBase(TypeID::ForType<NormalizedType<T>>(), value)
    {
    }

    template <class T, typename = std::enable_if_t< !std::is_const_v< T > > >
    AnyRef &operator=(T *value)
    {
        const TypeID new_type_id = TypeID::ForType<NormalizedType<T>>();

        m_type_id = new_type_id;
        m_ptr = value;

        return *this;
    }

    AnyRef(const AnyRef &other)
        : AnyRefBase(static_cast<const AnyRefBase &>(other))
    {
    }

    AnyRef &operator=(const AnyRef &other)
    {
        static_cast<AnyRefBase &>(*this) = static_cast<const AnyRefBase &>(other);

        return *this;
    }

    AnyRef(AnyRef &&other) noexcept
        : AnyRefBase(static_cast<AnyRefBase &&>(other))
    {
    }

    AnyRef &operator=(AnyRef &&other) noexcept
    {
        static_cast<AnyRefBase &>(*this) = static_cast<AnyRefBase &&>(other);

        return *this;
    }

    ~AnyRef() = default;

    /*! \brief Get a raw pointer to the held object. */
    HYP_FORCE_INLINE void *GetPointer() const
        { return m_ptr; }

    /*! \brief Returns the held object as a reference to type T. If the held object is not of type T, an assertion will fail. */
    template <class T>
    HYP_FORCE_INLINE T &Get() const
    {
        constexpr TypeID requested_type_id = TypeID::ForType<NormalizedType<T>>();
        AssertThrowMsg(m_type_id == requested_type_id || IsInstanceOfHypClass(GetClass(requested_type_id), m_ptr, m_type_id), "Held type not equal to requested type!");

        return *static_cast<NormalizedType<T> *>(m_ptr);
    }

    /*! \brief Attempts to get the held object as a pointer to type T. If the held object is not of type T, nullptr is returned. */
    template <class T>
    HYP_FORCE_INLINE T *TryGet() const
    {
        constexpr TypeID requested_type_id = TypeID::ForType<NormalizedType<T>>();

        if (m_type_id == requested_type_id || IsInstanceOfHypClass(GetClass(requested_type_id), m_ptr, m_type_id)) {
            return static_cast<NormalizedType<T> *>(m_ptr);
        }

        return nullptr;
    }

    template <class T, typename = std::enable_if_t< !std::is_const_v< T > && !std::is_base_of_v< AnyRefBase, NormalizedType<T> > && !std::is_base_of_v< detail::AnyBase, NormalizedType<T> > > >
    void Set(T &value)
    {
        m_type_id = TypeID::ForType<NormalizedType<T>>();
        m_ptr = &value;
    }

    static AnyRef Empty()
        { return AnyRef(); }

    template <class T>
    static AnyRef Empty()
        { return AnyRef(TypeID::ForType<T>(), nullptr); }
};

class ConstAnyRef : public detail::AnyRefBase
{
public:
    friend class detail::AnyBase;
    friend class Any;
    friend class CopyableAny;

    ConstAnyRef()
        : AnyRefBase(TypeID::ForType<void>(), nullptr)
    {
    }

    ConstAnyRef(TypeID type_id, const void *ptr)
        : AnyRefBase(type_id, const_cast<void *>(ptr))
    {
    }

    template <class T, typename = std::enable_if_t< !std::is_pointer_v< NormalizedType<T> > && !std::is_base_of_v< AnyRefBase, NormalizedType<T> > && !std::is_base_of_v< detail::AnyBase, NormalizedType<T> > > >
    explicit ConstAnyRef(T &&value)
        : AnyRefBase(TypeID::ForType<NormalizedType<T>>(), const_cast<NormalizedType<T> *>(&value))
    {
        static_assert(std::is_lvalue_reference_v<T>, "Must be an lvalue reference to use this constructor");
    }

    template <class T, typename = std::enable_if_t< !std::is_pointer_v< NormalizedType<T> > && !std::is_base_of_v< AnyRefBase, NormalizedType<T> > && !std::is_base_of_v< detail::AnyBase, NormalizedType<T> > > >
    ConstAnyRef &operator=(T &&value)
    {
        static_assert(std::is_lvalue_reference_v<T>, "Must be an lvalue reference to use this constructor");

        const TypeID new_type_id = TypeID::ForType<NormalizedType<T>>();

        m_type_id = new_type_id;
        m_ptr = const_cast<T *>(&value);

        return *this;
    }

    template <class T>
    ConstAnyRef(const T *value)
        : AnyRefBase(TypeID::ForType<NormalizedType<T>>(), const_cast<NormalizedType<T> *>(value))
    {
    }

    template <class T>
    ConstAnyRef &operator=(const T *value)
    {
        const TypeID new_type_id = TypeID::ForType<NormalizedType<T>>();

        m_type_id = new_type_id;
        m_ptr = const_cast<T *>(value);

        return *this;
    }

    ConstAnyRef(const ConstAnyRef &other)
        : AnyRefBase(static_cast<const AnyRefBase &>(other))
    {
    }

    ConstAnyRef &operator=(const ConstAnyRef &other)
    {
        static_cast<AnyRefBase &>(*this) = static_cast<const AnyRefBase &>(other);

        return *this;
    }

    ConstAnyRef(ConstAnyRef &&other) noexcept
        : AnyRefBase(static_cast<AnyRefBase &&>(other))
    {
    }

    ConstAnyRef &operator=(ConstAnyRef &&other) noexcept
    {
        static_cast<AnyRefBase &>(*this) = static_cast<AnyRefBase &&>(other);

        return *this;
    }

    ConstAnyRef(const AnyRef &other) noexcept
        : AnyRefBase(static_cast<const AnyRefBase &>(other))
    {
    }

    ConstAnyRef &operator=(const AnyRef &other) noexcept
    {
        static_cast<AnyRefBase &>(*this) = static_cast<const AnyRefBase &>(other);

        return *this;
    }

    ConstAnyRef(AnyRef &&other) noexcept
        : AnyRefBase(static_cast<AnyRefBase &&>(other))
    {
    }

    ConstAnyRef &operator=(AnyRef &&other) noexcept
    {
        static_cast<AnyRefBase &>(*this) = static_cast<AnyRefBase &&>(other);

        return *this;
    }

    ~ConstAnyRef() = default;

    /*! \brief Get a raw pointer to the held object. */
    HYP_FORCE_INLINE const void *GetPointer() const
        { return m_ptr; }
    
    /*! \brief Returns the held object as a const reference to type T. If the held object is not of type T, an assertion will fail. */
    template <class T>
    HYP_FORCE_INLINE const T &Get() const
    {
        const TypeID requested_type_id = TypeID::ForType<NormalizedType<T>>();
        AssertThrowMsg(m_type_id == requested_type_id || IsInstanceOfHypClass(GetClass(requested_type_id), m_ptr, m_type_id), "Held type not equal to requested type!");

        return *static_cast<const NormalizedType<T> *>(m_ptr);
    }
    
    /*! \brief Attempts to get the held object as a const pointer to type T. If the held object is not of type T, nullptr is returned. */
    template <class T>
    HYP_FORCE_INLINE const T *TryGet() const
    {
        const TypeID requested_type_id = TypeID::ForType<NormalizedType<T>>();
        
        // fixme args
        if (m_type_id == requested_type_id || IsInstanceOfHypClass(GetClass(requested_type_id), m_ptr, m_type_id)) {
            return static_cast<const NormalizedType<T> *>(m_ptr);
        }

        return nullptr;
    }

    template <class T, typename = std::enable_if_t< !std::is_base_of_v< AnyRefBase, NormalizedType<T> > && !std::is_base_of_v< detail::AnyBase, NormalizedType<T> > > >
    void Set(const T &value)
    {
        m_type_id = TypeID::ForType<NormalizedType<T>>();
        m_ptr = const_cast<T *>(&value);
    }

    static ConstAnyRef Empty()
        { return ConstAnyRef(); }

    template <class T>
    static ConstAnyRef Empty()
        { return ConstAnyRef(TypeID::ForType<T>(), nullptr); }
};

} // namespace memory

using memory::AnyRef;
using memory::ConstAnyRef;

} // namespace hyperion

#endif