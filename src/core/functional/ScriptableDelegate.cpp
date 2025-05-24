/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/functional/ScriptableDelegate.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <dotnet/Class.hpp>

namespace hyperion {

namespace functional {

HYP_API void LogScriptableDelegateError(const char *message, dotnet::Object *object_ptr)
{
    if (object_ptr) {
        HYP_LOG(DotNET, Error, "ScriptableDelegate: {} (Obj: {})", message, object_ptr->GetClass()->GetName());
    } else {
        HYP_LOG(DotNET, Error, "ScriptableDelegate: {}", message);
    }
}

} // namespace functional

} // namespace hyperion