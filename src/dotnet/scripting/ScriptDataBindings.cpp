/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <scripting/Script.hpp>

using namespace Hyperion;

extern "C"
{
    HYP_EXPORT ScriptData* ScriptData_AllocateNativeObject(ScriptData* pScriptData)
    {
        if (!pScriptData)
        {
            return nullptr;
        }

        return new ScriptData(*pScriptData);
    }

    HYP_EXPORT void ScriptData_FreeNativeObject(ScriptData* pScriptData)
    {
        if (!pScriptData)
        {
            return;
        }

        delete pScriptData;
    }
} // extern "C"
