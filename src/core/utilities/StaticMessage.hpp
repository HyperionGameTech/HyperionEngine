/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_STATIC_MESSAGE_HPP
#define HYPERION_STATIC_MESSAGE_HPP

#include <core/containers/StaticString.hpp>

#include <core/utilities/Format.hpp>

#include <core/Defines.hpp>

#include <Types.hpp>

namespace hyperion {

#pragma region StaticMessage

struct StaticMessage
{
    ANSIStringView  value;
};

template <auto Value>
struct StaticMessageInitializer
{
    static constexpr auto value_string = Value.Data();

    static const StaticMessage &MakeStaticMessage()
    {
        static const StaticMessage static_message {
            value_string
        };

        return static_message;
    }
};

template <auto Value>
const StaticMessage &MakeStaticMessage()
{
    return StaticMessageInitializer<Value>::MakeStaticMessage();
}

#define HYP_STATIC_MESSAGE(str) MakeStaticMessage<HYP_STATIC_STRING(str)>()

#pragma endregion StaticMessage

} // namespace hyperion

#endif
