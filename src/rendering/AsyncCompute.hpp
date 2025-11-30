/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/containers/FixedArray.hpp>
#include <core/containers/ArrayMap.hpp>

#include <rendering/RenderQueue.hpp>
#include <rendering/RenderMemory.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/RenderResult.hpp>
#include <rendering/RenderGpuBuffer.hpp>

#include <core/math/MathUtil.hpp>
#include <core/math/Extent.hpp>

#include <core/Types.hpp>

namespace hyperion {

class AsyncComputeBase
{
public:
    virtual ~AsyncComputeBase() = default;

    virtual bool IsSupported() const = 0;

    RenderQueue renderQueue;

    HYP_DEF_POOL_NEW_DELETE(g_renderPool);
};

} // namespace hyperion

#ifndef INCLUDE_FROM_RHI
#define INCLUDE_FROM_RHI_BASE

#ifdef HYP_VULKAN
#include <rendering/vulkan/VulkanAsyncCompute.hpp>
#endif

#undef INCLUDE_FROM_RHI_BASE
#else
#undef INCLUDE_FROM_RHI
#endif
