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

#include <Rendering/RenderTypes.hpp>

namespace Hyperion {

class DX12GpuTimerBackend final : public GpuTimerBackendBase
{
public:
    DX12GpuTimerBackend() = default;
    ~DX12GpuTimerBackend() override = default;

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

    void WriteStartTimestamp(DX12CommandBuffer* cmd, EngineStatGpuTimer* timer) override
    {
    }

    void WriteStopTimestamp(DX12CommandBuffer* cmd, EngineStatGpuTimer* timer) override
    {
    }

    void ResolveFrameResults(uint32 completedFrameIndex) override
    {
    }
};

} // namespace Hyperion
