/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/reflection/BoxedValue.hpp>
#include <core/reflection/Method.hpp>

#include <core/functional/ScriptableDelegate.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <dotnet/ManagedClass.hpp>

namespace hyperion {

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

void ScriptableDelegateHelper::InvokeMethod_Internal(BoxedValue* pOutBoxed, const Method* method, const Handle<ObjectBase>& target, Span<BoxedValue> argsHypData)
{
    HYP_CORE_ASSERT(method != nullptr, "Method cannot be null");

    if (pOutBoxed)
    {
        *pOutBoxed = method->Invoke(argsHypData);
    }
    else
    {
        (void)method->Invoke(argsHypData);
    }
}

} // namespace functional

} // namespace hyperion