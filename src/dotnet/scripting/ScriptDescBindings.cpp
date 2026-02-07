/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <scripting/Script.hpp>

using namespace Hyperion;

extern "C"
{
    HYP_EXPORT ScriptDesc* ScriptData_AllocateNativeObject(ScriptDesc* pScriptData)
    {
        if (!pScriptData)
        {
            return nullptr;
        }

        return new ScriptDesc(*pScriptData);
    }

    HYP_EXPORT void ScriptData_FreeNativeObject(ScriptDesc* pScriptData)
    {
        if (!pScriptData)
        {
            return;
        }

        delete pScriptData;
    }
} // extern "C"
