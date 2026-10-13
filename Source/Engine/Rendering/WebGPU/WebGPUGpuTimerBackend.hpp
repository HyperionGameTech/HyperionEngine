/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <Rendering/GpuTimerBackend.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <Rendering/WebGPU/WebGPUShared.hpp>

namespace Hyperion {

class WebGPUCommandBuffer;
class EngineStatGpuTimer;

// Timestamp queries are an optional WebGPU feature that browsers quantize heavily, so GPU timers are left out for now
class WebGPUGpuTimerBackend final : public GpuTimerBackendBase
{
public:
    WebGPUGpuTimerBackend() = default;
    ~WebGPUGpuTimerBackend() override = default;

    bool Initialize(DeviceBase* device) override
    {
        return false;
    }

    void Shutdown() override
    {
    }

    bool IsSupported() const override
    {
        return false;
    }

    double GetTimestampPeriod() const override
    {
        return 0.0;
    }

    void WriteStartTimestamp(WebGPUCommandBuffer* cmd, EngineStatGpuTimer* timer) override
    {
    }

    void WriteStopTimestamp(WebGPUCommandBuffer* cmd, EngineStatGpuTimer* timer) override
    {
    }

    void ResolveFrameResults(uint32 completedFrameIndex) override
    {
    }
};

} // namespace Hyperion
