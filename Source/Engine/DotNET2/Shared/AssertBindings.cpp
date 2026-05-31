/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Core/Debug/Debug.hpp>

#include <Core/Utilities/Format.hpp>

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
