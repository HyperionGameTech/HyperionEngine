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
    UTF8StringView  message;
    ANSIStringView  current_function;
};

template <auto MessageString, auto CurrentFunctionString>
struct StaticMessageInitializer
{
    static constexpr auto message_string = MessageString.Data();
    static constexpr auto current_function_string = CurrentFunctionString.Data();

    static const StaticMessage &MakeStaticMessage()
    {
        static const StaticMessage static_message {
            message_string,
            current_function_string
        };

        return static_message;
    }
};

template <auto MessageString, auto CurrentFunctionString>
const StaticMessage &MakeStaticMessage()
{
    return StaticMessageInitializer<MessageString, CurrentFunctionString>::MakeStaticMessage();
}

#define HYP_STATIC_MESSAGE(str) MakeStaticMessage<HYP_STATIC_STRING(str), HYP_STATIC_STRING(HYP_PRETTY_FUNCTION_NAME)>()

#pragma endregion StaticMessage

} // namespace hyperion

#endif
