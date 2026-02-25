/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Defines.hpp>

#include <Core/containers/FixedArray.hpp>
#include <Core/containers/ArrayMap.hpp>

#include <Core/functional/Delegate.hpp>

#include <rendering/RenderQueue.hpp>
#include <rendering/RenderMemory.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/RenderResult.hpp>
#include <rendering/GpuBuffer.hpp>

#include <Core/math/MathUtil.hpp>
#include <Core/math/Extent.hpp>

#include <Core/Types.hpp>

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
