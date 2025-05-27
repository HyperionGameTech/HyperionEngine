/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scripting/Script.hpp>

using namespace hyperion;

extern "C"
{

    HYP_EXPORT ManagedScript* ManagedScript_AllocateNativeObject(ManagedScript* in_managed_script)
    {
        if (!in_managed_script)
        {
            return nullptr;
        }

        return new ManagedScript(*in_managed_script);
    }

    HYP_EXPORT void ManagedScript_FreeNativeObject(ManagedScript* in_managed_script)
    {
        if (!in_managed_script)
        {
            return;
        }

        delete in_managed_script;
    }

} // extern "C"