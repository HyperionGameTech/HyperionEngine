/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <core/debug/Debug.hpp>

#include <core/utilities/Format.hpp>

using namespace Hyperion;

extern "C"
{
    HYP_EXPORT void Assert_Throw(const char* pMessage, const char* pFuncName, uint32 line)
    {
        if (!pMessage)
        {
            pMessage = "<no message>";
        }

        if (pFuncName != nullptr)
        {
            HYP_FAIL("{}:{}: Assertion failed!\n\t{}", pFuncName, line, pMessage);
        }
        else
        {
            HYP_FAIL("Assertion failed!\n\t{}", pMessage);
        }
    }

} // extern "C"
