/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ENUM_FLAGS_HPP
#define HYPERION_ENUM_FLAGS_HPP

#include <Constants.hpp>
#include <Types.hpp>
#include <HashCode.hpp>

#include <type_traits>

namespace hyperion {

template <class Enum>
struct EnumFlagsDecl
{
    static constexpr bool is_enum_flags = false;
};

template <class Enum>
struct EnumFlags
{
    using EnumType = NormalizedType<Enum>;
    using UnderlyingType = NormalizedType<std::underlying_type_t<EnumType>>;

    struct SubscriptWrapper
    {
        EnumFlags& enum_flags;
        EnumType flag;

        constexpr SubscriptWrapper(EnumFlags& enum_flags, EnumType flag)
            : enum_flags(enum_flags),
              flag(flag)
        {
        }

        SubscriptWrapper(const SubscriptWrapper& other) = delete;
        SubscriptWrapper& operator=(const SubscriptWrapper& other) = delete;
        SubscriptWrapper(SubscriptWrapper&& other) noexcept = delete;
        SubscriptWrapper& operator=(SubscriptWrapper&& other) noexcept = delete;

        constexpr HYP_FORCE_INLINE bool operator!() const
        {
            return !bool(*this);
        }

        constexpr HYP_FORCE_INLINE operator bool() const
        {
            return bool(UnderlyingType(enum_flags.value & static_cast<UnderlyingType>(flag)));
        }

        constexpr HYP_FORCE_INLINE bool operator==(bool value) const
        {
            return bool(UnderlyingType(enum_flags.value & static_cast<UnderlyingType>(flag))) == value;
        }

        constexpr HYP_FORCE_INLINE bool operator!=(bool value) const
        {
            return bool(UnderlyingType(enum_flags.value & static_cast<UnderlyingType>(flag))) != value;
        }

        constexpr HYP_FORCE_INLINE SubscriptWrapper& operator=(bool value)
        {
            if (value)
            {
                enum_flags.value |= static_cast<UnderlyingType>(flag);
            }
            else
            {
                enum_flags.value &= ~static_cast<UnderlyingType>(flag);
            }

            return *this;
        }
    };

    UnderlyingType value {};

    constexpr EnumFlags() = default;

    constexpr EnumFlags(EnumType value)
        : value(static_cast<UnderlyingType>(value))
    {
    }

    constexpr EnumFlags(UnderlyingType value)
        : value(value)
    {
    }

    constexpr EnumFlags(const EnumFlags& other) = default;
    EnumFlags& operator=(const EnumFlags& other) = default;
    constexpr EnumFlags(EnumFlags&& other) noexcept = default;
    EnumFlags& operator=(EnumFlags&& other) noexcept = default;
    ~EnumFlags() = default;

    HYP_FORCE_INLINE constexpr explicit operator bool() const
    {
        return bool(value);
    }

    HYP_FORCE_INLINE constexpr operator EnumType() const
    {
        return static_cast<EnumType>(value);
    }

    HYP_FORCE_INLINE constexpr operator UnderlyingType() const
    {
        return value;
    }

    HYP_FORCE_INLINE constexpr bool operator==(const EnumFlags& other) const
    {
        return value == other.value;
    }

    HYP_FORCE_INLINE constexpr bool operator==(Enum rhs) const
    {
        return *this == EnumFlags<Enum>(rhs);
    }

    HYP_FORCE_INLINE constexpr bool operator!=(const EnumFlags& other) const
    {
        return value != other.value;
    }

    HYP_FORCE_INLINE constexpr bool operator!=(Enum rhs) const
    {
        return *this != EnumFlags<Enum>(rhs);
    }

    HYP_FORCE_INLINE constexpr EnumFlags operator~() const
    {
        return EnumFlags(~value);
    }

    HYP_FORCE_INLINE constexpr EnumFlags operator|(const EnumFlags& other) const
    {
        return EnumFlags(value | other.value);
    }

    HYP_FORCE_INLINE EnumFlags& operator|=(const EnumFlags& other)
    {
        value |= other.value;

        return *this;
    }

    HYP_FORCE_INLINE constexpr EnumFlags operator|(EnumType flag) const
    {
        return EnumFlags(value | static_cast<UnderlyingType>(flag));
    }

    HYP_FORCE_INLINE EnumFlags& operator|=(EnumType flag)
    {
        value |= static_cast<UnderlyingType>(flag);

        return *this;
    }

    HYP_FORCE_INLINE constexpr EnumFlags operator&(const EnumFlags& other) const
    {
        return EnumFlags(value & other.value);
    }

    HYP_FORCE_INLINE EnumFlags& operator&=(const EnumFlags& other)
    {
        value &= other.value;

        return *this;
    }

    HYP_FORCE_INLINE constexpr EnumFlags operator&(EnumType flag) const
    {
        return EnumFlags(value & static_cast<UnderlyingType>(flag));
    }

    HYP_FORCE_INLINE EnumFlags& operator&=(EnumType flag)
    {
        value &= static_cast<UnderlyingType>(flag);

        return *this;
    }

    HYP_FORCE_INLINE constexpr EnumFlags operator^(const EnumFlags& other) const
    {
        return EnumFlags(value ^ other.value);
    }

    HYP_FORCE_INLINE EnumFlags& operator^=(const EnumFlags& other)
    {
        value ^= other.value;

        return *this;
    }

    HYP_FORCE_INLINE constexpr EnumFlags operator^(EnumType flag) const
    {
        return EnumFlags(value ^ static_cast<UnderlyingType>(flag));
    }

