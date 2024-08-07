/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <dotnet/runtime/ManagedHandle.hpp>

#include <scene/camera/Camera.hpp>

#include <core/utilities/TypeID.hpp>

#include <Engine.hpp>
#include <Types.hpp>

using namespace hyperion;

extern "C" {
HYP_EXPORT uint32 Camera_GetTypeID()
{
    return TypeID::ForType<Camera>().Value();
}

HYP_EXPORT void Camera_Create(ManagedHandle *handle)
{
    *handle = CreateManagedHandleFromHandle(CreateObject<Camera>());
}

} // extern "C"