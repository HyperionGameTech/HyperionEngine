/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/utilities/TypeId.hpp>
#include <core/Defines.hpp>

#include <core/Types.hpp>

#include <type_traits>

namespace hyperion {

class HypClass;

extern HYP_API const HypClass* GetClass(TypeId typeId);
extern HYP_API bool IsA(const HypClass* hypClass, const void* ptr, TypeId typeId);

namespace memory {

class Any;
class Copyable;

class AnyBase;

/*! \brief A non-owning reference to an object of any type.
 *  Type enforcement is done at runtime. */
class AnyRefBase
{
    using PointerType = void*;

public:
    friend class AnyBase;
    friend class Any;
    friend class Any;

    AnyRefBase(TypeId typeId, PointerType ptr)
        : m_typeId(typeId),
          m_ptr(ptr)
    {
    }

    AnyRefBase(const AnyRefBase& other)
        : m_typeId(other.m_typeId),
          m_ptr(other.m_ptr)
    {
    }

    AnyRefBase& operator=(const AnyRefBase& other)
    {
        if (this == &other)
        {
            return *this;
        }

        m_typeId = other.m_typeId;
        m_ptr = other.m_ptr;

        return *this;
    }

    AnyRefBase(AnyRefBase&& other) noexcept
        : m_typeId(std::move(other.m_typeId)),
          m_ptr(other.m_ptr)
    {
        other.m_typeId = TypeId::ForType<void>();
        other.m_ptr = nullptr;
    }

    AnyRefBase& operator=(AnyRefBase&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        m_typeId = other.m_typeId;
        m_ptr = other.m_ptr;

        other.m_typeId = TypeId::ForType<void>();
        other.m_ptr = nullptr;

        return *this;
    }

    ~AnyRefBase() = default;

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return m_ptr != nullptr;
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return !m_ptr;
    }

    HYP_FORCE_INLINE bool operator==(const AnyRefBase& other) const
    {
        return m_ptr == other.m_ptr;
    }

    template <class OtherType>
    HYP_FORCE_INLINE bool operator==(const OtherType* ptr) const
    {
        return m_ptr == ptr;
    }

    HYP_FORCE_INLINE bool operator==(std::nullptr_t) const
    {
        return m_ptr == nullptr;
    }

    HYP_FORCE_INLINE bool operator!=(const AnyRefBase& other) const
    {
        return m_ptr != other.m_ptr;
    }

    template <class OtherType>
    HYP_FORCE_INLINE bool operator!=(const OtherType* ptr) const
    {
        return m_ptr != ptr;
    }

    HYP_FORCE_INLINE bool operator!=(std::nullptr_t) const
    {
        return m_ptr != nullptr;
    }

    /*! \brief Returns true if the AnyRef has a value. */
    HYP_FORCE_INLINE bool HasValue() const
    {
        return m_ptr != nullptr;
    }

    /*! \brief Returns the TypeId of the held object. */
    HYP_FORCE_INLINE TypeId GetTypeId() const
    {
        return m_typeId;
    }

    /*! \brief Returns the HypClass of the held object, if one is registered. */
    HYP_FORCE_INLINE const HypClass* GetHypClass() const
    {
        return GetClass(m_typeId);
    }

    /*! \brief Returns true if the held object is of type T.
     *  If T has a HypClass registered, this function will also return true if the held object is a subclass of T. */
    template <class T>
    HYP_FORCE_INLINE bool Is() const
    {
        constexpr TypeId typeId = TypeId::ForType<NormalizedType<T>>();

        return m_typeId == typeId
            || IsA(GetClass(typeId), m_ptr, m_typeId);
    }

    /*! \brief Returns true if the held object is of type \ref{typeId}.
     *  If the type with the given Id has a HypClass registered, this function will also return true if the held object is a subclass of the type. */
    HYP_FORCE_INLINE bool Is(TypeId typeId) const
    {
        return m_typeId == typeId
            || IsA(GetClass(typeId), m_ptr, m_typeId);
    }

    /*! \brief Resets the current value held in the AnyRef. */
    HYP_FORCE_INLINE void Reset()
    {
        m_typeId = TypeId::ForType<void>();
        m_ptr = nullptr;
    }

protected:
    TypeId m_typeId;
    PointerType m_ptr;
};

class AnyRef : public AnyRefBase
{
public:
    friend class AnyBase;
    friend class Any;
    friend class Any;

    AnyRef()
        : AnyRefBase(TypeId::ForType<void>(), nullptr)
    {
    }

