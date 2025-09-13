/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Name.hpp>
#include <core/Util.hpp>

#include <core/Types.hpp>
#include <core/HashCode.hpp>

#include <atomic>

namespace hyperion {
namespace utilities {

using TypeIdValue = uint32;

static constexpr uint32 g_typeIdHashBitOffset = 1;
static constexpr uint32 g_typeIdHashMax = (~0u << g_typeIdHashBitOffset) >> g_typeIdHashBitOffset;
static constexpr uint32 g_typeIdFlagBitMask = 0x1u;
static constexpr uint32 g_typeIdFlagMax = 0x1u;

enum TypeIdFlags : uint8
{
    TYPE_ID_FLAGS_NONE = 0x0,
    TYPE_ID_FLAGS_DYNAMIC = 0x1 // Type is dynamic - does not map 1:1 to a native C++ type. E.g C# class
};

template <class T, uint8 Flags>
struct TypeId_Impl
{
    static_assert(Flags <= g_typeIdFlagMax, "Flags cannot be > 0x3");

    static constexpr TypeIdValue value = ((TypeNameWithoutNamespace<T>().GetHashCode().Value() % HashCode::ValueType(g_typeIdHashMax)) << g_typeIdHashBitOffset) | (Flags & g_typeIdFlagMax);
};

template <uint8 Flags>
struct TypeId_Impl<void, Flags>
{
    static constexpr TypeIdValue value = 0;
};

template <uint8 Flags>
struct TypeId_FromString_Impl
{
    static_assert(Flags <= g_typeIdFlagMax, "Flags cannot be > 0x3");

    constexpr TypeIdValue operator()(const char* str) const
    {
        return ((HashCode::GetHashCode(str).Value() % HashCode::ValueType(g_typeIdHashMax)) << g_typeIdHashBitOffset) | (Flags & g_typeIdFlagMax);
    }
};

/*! \brief Simple 32-bit identifier for a given type. Stable across DLLs as the type hash is based on the name of the type. */
HYP_STRUCT()
struct TypeId
{
    using ValueType = TypeIdValue;

private:
    ValueType m_value;

    static constexpr ValueType voidValue = ValueType(0);

public:
    template <class T>
    static constexpr TypeId ForType()
    {
        return TypeId { TypeId_Impl<NormalizedType<T>, TypeIdFlags::TYPE_ID_FLAGS_NONE>::value };
    }

    static constexpr TypeId ForManagedType(const char* str)
    {
        return TypeId { TypeId_FromString_Impl<TypeIdFlags::TYPE_ID_FLAGS_DYNAMIC> {}(str) };
    }

    constexpr TypeId()
        : m_value { voidValue }
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
        other.m_value = voidValue;
    }

    constexpr TypeId& operator=(TypeId&& other) noexcept
    {
        m_value = other.m_value;
        other.m_value = voidValue;

        return *this;
    }

    constexpr TypeId& operator=(ValueType id)
    {
        m_value = id;

        return *this;
    }

    HYP_FORCE_INLINE constexpr explicit operator bool() const
    {
        return m_value != voidValue;
    }

    HYP_FORCE_INLINE constexpr bool operator!() const
    {
        return m_value == voidValue;
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
        return !((m_value & g_typeIdFlagMax) & TYPE_ID_FLAGS_DYNAMIC);
    }

    HYP_FORCE_INLINE constexpr bool IsDynamicType() const
    {
        return (m_value & g_typeIdFlagMax) & TYPE_ID_FLAGS_DYNAMIC;
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
        return TypeId { voidValue };
    }
};

} // namespace utilities

using utilities::TypeId;

} // namespace hyperion
