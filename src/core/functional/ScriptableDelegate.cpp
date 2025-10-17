/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/functional/ScriptableDelegate.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

// needed by dotnet/ManagedClass.hpp
#include <core/reflection/HypData.hpp>

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

} // namespace functional

} // namespace hyperion