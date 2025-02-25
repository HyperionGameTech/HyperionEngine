/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_STATIC_MESSAGE_HPP
#define HYPERION_STATIC_MESSAGE_HPP

#include <core/containers/StaticString.hpp>

#include <core/utilities/Format.hpp>

#include <core/Util.hpp>
#include <core/Defines.hpp>

#include <Types.hpp>

namespace hyperion {

#pragma region StaticMessage

template <auto Value>
struct StaticMessageInitializer
{
    static constexpr auto value = Value.Data();
};

struct StaticMessage
{
    ANSIStringView  value;

    constexpr StaticMessage()                                           = default;

    template <auto MessageStaticString>
    constexpr StaticMessage(ValueWrapper<MessageStaticString>)
        : value(StaticMessageInitializer<MessageStaticString>().value)
    {
    }

    constexpr StaticMessage(const StaticMessage &other)                 = default;
    constexpr StaticMessage &operator=(const StaticMessage &other)      = default;
    constexpr StaticMessage(StaticMessage &&other) noexcept             = default;
    constexpr StaticMessage &operator=(StaticMessage &&other) noexcept  = default;

    constexpr operator const ANSIStringView &() const
        { return value; }
};

template <auto Value>
inline const StaticMessage &MakeStaticMessage()
{
    static const StaticMessage value { ValueWrapper<Value>() };

    return value;
}

#define HYP_STATIC_MESSAGE(str) MakeStaticMessage<HYP_STATIC_STRING(str)>()

#pragma endregion StaticMessage

} // namespace hyperion

#endif
