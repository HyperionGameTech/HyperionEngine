/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scripting/Script.hpp>

using namespace hyperion;

extern "C"
{
    HYP_EXPORT ManagedScript* ManagedScript_AllocateNativeObject(ManagedScript* inManagedScript)
    {
        if (!inManagedScript)
        {
            return nullptr;
        }

        return new ManagedScript(*inManagedScript);
    }

    HYP_EXPORT void ManagedScript_FreeNativeObject(ManagedScript* inManagedScript)
    {
        if (!inManagedScript)
        {
            return;
        }

        delete inManagedScript;
    }
} // extern "C"