    AnyRef(TypeId typeId, void* ptr)
        : AnyRefBase(typeId, ptr)
    {
    }

    template <class T, typename = std::enable_if_t<!std::is_pointer_v<NormalizedType<T>> && !std::is_base_of_v<AnyRefBase, NormalizedType<T>> && !std::is_base_of_v<AnyBase, NormalizedType<T>>>>
    explicit AnyRef(T& value)
        : AnyRefBase(TypeId::ForType<NormalizedType<T>>(), &value)
    {
    }

    template <class T, typename = std::enable_if_t<!std::is_pointer_v<NormalizedType<T>> && !std::is_base_of_v<AnyRefBase, NormalizedType<T>> && !std::is_base_of_v<AnyBase, NormalizedType<T>>>>
    AnyRef& operator=(T& value)
    {
        const TypeId newTypeId = TypeId::ForType<NormalizedType<T>>();

        m_typeId = newTypeId;
        m_ptr = &value;

        return *this;
    }

    template <class T, typename = std::enable_if_t<!std::is_const_v<T>>>
    AnyRef(T* value)
        : AnyRefBase(TypeId::ForType<NormalizedType<T>>(), value)
    {
    }

    template <class T>
    AnyRef(const AnyRefBase* value) = delete;

    AnyRef(const AnyRef& other)
        : AnyRefBase(static_cast<const AnyRefBase&>(other))
    {
    }

    AnyRef& operator=(const AnyRef& other)
    {
        static_cast<AnyRefBase&>(*this) = static_cast<const AnyRefBase&>(other);

        return *this;
    }

    AnyRef(AnyRef&& other) noexcept
        : AnyRefBase(static_cast<AnyRefBase&&>(other))
    {
    }

    AnyRef& operator=(AnyRef&& other) noexcept
    {
        static_cast<AnyRefBase&>(*this) = static_cast<AnyRefBase&&>(other);

        return *this;
    }

    ~AnyRef() = default;

    /*! \brief Get a raw pointer to the held object. */
    HYP_FORCE_INLINE void* GetPointer() const
    {
        return m_ptr;
    }

    /*! \brief Returns the held object as a reference to type T. If the held object is not of type T, an assertion will fail. */
    template <class T>
    HYP_FORCE_INLINE T& Get() const
    {
        constexpr TypeId requestedTypeId = TypeId::ForType<NormalizedType<T>>();
        HYP_CORE_ASSERT(m_typeId == requestedTypeId || IsA(GetClass(requestedTypeId), m_ptr, m_typeId), "Held type not equal to requested type!");

        return *static_cast<NormalizedType<T>*>(m_ptr);
    }

    /*! \brief Attempts to get the held object as a pointer to type T. If the held object is not of type T, nullptr is returned. */
    template <class T>
    HYP_FORCE_INLINE T* TryGet() const
    {
        constexpr TypeId requestedTypeId = TypeId::ForType<NormalizedType<T>>();

        if (m_typeId == requestedTypeId || IsA(GetClass(requestedTypeId), m_ptr, m_typeId))
        {
            return static_cast<NormalizedType<T>*>(m_ptr);
        }

        return nullptr;
    }

    template <class T, typename = std::enable_if_t<!std::is_const_v<T> && !std::is_base_of_v<AnyRefBase, NormalizedType<T>> && !std::is_base_of_v<AnyBase, NormalizedType<T>>>>
    void Set(T& value)
    {
        m_typeId = TypeId::ForType<NormalizedType<T>>();
        m_ptr = &value;
    }

    static AnyRef Empty()
    {
        return AnyRef();
    }

    template <class T>
    static AnyRef Empty()
    {
        return AnyRef(TypeId::ForType<T>(), nullptr);
    }
};

class ConstAnyRef : public AnyRefBase
{
public:
    friend class AnyBase;
    friend class Any;
    friend class Any;

    ConstAnyRef()
        : AnyRefBase(TypeId::ForType<void>(), nullptr)
    {
    }

    ConstAnyRef(TypeId typeId, const void* ptr)
        : AnyRefBase(typeId, const_cast<void*>(ptr))
    {
    }

    template <class T, typename = std::enable_if_t<!std::is_pointer_v<NormalizedType<T>> && !std::is_base_of_v<AnyRefBase, NormalizedType<T>> && !std::is_base_of_v<AnyBase, NormalizedType<T>>>>
    explicit ConstAnyRef(T&& value)
        : AnyRefBase(TypeId::ForType<NormalizedType<T>>(), const_cast<NormalizedType<T>*>(&value))
    {
        static_assert(std::is_lvalue_reference_v<T>, "Must be an lvalue reference to use this constructor");
    }

