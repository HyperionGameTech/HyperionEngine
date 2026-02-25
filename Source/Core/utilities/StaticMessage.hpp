/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/containers/StaticString.hpp>

#include <Core/utilities/Format.hpp>

#include <Core/Util.hpp>
#include <Core/Defines.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

#pragma region StaticMessage

template <auto Value>
struct StaticMessageInitializer
{
    static constexpr auto value = Value.Data();
};

struct StaticMessage
{
    ANSIStringView value;

    constexpr StaticMessage() = default;

    template <auto MessageStaticString>
    constexpr StaticMessage(ValueWrapper<MessageStaticString>)
        : value(StaticMessageInitializer<MessageStaticString>().value)
    {
    }

    constexpr StaticMessage(const StaticMessage& other) = default;
    constexpr StaticMessage& operator=(const StaticMessage& other) = default;
    constexpr StaticMessage(StaticMessage&& other) noexcept = default;
    constexpr StaticMessage& operator=(StaticMessage&& other) noexcept = default;

    constexpr operator const ANSIStringView&() const
    {
        return value;
    }
};

template <auto Value>
inline const StaticMessage& MakeStaticMessage()
{
    static const StaticMessage value { ValueWrapper<Value>() };

    return value;
}

#define HYP_STATIC_MESSAGE(str) MakeStaticMessage<HYP_STATIC_STRING(str)>()

#pragma endregion StaticMessage

} // namespace Hyperion
