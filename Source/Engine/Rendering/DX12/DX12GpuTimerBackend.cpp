/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <DX12Pch.hpp>

#include "DX12GpuTimerBackend.hpp"
#include "DX12CommandBuffer.hpp"
#include "DX12RenderInterface.hpp"

#include <Framework/EngineStats.hpp>
#include <Framework/EngineGlobals.hpp>

#include <Rendering/RenderInterface.hpp>

namespace Hyperion {

DX12GpuTimerBackend::DX12GpuTimerBackend()
    : GpuTimerBackendBase(),
      m_frames { }
{
}

DX12GpuTimerBackend::~DX12GpuTimerBackend()
{
    Shutdown();
}

bool DX12GpuTimerBackend::Initialize(DeviceBase* device)
{
    ID3D12Device* dxDevice = RI.GetDevice();

    if (!dxDevice)
    {
        return false;
    }

    const DX12QueueData* queueData = RI.GetQueueData(D3D12_COMMAND_LIST_TYPE_DIRECT);

    if (!queueData || !queueData->commandQueue)
    {
        return false;
    }

    queueData->commandQueue->GetTimestampFrequency(&m_timestampFrequency);

    if (m_timestampFrequency == 0)
    {
        return false;
    }

    m_timestampPeriod = 1.0 / double(m_timestampFrequency);

    D3D12_QUERY_HEAP_DESC queryHeapDesc {};
    queryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    queryHeapDesc.Count = MaxGpuTimestampQueriesPerFrame;
    queryHeapDesc.NodeMask = 0;

    for (uint32 i = 0; i < NumFramesInFlight; i++)
    {
        PerFrameState& frameState = m_frames[i];

        HRESULT hr = dxDevice->CreateQueryHeap(&queryHeapDesc, IID_PPV_ARGS(&frameState.queryHeap));

        if (FAILED(hr))
        {
            Shutdown();
            return false;
        }

        D3D12_HEAP_PROPERTIES heapProps {};
        heapProps.Type = D3D12_HEAP_TYPE_READBACK;
        heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

        D3D12_RESOURCE_DESC bufDesc {};
        bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufDesc.Alignment = 0;
        bufDesc.Width = MaxGpuTimestampQueriesPerFrame * sizeof(uint64);
        bufDesc.Height = 1;
        bufDesc.DepthOrArraySize = 1;
        bufDesc.MipLevels = 1;
        bufDesc.Format = DXGI_FORMAT_UNKNOWN;
        bufDesc.SampleDesc.Count = 1;
        bufDesc.SampleDesc.Quality = 0;
        bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        bufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        hr = dxDevice->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &bufDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&frameState.readbackBuffer));

        if (FAILED(hr))
        {
            Shutdown();
            return false;
        }
    }

    m_isSupported = true;
    return true;
}

void DX12GpuTimerBackend::Shutdown()
{
    for (uint32 i = 0; i < NumFramesInFlight; i++)
    {
        PerFrameState& frameState = m_frames[i];
        frameState.queryHeap.Reset();
        frameState.readbackBuffer.Reset();
        frameState.resultsPending = false;
        frameState.resolveRecorded = false;
        frameState.timerCount = 0;
    }

    m_timers.Clear();

    m_isSupported = false;
}

bool DX12GpuTimerBackend::IsSupported() const
{
    return m_isSupported;
}

double DX12GpuTimerBackend::GetTimestampPeriod() const
{
    return m_timestampPeriod;
}

uint32 DX12GpuTimerBackend::GetOrCreateQuerySlot(EngineStatGpuTimer* timer)
{
    if (timer->querySlotIndex < MaxGpuTimers)
    {
        return timer->querySlotIndex;
    }

    if (uint32(m_timers.Size()) >= MaxGpuTimers)
    {
        return UINT32_MAX;
    }

    m_timers.PushBack(timer);

    const uint32 slot = uint32(m_timers.Size() - 1);

    timer->querySlotIndex = slot;

    return slot;
}