    HYP_FORCE_INLINE EnumFlags& operator^=(EnumType flag)
    {
        value ^= static_cast<UnderlyingType>(flag);

        return *this;
    }

    HYP_FORCE_INLINE constexpr EnumFlags operator<<(int bits) const
    {
        return EnumFlags(value << bits);
    }

    HYP_FORCE_INLINE EnumFlags& operator<<=(int bits)
    {
        value <<= bits;

        return *this;
    }

    HYP_FORCE_INLINE constexpr EnumFlags operator>>(int bits) const
    {
        return EnumFlags(value >> bits);
    }

    HYP_FORCE_INLINE EnumFlags& operator>>=(int bits)
    {
        value >>= bits;

        return *this;
    }

    HYP_FORCE_INLINE constexpr SubscriptWrapper operator[](EnumType flag)
    {
        return SubscriptWrapper { *this, flag };
    }

    HYP_FORCE_INLINE constexpr const SubscriptWrapper operator[](EnumType flag) const
    {
        return SubscriptWrapper { const_cast<EnumFlags&>(*this), flag };
    }

    HYP_FORCE_INLINE constexpr HashCode GetHashCode() const
    {
        return HashCode::GetHashCode(value);
    }
};

// Unary ~

template <class Enum, typename = std::enable_if_t<std::is_enum_v<Enum> && EnumFlagsDecl<Enum>::is_enum_flags>>
constexpr EnumFlags<Enum> operator~(Enum lhs)
{
    return EnumFlags<Enum>(~static_cast<typename EnumFlags<Enum>::UnderlyingType>(lhs));
}

// Binary bitwise operators

template <class Enum, typename = std::enable_if_t<std::is_enum_v<Enum> && EnumFlagsDecl<Enum>::is_enum_flags>>
constexpr EnumFlags<Enum> operator|(Enum lhs, EnumFlags<Enum> rhs)
{
    return EnumFlags<Enum>(lhs) | rhs;
}

template <class Enum, typename = std::enable_if_t<std::is_enum_v<Enum> && EnumFlagsDecl<Enum>::is_enum_flags>>
constexpr EnumFlags<Enum> operator|(Enum lhs, Enum rhs)
{
    return EnumFlags<Enum>(lhs) | EnumFlags<Enum>(rhs);
}

template <class Enum, typename = std::enable_if_t<std::is_enum_v<Enum> && EnumFlagsDecl<Enum>::is_enum_flags>>
EnumFlags<Enum>& operator|=(EnumFlags<Enum> lhs, Enum rhs)
{
    return lhs |= EnumFlags<Enum>(rhs);
}

template <class Enum, typename = std::enable_if_t<std::is_enum_v<Enum> && EnumFlagsDecl<Enum>::is_enum_flags>>
constexpr EnumFlags<Enum> operator&(Enum lhs, EnumFlags<Enum> rhs)
{
    return EnumFlags<Enum>(lhs) & rhs;
}

template <class Enum, typename = std::enable_if_t<std::is_enum_v<Enum> && EnumFlagsDecl<Enum>::is_enum_flags>>
constexpr EnumFlags<Enum> operator&(Enum lhs, Enum rhs)
{
    return EnumFlags<Enum>(lhs) & EnumFlags<Enum>(rhs);
}

template <class Enum, typename = std::enable_if_t<std::is_enum_v<Enum> && EnumFlagsDecl<Enum>::is_enum_flags>>
EnumFlags<Enum>& operator&=(EnumFlags<Enum> lhs, Enum rhs)
{
    return lhs &= EnumFlags<Enum>(rhs);
}

template <class Enum, typename = std::enable_if_t<std::is_enum_v<Enum> && EnumFlagsDecl<Enum>::is_enum_flags>>
constexpr EnumFlags<Enum> operator^(Enum lhs, EnumFlags<Enum> rhs)
{
    return EnumFlags<Enum>(lhs) ^ rhs;
}

template <class Enum, typename = std::enable_if_t<std::is_enum_v<Enum> && EnumFlagsDecl<Enum>::is_enum_flags>>
constexpr EnumFlags<Enum> operator^(Enum lhs, Enum rhs)
{
    return EnumFlags<Enum>(lhs) ^ EnumFlags<Enum>(rhs);
}

template <class Enum, typename = std::enable_if_t<std::is_enum_v<Enum> && EnumFlagsDecl<Enum>::is_enum_flags>>
EnumFlags<Enum>& operator^=(EnumFlags<Enum> lhs, Enum rhs)
{
    return lhs ^= EnumFlags<Enum>(rhs);
}

template <class Enum, Enum... AllValues>
struct MergeEnumFlags;

template <class Enum>
struct MergeEnumFlags<Enum>
{
    static constexpr EnumFlags<Enum> value = {};
};

template <class Enum, Enum... Values>
struct MergeEnumFlags
{
    static constexpr EnumFlags<Enum> value = (Values | ...);
};

} // namespace hyperion

#define HYP_MAKE_ENUM_FLAGS_PUBLIC(_enum)           \
    namespace hyperion {                            \
    template <>                                     \
    struct EnumFlagsDecl<_enum>                     \
    {                                               \
        static constexpr bool is_enum_flags = true; \
    };                                              \
    }

#define HYP_MAKE_ENUM_FLAGS(_enum)                  \
    template <>                                     \
    struct ::hyperion::EnumFlagsDecl<_enum>         \
    {                                               \
        static constexpr bool is_enum_flags = true; \
    };

#endif