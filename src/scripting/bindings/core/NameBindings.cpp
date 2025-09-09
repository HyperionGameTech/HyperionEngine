/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/debug/Debug.hpp>

#include <core/Name.hpp>

#include <core/Types.hpp>
#include <core/Constants.hpp>

#include <type_traits>

using namespace hyperion;

extern "C"
{

    static_assert(sizeof(Name) == 8, "Name size mismatch, ensure C# implementation matches C++");
    static_assert(std::is_standard_layout_v<Name>, "Name is not standard layout");

    HYP_EXPORT void Name_FromString(const char* str, bool weak, Name* outName)
    {
        if (!outName)
        {
            return;
        }

        if (!str)
        {
            *outName = Name::Invalid();

            return;
        }

        if (weak)
        {
            *outName = Name(CreateWeakNameFromDynamicString(str).hashCode);
        }
        else
        {
            *outName = Name(CreateNameFromDynamicString(str).hashCode);
        }
    }

    HYP_EXPORT const char* Name_LookupString(const Name* name)
    {
        static const char invalidNameString[] = "";

        if (!name)
        {
            return invalidNameString;
        }

        return name->LookupString();
    }

} // extern "C"