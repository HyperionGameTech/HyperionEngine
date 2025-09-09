/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/debug/Debug.hpp>

#include <core/utilities/Format.hpp>

#include <core/Types.hpp>

using namespace hyperion;

extern "C"
{
    HYP_EXPORT void Assert_Throw(const char* message, const char* funcName, uint32 line)
    {
        if (!message)
        {
            message = "<no message>";
        }

        if (funcName != nullptr)
        {
            HYP_FAIL("{}:{}: Assertion failed!\n\t{}", funcName, line, message);
        }
        else
        {
            HYP_FAIL("Assertion failed!\n\t{}", message);
        }
    }

} // extern "C"
