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

#include <Core/Constants.hpp>
#include <Core/Containers/FixedArray.hpp>

#include <Rendering/DX12/DX12Shared.hpp>

namespace Hyperion {

class DX12CommandBuffer;
class EngineStatGpuTimer;

class DX12GpuTimerBackend final : public GpuTimerBackendBase
{
public:
    DX12GpuTimerBackend();
    ~DX12GpuTimerBackend() override;

    bool Initialize(DeviceBase* device) override;
    void Shutdown() override;

    bool IsSupported() const override;
    double GetTimestampPeriod() const override;

    void WriteStartTimestamp(DX12CommandBuffer* cmd, EngineStatGpuTimer* timer) override;
    void WriteStopTimestamp(DX12CommandBuffer* cmd, EngineStatGpuTimer* timer) override;

    void ResolveFrameResults(uint32 completedFrameIndex) override;

private:
    double ComputeDeltaMs(uint64 start, uint64 end) const;

    uint32 GetOrCreateQuerySlot(EngineStatGpuTimer* timer);

    void EnsureResolveRecorded(DX12CommandBuffer* cmd, uint32 frameIndex);

    struct PerFrameState
    {
        ComPtr<ID3D12QueryHeap> queryHeap;
        ComPtr<ID3D12Resource> readbackBuffer;
        bool resultsPending = false;
        bool resolveRecorded = false;
        uint32 timerCount = 0;
    };

    FixedArray<PerFrameState, NumFramesInFlight> m_frames;
    uint64 m_timestampFrequency = 0;
    double m_timestampPeriod = 0.0;
    bool m_isSupported = false;
};

} // namespace Hyperion
