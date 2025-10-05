/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/functional/ScriptableDelegate.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

// needed by dotnet/Class.hpp
#include <core/object/HypData.hpp>

#include <dotnet/Class.hpp>

namespace hyperion {

namespace functional {

HYP_API void LogScriptableDelegateError(const char* message, dotnet::Object* objectPtr)
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