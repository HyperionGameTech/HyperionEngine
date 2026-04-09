/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Util.hpp>
#include <Core/Types.hpp>
#include <Core/HashCode.hpp>

#include <Core/reflection/ObjectMacros.hpp>

namespace Hyperion {
namespace utilities {

using TypeIdValue = uint32;

#define CONSTEXPR_TYPE_ID(T) (!std::is_void_v<T> ? ((TypeNameWithoutNamespace<T>().GetHashCode().Value() % HashCode::ValueType(0x7FFFFFFFu)) << 1) : 0)
#define TYPE_ID_FROM_STRING(str) ((HashCode::GetHashCode(str).Value() % HashCode::ValueType(0x7FFFFFFFu)) << 1)

/*! \brief Simple 32-bit identifier for a given type. Stable across DLLs as the type hash is based on the name of the type. */
HYP_STRUCT()
struct TypeId
{
    HYP_STRUCT_BODY(TypeId);

    using ValueType = TypeIdValue;

private:
    ValueType m_value;

    static constexpr ValueType VoidValue = ValueType(0);

public:
    template <class T>
#if HYP_TYPE_ID_COMPILE_TIME
    static HYP_CONSTEVAL TypeId ForType()
#else
    static TypeId ForType()
#endif
    {
#if HYP_TYPE_ID_COMPILE_TIME
        return TypeId { TypeIdValue(CONSTEXPR_TYPE_ID(T)) };
#else
        static const TypeId s_typeId { TypeIdValue(CONSTEXPR_TYPE_ID(T)) };
        return s_typeId;
#endif
    }

#if HYP_TYPE_ID_COMPILE_TIME
    static constexpr TypeId ForManagedType(const char* str)
#else
    static TypeId ForManagedType(const char* str)
#endif
    {
        return TypeId { TypeIdValue(TYPE_ID_FROM_STRING(str) | 0x1) };
    }

    constexpr TypeId()
        : m_value { VoidValue }
    {
    }

    explicit constexpr TypeId(ValueType id)
        : m_value(id)
    {
    }

    constexpr TypeId(const TypeId& other) = default;
    TypeId& operator=(const TypeId& other) = default;

    constexpr TypeId(TypeId&& other) noexcept
        : m_value(other.m_value)
    {
        other.m_value = VoidValue;
    }

    constexpr TypeId& operator=(TypeId&& other) noexcept
    {
        m_value = other.m_value;
        other.m_value = VoidValue;

        return *this;
    }

    constexpr TypeId& operator=(ValueType id)
    {
        m_value = id;

        return *this;
    }

    HYP_FORCE_INLINE constexpr explicit operator bool() const
    {
        return m_value != VoidValue;
    }

    HYP_FORCE_INLINE constexpr bool operator!() const
    {
        return m_value == VoidValue;
    }

    HYP_FORCE_INLINE constexpr bool operator==(const TypeId& other) const
    {
        return m_value == other.m_value;
    }

    HYP_FORCE_INLINE constexpr bool operator!=(const TypeId& other) const
    {
        return m_value != other.m_value;
    }

    HYP_FORCE_INLINE constexpr bool operator<(const TypeId& other) const
    {
        return m_value < other.m_value;
    }

    HYP_FORCE_INLINE constexpr bool operator<=(const TypeId& other) const
    {
        return m_value <= other.m_value;
    }

    HYP_FORCE_INLINE constexpr bool operator>(const TypeId& other) const
    {
        return m_value > other.m_value;
    }

    HYP_FORCE_INLINE constexpr bool operator>=(const TypeId& other) const
    {
        return m_value >= other.m_value;
    }

    HYP_FORCE_INLINE constexpr bool IsNativeType() const
    {
        return (m_value & 0x1) == 0x0;
    }

    HYP_FORCE_INLINE constexpr bool IsDynamicType() const
    {
        return (m_value & 0x1) == 0x1;
    }

    HYP_FORCE_INLINE constexpr ValueType Value() const
    {
        return m_value;
    }

    HYP_FORCE_INLINE constexpr HashCode GetHashCode() const
    {
        return HashCode(HashCode::ValueType(m_value));
    }

    HYP_FORCE_INLINE static constexpr TypeId Void()
    {
        return TypeId { VoidValue };
    }
};

template <class T>
const TypeId& TypeIdOf()
{
    static TypeId s_typeId = TypeId::ForType<T>();
    return s_typeId;
}

} // namespace utilities

using utilities::TypeId;
using utilities::TypeIdOf;

} // namespace Hyperion
