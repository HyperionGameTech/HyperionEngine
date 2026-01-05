/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/reflection/ObjectBase.hpp>
#include <core/reflection/Handle.hpp>

namespace Hyperion {

HYP_CLASS(Abstract, NoScriptBindings)
class DeviceBase : public ObjectBase
{
    HYP_OBJECT_BODY(DeviceBase);

public:
    virtual ~DeviceBase() override = default;
};

} // namespace Hyperion

#ifndef INCLUDE_FROM_RHI
#define INCLUDE_FROM_RHI_BASE

#if HYP_VULKAN
#include <rendering/vulkan/VulkanDevice.hpp>
#endif

#undef INCLUDE_FROM_RHI_BASE
#else
#undef INCLUDE_FROM_RHI
#endif