void DX12GpuTimerBackend::EnsureResolveRecorded(DX12CommandBuffer* cmd, uint32 frameIndex)
{
    const uint32 prevFrameIndex = (frameIndex + NumFramesInFlight - 1) % NumFramesInFlight;

    PerFrameState& prevFrameState = m_frames[prevFrameIndex];

    if (prevFrameState.resultsPending && !prevFrameState.resolveRecorded)
    {
        const uint32 queryCount = prevFrameState.timerCount * 2;

        if (queryCount == 0)
        {
            return;
        }

        cmd->GetCommandList()->ResolveQueryData(
            prevFrameState.queryHeap.Get(),
            D3D12_QUERY_TYPE_TIMESTAMP,
            0,
            queryCount,
            prevFrameState.readbackBuffer.Get(),
            0);

        prevFrameState.resolveRecorded = true;
    }
}

void DX12GpuTimerBackend::WriteStartTimestamp(DX12CommandBuffer* cmd, EngineStatGpuTimer* timer)
{
    if (!m_isSupported || !cmd || !timer)
    {
        return;
    }

    const uint32 frameIndex = GetFrameCounter() % NumFramesInFlight;

    EnsureResolveRecorded(cmd, frameIndex);

    PerFrameState& frameState = m_frames[frameIndex];

    if (!frameState.resultsPending)
    {
        frameState.resultsPending = true;
        frameState.timerCount = 0;
    }

    const uint32 slot = GetOrCreateQuerySlot(timer);

    if (slot >= MaxGpuTimers)
    {
        return;
    }

    cmd->GetCommandList()->EndQuery(frameState.queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 2);

    frameState.timerCount = MathUtil::Max(frameState.timerCount, slot + 1);
}

void DX12GpuTimerBackend::WriteStopTimestamp(DX12CommandBuffer* cmd, EngineStatGpuTimer* timer)
{
    if (!m_isSupported || !cmd || !timer)
    {
        return;
    }

    const uint32 frameIndex = GetFrameCounter() % NumFramesInFlight;

    EnsureResolveRecorded(cmd, frameIndex);

    PerFrameState& frameState = m_frames[frameIndex];

    if (!frameState.resultsPending)
    {
        frameState.resultsPending = true;
        frameState.timerCount = 0;
    }

    const uint32 slot = GetOrCreateQuerySlot(timer);

    if (slot >= MaxGpuTimers)
    {
        return;
    }

    cmd->GetCommandList()->EndQuery(frameState.queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * 2 + 1);

    frameState.timerCount = MathUtil::Max(frameState.timerCount, slot + 1);

    timer->querySlotIndex = UINT32_MAX;
}

double DX12GpuTimerBackend::ComputeDeltaMs(uint64 start, uint64 end) const
{
    if (end <= start)
    {
        return 0.0;
    }

    return double(end - start) * m_timestampPeriod * 1000.0;
}

void DX12GpuTimerBackend::ResolveFrameResults(uint32 completedFrameIndex)
{
    if (!m_isSupported)
    {
        return;
    }

    PerFrameState& frameState = m_frames[completedFrameIndex];

    if (!frameState.resultsPending || !frameState.resolveRecorded)
    {
        return;
    }

    void* mappedData = nullptr;

    D3D12_RANGE readRange { 0, MaxGpuTimestampQueriesPerFrame * sizeof(uint64) };

    HRESULT hr = frameState.readbackBuffer->Map(0, &readRange, &mappedData);

    if (FAILED(hr))
    {
        return;
    }

    uint64* timestamps = static_cast<uint64*>(mappedData);

    for (uint32 i = 0; i < uint32(m_timers.Size()); i++)
    {
        EngineStatGpuTimer* timer = m_timers[i];

        if (!timer)
        {
            continue;
        }

        const uint32 startIndex = i * 2;
        const uint32 endIndex = i * 2 + 1;

        const uint64 startTimestamp = timestamps[startIndex];
        const uint64 endTimestamp = timestamps[endIndex];

        if (startTimestamp != 0 && endTimestamp != 0)
        {
            const double elapsedMs = ComputeDeltaMs(startTimestamp, endTimestamp);

            if (elapsedMs > 0.0)
            {
                timer->RecordElapsedMs(static_cast<float>(elapsedMs));
            }
        }
    }

    Memory::Zero(mappedData, MaxGpuTimestampQueriesPerFrame * sizeof(uint64));

    D3D12_RANGE writeRange { 0, 0 };
    frameState.readbackBuffer->Unmap(0, &writeRange);

    frameState.resultsPending = false;
    frameState.resolveRecorded = false;
}

} // namespace Hyperion
