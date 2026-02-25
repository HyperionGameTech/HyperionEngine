/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <Core/reflection/BoxedValue.hpp>
#include <Core/reflection/Method.hpp>

#include <scripting/ScriptableDelegate.hpp>

#include <Core/logging/Logger.hpp>
#include <Core/logging/LogChannels.hpp>

#include <dotnet/ManagedClass.hpp>

namespace Hyperion {

namespace functional {

HYP_API void LogScriptableDelegateError(const char* message, dotnet::ManagedObject* objectPtr)
{
    if (objectPtr)
    {
        HYP_LOG(Core, Error, "ScriptableDelegate: {} (Obj: {})", message, objectPtr->GetClass()->GetName());
    }
    else
    {
        HYP_LOG(Core, Error, "ScriptableDelegate: {}", message);
    }
}

void ScriptableDelegateHelper::InvokeMethod_Internal(BoxedValue* pOutBoxed, const Method* method, const Handle<ObjectBase>& target, Span<BoxedValue> argsBoxed)
{
    HYP_CORE_ASSERT(method != nullptr, "Method cannot be null");

    if (pOutBoxed)
    {
        *pOutBoxed = method->Invoke(argsBoxed);
    }
    else
    {
        (void)method->Invoke(argsBoxed);
    }
}

} // namespace functional

} // namespace Hyperion