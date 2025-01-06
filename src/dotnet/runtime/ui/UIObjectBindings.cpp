/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <ui/UIObject.hpp>

#include <Types.hpp>

using namespace hyperion;

extern "C" {

HYP_EXPORT const char *UIEventHandlerResult_GetMessage(UIEventHandlerResult *result)
{
    AssertThrow(result != nullptr);

    if (Optional<UTF8StringView> message = result->GetMessage()) {
        return message->Data();
    }

    return nullptr;
}

HYP_EXPORT const char *UIEventHandlerResult_GetFunctionName(UIEventHandlerResult *result)
{
    AssertThrow(result != nullptr);

    if (Optional<ANSIStringView> function_name = result->GetFunctionName()) {
        return function_name->Data();
    }

    return nullptr;
}

} // extern "C"