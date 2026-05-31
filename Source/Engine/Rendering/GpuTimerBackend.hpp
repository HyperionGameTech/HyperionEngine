/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Types.hpp>
#include <Core/Defines.hpp>

#include <Core/Memory/Pool/Pool.hpp>

#include <Rendering/RenderMemory.hpp>

namespace Hyperion {

class DeviceBase;
class CommandBufferBase;
class EngineStatGpuTimer;

class GpuTimerBackendBase
{
public:
    HYP_DEF_POOL_NEW_DELETE(g_rhiPool);

    virtual ~GpuTimerBackendBase() = default;

    virtual bool Initialize(DeviceBase* device) = 0;
    virtual void Shutdown() = 0;

    virtual bool IsSupported() const = 0;
    virtual double GetTimestampPeriod() const = 0;

    void OnFrameStart()
    {
        m_timers.Clear();
    }

    void OnFrameEnd()
    {
    }

    virtual void WriteStartTimestamp(CommandBuffer* cmd, EngineStatGpuTimer* timer) = 0;
    virtual void WriteStopTimestamp(CommandBuffer* cmd, EngineStatGpuTimer* timer) = 0;

    virtual void ResolveFrameResults(uint32 completedFrameIndex) = 0;

protected:
    Array<EngineStatGpuTimer*, RHIAllocator> m_timers;
};

} // namespace Hyperion

#ifndef INCLUDE_FROM_RHI
#define INCLUDE_FROM_RHI_BASE

#if HYP_VULKAN
#include <Rendering/Vulkan/VulkanGpuTimerBackend.hpp>

namespace Hyperion
{
    using GpuTimerBackend = VulkanGpuTimerBackend;
} // namespace Hyperion

#elif HYP_DX12
#include <Rendering/DX12/DX12GpuTimerBackend.hpp>

namespace Hyperion
{
    using GpuTimerBackend = DX12GpuTimerBackend;
} // namespace Hyperion

#endif

#undef INCLUDE_FROM_RHI_BASE
#else
#undef INCLUDE_FROM_RHI
#endif