    template <class T, typename = std::enable_if_t<!std::is_pointer_v<NormalizedType<T>> && !std::is_base_of_v<AnyRefBase, NormalizedType<T>> && !std::is_base_of_v<AnyBase, NormalizedType<T>>>>
    ConstAnyRef& operator=(T&& value)
    {
        static_assert(std::is_lvalue_reference_v<T>, "Must be an lvalue reference to use this constructor");

        const TypeId newTypeId = TypeId::ForType<NormalizedType<T>>();

        m_typeId = newTypeId;
        m_ptr = const_cast<T*>(&value);

        return *this;
    }

    template <class T>
    ConstAnyRef(const T* value)
        : AnyRefBase(TypeId::ForType<NormalizedType<T>>(), const_cast<NormalizedType<T>*>(value))
    {
    }

    ConstAnyRef(const ConstAnyRef* value) = delete;

    ConstAnyRef(const ConstAnyRef& other)
        : AnyRefBase(static_cast<const AnyRefBase&>(other))
    {
    }

    ConstAnyRef& operator=(const ConstAnyRef& other)
    {
        static_cast<AnyRefBase&>(*this) = static_cast<const AnyRefBase&>(other);

        return *this;
    }

    ConstAnyRef(ConstAnyRef&& other) noexcept
        : AnyRefBase(static_cast<AnyRefBase&&>(other))
    {
    }

    ConstAnyRef& operator=(ConstAnyRef&& other) noexcept
    {
        static_cast<AnyRefBase&>(*this) = static_cast<AnyRefBase&&>(other);

        return *this;
    }

    ConstAnyRef(const AnyRef& other) noexcept
        : AnyRefBase(static_cast<const AnyRefBase&>(other))
    {
    }

    ConstAnyRef& operator=(const AnyRef& other) noexcept
    {
        static_cast<AnyRefBase&>(*this) = static_cast<const AnyRefBase&>(other);

        return *this;
    }

    ConstAnyRef(AnyRef&& other) noexcept
        : AnyRefBase(static_cast<AnyRefBase&&>(other))
    {
    }

    ConstAnyRef& operator=(AnyRef&& other) noexcept
    {
        static_cast<AnyRefBase&>(*this) = static_cast<AnyRefBase&&>(other);

        return *this;
    }

    ~ConstAnyRef() = default;

    /*! \brief Get a raw pointer to the held object. */
    HYP_FORCE_INLINE const void* GetPointer() const
    {
        return m_ptr;
    }

    /*! \brief Returns the held object as a const reference to type T. If the held object is not of type T, an assertion will fail. */
    template <class T>
    HYP_FORCE_INLINE const T& Get() const
    {
        const TypeId requestedTypeId = TypeId::ForType<NormalizedType<T>>();
        HYP_CORE_ASSERT(m_typeId == requestedTypeId || IsA(GetClass(requestedTypeId), m_ptr, m_typeId), "Held type not equal to requested type!");

        return *static_cast<const NormalizedType<T>*>(m_ptr);
    }

    /*! \brief Attempts to get the held object as a const pointer to type T. If the held object is not of type T, nullptr is returned. */
    template <class T>
    HYP_FORCE_INLINE const T* TryGet() const
    {
        const TypeId requestedTypeId = TypeId::ForType<NormalizedType<T>>();

        // fixme args
        if (m_typeId == requestedTypeId || IsA(GetClass(requestedTypeId), m_ptr, m_typeId))
        {
            return static_cast<const NormalizedType<T>*>(m_ptr);
        }

        return nullptr;
    }

    template <class T, typename = std::enable_if_t<!std::is_base_of_v<AnyRefBase, NormalizedType<T>> && !std::is_base_of_v<AnyBase, NormalizedType<T>>>>
    void Set(const T& value)
    {
        m_typeId = TypeId::ForType<NormalizedType<T>>();
        m_ptr = const_cast<T*>(&value);
    }

    static ConstAnyRef Empty()
    {
        return ConstAnyRef();
    }

    template <class T>
    static ConstAnyRef Empty()
    {
        return ConstAnyRef(TypeId::ForType<T>(), nullptr);
    }
};

} // namespace memory

using memory::AnyRef;
using memory::ConstAnyRef;

} // namespace hyperion
