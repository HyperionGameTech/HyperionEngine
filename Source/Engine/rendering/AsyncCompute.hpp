/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/containers/FixedArray.hpp>
#include <core/containers/ArrayMap.hpp>

#include <core/functional/Delegate.hpp>

#include <rendering/RenderQueue.hpp>
#include <rendering/RenderMemory.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/RenderResult.hpp>
#include <rendering/GpuBuffer.hpp>

#include <core/math/MathUtil.hpp>
#include <core/math/Extent.hpp>

#include <core/Types.hpp>

namespace Hyperion {

class AsyncComputeBase
{
public:
    HYP_DEF_POOL_NEW_DELETE(g_renderPool);

    virtual ~AsyncComputeBase() = default;

    virtual bool IsSupported() const = 0;

    virtual bool CheckStatus() = 0;

    virtual void Create() = 0;

    RenderQueue renderQueue;
    uint32 lastFrame = uint32(-1);

    Delegate<void> OnCompleted;
};

} // namespace Hyperion

#ifndef INCLUDE_FROM_RHI
#define INCLUDE_FROM_RHI_BASE

#if HYP_VULKAN
#include <rendering/vulkan/VulkanAsyncCompute.hpp>
#elif HYP_DX12
#include <rendering/dx12/DX12AsyncCompute.hpp>
#endif

#undef INCLUDE_FROM_RHI_BASE
#else
#undef INCLUDE_FROM_RHI
#endif
