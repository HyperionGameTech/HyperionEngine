/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <scripting/Script.hpp>

using namespace hyperion;

extern "C"
{
    HYP_EXPORT ScriptData* ScriptData_AllocateNativeObject(ScriptData* inScriptData)
    {
        if (!inScriptData)
        {
            return nullptr;
        }

        return new ScriptData(*inScriptData);
    }

    HYP_EXPORT void ScriptData_FreeNativeObject(ScriptData* inScriptData)
    {
        if (!inScriptData)
        {
            return;
        }

        delete inScriptData;
    }
} // extern "C"
