/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/reflection/HypData.hpp>
#include <core/reflection/HypMethod.hpp>

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
        HYP_LOG(DotNET, Error, "ScriptableDelegate: {} (Obj: {})", message, objectPtr->GetClass()->GetName());
    }
    else
    {
        HYP_LOG(DotNET, Error, "ScriptableDelegate: {}", message);
    }
}

void ScriptableDelegateHelper::InvokeHypMethod_Internal(HypData* outReturnHypData, const HypMethod* method, const Handle<HypObjectBase>& target, Span<HypData> argsHypData)
{
    HYP_CORE_ASSERT(method != nullptr, "Method cannot be null");

    if (outReturnHypData)
    {
        *outReturnHypData = method->Invoke(argsHypData);
    }
    else
    {
        (void)method->Invoke(argsHypData);
    }
}

} // namespace functional

} // namespace hyperion