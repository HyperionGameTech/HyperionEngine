/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <HyperionPch.hpp>

#include <Core/debug/Debug.hpp>

#include <Core/utilities/Format.hpp>

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
