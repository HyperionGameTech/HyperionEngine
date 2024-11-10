/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/system/Debug.hpp>

#include <core/Name.hpp>

#include <Types.hpp>
#include <Constants.hpp>

#include <type_traits>

using namespace hyperion;

extern "C" {

static_assert(sizeof(Name) == 8, "Name size mismatch, ensure C# implementation matches C++");
static_assert(std::is_standard_layout_v<Name>, "Name is not standard layout");

HYP_EXPORT void Name_FromString(const char *str, bool weak, Name *out_name)
{
    if (!out_name) {
        return;
    }

    if (!str) {
        *out_name = Name::Invalid();

        return;
    }

    if (weak) {
        *out_name = Name(CreateWeakNameFromDynamicString(str).hash_code);
    } else {
        *out_name = Name(CreateNameFromDynamicString(str).hash_code);
    }
}

HYP_EXPORT const char *Name_LookupString(const Name *name)
{
    static const char invalid_name_string[] = "";

    if (!name) {
        return invalid_name_string;
    }

    return name->LookupString();
}

} // extern "C"