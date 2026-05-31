/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Core/debug/Debug.hpp>

#include <Core/name/Name.hpp>

#include <type_traits>

using namespace Hyperion;

extern "C"
{

    static_assert(sizeof(Name) == 8, "Name size mismatch, ensure C# implementation matches C++");
    static_assert(std::is_standard_layout_v<Name>, "Name is not standard layout");

    HYP_EXPORT void Name_FromString(const char* pString, bool weak, Name* pOutName)
    {
        if (!pOutName)
        {
            return;
        }

        if (!pString)
        {
            *pOutName = Name::Invalid();

            return;
        }

        if (weak)
        {
            *pOutName = Name(CreateStringHashFromDynamicString(pString).hashCode);
        }
        else
        {
            *pOutName = Name(CreateNameFromDynamicString(pString).hashCode);
        }
    }

    HYP_EXPORT const char* Name_LookupString(const Name* pName)
    {
        static const char s_invalidNameString[] = "";

        if (!pName)
        {
            return s_invalidNameString;
        }

        return pName->LookupString();
    }

} // extern "C"
