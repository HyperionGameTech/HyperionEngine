/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/functional/ScriptableDelegate.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/object/HypClass.hpp>

#ifdef HYP_SCRIPT
#include <script/HypScript.hpp>
#include <script/vm/Value.hpp>
#endif

namespace hyperion {
namespace functional {

#ifdef HYP_DOTNET
HYP_API void LogScriptableDelegateError(const char* message, dotnet::Object* pObj)
{
    if (pObj)
    {
        HYP_LOG(Script, Error, "ScriptableDelegate: {} (Obj: {})", message, pObj->GetClass()->GetName());
    }
    else
    {
        HYP_LOG(Script, Error, "ScriptableDelegate: {}", message);
    }
}
#endif

#ifdef HYP_SCRIPT
HYP_API void LogScriptableDelegateError(const char* message, const Script_Value& value)
{
    if (value.IsValid())
    {
        HYP_LOG(Script, Error, "ScriptableDelegate: {} (Obj: {})", message, value.ToString());
    }
    else
    {
        HYP_LOG(Script, Error, "ScriptableDelegate: {}", message);
    }
}
#endif

#if !defined(HYP_DOTNET) && !defined(HYP_SCRIPT)
// stub method
HYP_API void LogScriptableDelegateError(const char* message, void* /* unused */)
{
    HYP_LOG(Script, Error, "ScriptableDelegate: {}", message);
}
#endif

} // namespace functional
} // namespace hyperion
