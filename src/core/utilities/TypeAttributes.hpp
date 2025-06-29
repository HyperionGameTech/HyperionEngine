/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_TYPE_ATTRIBUTES_HPP
#define HYPERION_TYPE_ATTRIBUTES_HPP

#include <core/utilities/TypeId.hpp>
#include <core/utilities/EnumFlags.hpp>
#include <core/Name.hpp>
#include <core/Util.hpp>

#include <Types.hpp>
#include <HashCode.hpp>

#include <type_traits>

namespace hyperion {

enum class TypeAttributeFlags : uint32
{
    NONE = 0x0,
    POD_TYPE = 0x1,
    CLASS_TYPE = 0x2,
    ENUM_TYPE = 0x4,
    FUNDAMENTAL_TYPE = 0x8,
    MATH_TYPE = 0x10,
    INTEGRAL_TYPE = 0x20,
    FLOAT_TYPE = 0x40,

    HYP_CLASS = 0x1000
};

HYP_MAKE_ENUM_FLAGS(TypeAttributeFlags)

class HypClass;
extern HYP_API const HypClass* GetClass(TypeId type_id);

namespace utilities {

struct TypeAttributes
{
    TypeId id = TypeId::Void();
    Name name = Name::Invalid();
    SizeType size = 0;
    SizeType alignment = 0;
    EnumFlags<TypeAttributeFlags> flags = TypeAttributeFlags::NONE;

    HYP_FORCE_INLINE constexpr operator bool() const
    {
        return IsValid();
    }

    HYP_FORCE_INLINE constexpr bool IsValid() const
    {
        return id != TypeId::Void();
    }

    template <class T>
    static TypeAttributes ForType()
    {
        constexpr TypeId type_id = TypeId::ForType<NormalizedType<T>>();

        EnumFlags<TypeAttributeFlags> flags = TypeAttributeFlags::NONE;

        if constexpr (std::is_class_v<NormalizedType<T>>)
        {
            flags |= TypeAttributeFlags::CLASS_TYPE;

            if (const HypClass* hyp_class = GetClass(type_id))
            {
                flags |= TypeAttributeFlags::HYP_CLASS;
            }

            if constexpr (is_pod_type<NormalizedType<T>>)
            {
                flags |= TypeAttributeFlags::POD_TYPE;
            }
        }

        if constexpr (std::is_enum_v<NormalizedType<T>>)
        {
            flags |= TypeAttributeFlags::ENUM_TYPE;
        }

        if constexpr (std::is_fundamental_v<NormalizedType<T>>)
        {
            flags |= TypeAttributeFlags::FUNDAMENTAL_TYPE;
        }

        if constexpr (std::is_arithmetic_v<NormalizedType<T>>)
        {
            flags |= TypeAttributeFlags::MATH_TYPE;
        }

        if constexpr (std::is_integral_v<NormalizedType<T>>)
        {
            flags |= TypeAttributeFlags::INTEGRAL_TYPE;
        }

        if constexpr (std::is_floating_point_v<NormalizedType<T>>)
        {
            flags |= TypeAttributeFlags::FLOAT_TYPE;
        }

        return {
            type_id,
            CreateNameFromDynamicString(TypeNameWithoutNamespace<NormalizedType<T>>().Data()),
            sizeof(NormalizedType<T>),
            alignof(NormalizedType<T>),
            flags
        };
    }

    HYP_FORCE_INLINE constexpr bool IsPOD() const
    {
        return flags & TypeAttributeFlags::POD_TYPE;
    }

    HYP_FORCE_INLINE constexpr bool IsClass() const
    {
        return flags & TypeAttributeFlags::CLASS_TYPE;
    }

    HYP_FORCE_INLINE constexpr bool IsEnum() const
    {
        return flags & TypeAttributeFlags::ENUM_TYPE;
    }

    HYP_FORCE_INLINE constexpr bool IsFundamental() const
    {
        return flags & TypeAttributeFlags::FUNDAMENTAL_TYPE;
    }

    HYP_FORCE_INLINE constexpr bool IsMathType() const
    {
        return flags & TypeAttributeFlags::MATH_TYPE;
    }

    HYP_FORCE_INLINE constexpr bool IsIntegralType() const
    {
        return flags & TypeAttributeFlags::INTEGRAL_TYPE;
    }

    HYP_FORCE_INLINE constexpr bool IsFloatType() const
    {
        return flags & TypeAttributeFlags::FLOAT_TYPE;
    }

    HYP_FORCE_INLINE constexpr bool HasHypClass() const
    {
        return flags & TypeAttributeFlags::HYP_CLASS;
    }

    HYP_FORCE_INLINE const HypClass* GetHypClass() const
    {
        return GetClass(id);
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(id.GetHashCode());
        hc.Add(name.GetHashCode());
        hc.Add(size);
        hc.Add(alignment);
        hc.Add(flags);

        return hc;
    }
};
} // namespace utilities

using utilities::TypeAttributes;

} // namespace hyperion

#endif
