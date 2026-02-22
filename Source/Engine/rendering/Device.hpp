/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <Core/Defines.hpp>

#include <Core/reflection/ObjectBase.hpp>
#include <Core/reflection/Handle.hpp>

#include <rendering/RenderMemory.hpp>

namespace Hyperion {

HYP_CLASS(Abstract, NoScriptBindings)
class DeviceBase : public ObjectBase
{
    HYP_OBJECT_BODY(DeviceBase);

public:
    static Pool* GetAllocator() { return g_rhiPool; }
    
    virtual ~DeviceBase() override = default;
};

} // namespace Hyperion

#ifndef INCLUDE_FROM_RHI
#define INCLUDE_FROM_RHI_BASE

#if HYP_VULKAN
#include <rendering/vulkan/VulkanDevice.hpp>
#elif HYP_DX12
#include <rendering/dx12/DX12Device.hpp>
#endif

#undef INCLUDE_FROM_RHI_BASE
#else
#undef INCLUDE_FROM_RHI
#endif
