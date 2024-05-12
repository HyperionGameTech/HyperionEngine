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

    UnderlyingType  value { };

    constexpr EnumFlags()                               = default;

    constexpr EnumFlags(EnumType value)
        : value(static_cast<UnderlyingType>(value))
    {
    }

    constexpr EnumFlags(UnderlyingType value)
        : value(value)
    {
    }

    constexpr EnumFlags(const EnumFlags &other)         = default;
    EnumFlags &operator=(const EnumFlags &other)        = default;
    constexpr EnumFlags(EnumFlags &&other) noexcept     = default;
    EnumFlags &operator=(EnumFlags &&other) noexcept    = default;
    ~EnumFlags()                                        = default;

    [[nodiscard]]
    HYP_FORCE_INLINE
    constexpr operator EnumType() const
        { return static_cast<EnumType>(value); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    constexpr operator UnderlyingType() const
        { return value; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    constexpr bool operator==(const EnumFlags &other) const
        { return value == other.value; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    constexpr bool operator==(Enum rhs) const
        { return *this == EnumFlags<Enum>(rhs); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    constexpr bool operator!=(const EnumFlags &other) const
        { return value != other.value; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    constexpr bool operator!=(Enum rhs) const
        { return *this != EnumFlags<Enum>(rhs); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    constexpr EnumFlags operator~() const
        { return EnumFlags(~value); }

    [[nodiscard]]
    HYP_FORCE_INLINE
    constexpr EnumFlags operator|(const EnumFlags &other) const
        { return EnumFlags(value | other.value); }

    HYP_FORCE_INLINE
    EnumFlags &operator|=(const EnumFlags &other)
    {
        value |= other.value;

        return *this;
    }

    [[nodiscard]]
    HYP_FORCE_INLINE
    constexpr EnumFlags operator|(EnumType flag) const
        { return EnumFlags(value | static_cast<UnderlyingType>(flag)); }

    HYP_FORCE_INLINE
    EnumFlags &operator|=(EnumType flag)
    {
        value |= static_cast<UnderlyingType>(flag);

        return *this;
    }

    [[nodiscard]]
    HYP_FORCE_INLINE
    constexpr EnumFlags operator&(const EnumFlags &other) const
        { return EnumFlags(value & other.value); }

    HYP_FORCE_INLINE
    EnumFlags &operator&=(const EnumFlags &other)
    {
        value &= other.value;

        return *this;
    }

    [[nodiscard]]
    HYP_FORCE_INLINE
    constexpr EnumFlags operator&(EnumType flag) const
        { return EnumFlags(value & static_cast<UnderlyingType>(flag)); }

    HYP_FORCE_INLINE
    EnumFlags &operator&=(EnumType flag)
    {
        value &= static_cast<UnderlyingType>(flag);

        return *this;
    }

    [[nodiscard]]
    HYP_FORCE_INLINE
    constexpr EnumFlags operator^(const EnumFlags &other) const
        { return EnumFlags(value ^ other.value); }

    HYP_FORCE_INLINE
    EnumFlags &operator^=(const EnumFlags &other)
    {
        value ^= other.value;

        return *this;
    }

    [[nodiscard]]
    HYP_FORCE_INLINE
    constexpr EnumFlags operator^(EnumType flag) const
        { return EnumFlags(value ^ static_cast<UnderlyingType>(flag)); }

    HYP_FORCE_INLINE
    EnumFlags &operator^=(EnumType flag)
    {
        value ^= static_cast<UnderlyingType>(flag);

        return *this;
    }

    [[nodiscard]]
    HYP_FORCE_INLINE
    constexpr bool operator[](EnumType flag) const
        { return (value & static_cast<UnderlyingType>(flag)) != 0; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    constexpr HashCode GetHashCode() const
        { return HashCode::GetHashCode(value); }
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
EnumFlags<Enum> &operator|=(EnumFlags<Enum> lhs, Enum rhs)
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
EnumFlags<Enum> &operator&=(EnumFlags<Enum> lhs, Enum rhs)
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
EnumFlags<Enum> &operator^=(EnumFlags<Enum> lhs, Enum rhs)
{
    return lhs ^= EnumFlags<Enum>(rhs);
}

} // namespace hyperion

#define HYP_MAKE_ENUM_FLAGS_PUBLIC(_enum) \
    namespace hyperion { \
        template <> \
        struct EnumFlagsDecl<_enum> \
        { \
            static constexpr bool is_enum_flags = true; \
        }; \
    }

#define HYP_MAKE_ENUM_FLAGS(_enum) \
    template <> \
    struct EnumFlagsDecl<_enum> \
    { \
        static constexpr bool is_enum_flags = true; \
    };

#endif