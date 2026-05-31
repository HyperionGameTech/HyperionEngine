/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/Containers/FixedArray.hpp>
#include <Core/Containers/ArrayMap.hpp>

#include <Core/Functional/Delegate.hpp>

#include <Rendering/CommandRecorder.hpp>
#include <Rendering/RenderMemory.hpp>
#include <Rendering/RenderTypes.hpp>
#include <Rendering/RenderResult.hpp>
#include <Rendering/GpuBuffer.hpp>

#include <Core/Math/MathUtil.hpp>
#include <Core/Math/Extent.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

class AsyncComputeBase
{
public:
    HYP_DEF_POOL_NEW_DELETE(g_rhiPool);

    virtual ~AsyncComputeBase() = default;

    virtual bool IsSupported() const = 0;

    virtual bool CheckStatus() = 0;

    virtual void Create() = 0;

    CommandRecorder cr;
    uint32 lastFrame = uint32(-1);

    Delegate<void> OnCompleted;
};

} // namespace Hyperion

#ifndef INCLUDE_FROM_RHI
#define INCLUDE_FROM_RHI_BASE

#if HYP_VULKAN
#include <Rendering/Vulkan/VulkanAsyncCompute.hpp>
#elif HYP_DX12
#include <Rendering/DX12/DX12AsyncCompute.hpp>
#endif

#undef INCLUDE_FROM_RHI_BASE
#else
#undef INCLUDE_FROM_RHI
#endif
