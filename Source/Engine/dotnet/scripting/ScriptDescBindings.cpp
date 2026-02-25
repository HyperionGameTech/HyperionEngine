/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#include <HyperionPch.hpp>

#include <scripting/Script.hpp>

using namespace Hyperion;

extern "C"
{
    HYP_EXPORT ScriptDesc* ScriptDesc_AllocateNativeObject(ScriptDesc* scriptDesc)
    {
        if (!scriptDesc)
        {
            return nullptr;
        }

        return new ScriptDesc(*scriptDesc);
    }

    HYP_EXPORT void ScriptDesc_FreeNativeObject(ScriptDesc* scriptDesc)
    {
        if (!scriptDesc)
        {
            return;
        }

        delete scriptDesc;
    }
} // extern "C"
