/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/CommandBuffer.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

namespace Hyperion {

class DX12GraphicsPipeline;

HYP_CLASS(NoScriptBindings)
class DX12CommandBuffer final : public CommandBufferBase
{
    HYP_OBJECT_BODY(DX12CommandBuffer);

public:
    explicit DX12CommandBuffer(D3D12_COMMAND_LIST_TYPE type);
    ~DX12CommandBuffer();

    HYP_FORCE_INLINE D3D12_COMMAND_LIST_TYPE GetType() const
    {
        return m_type;
    }

    HYP_FORCE_INLINE ID3D12GraphicsCommandList* GetCommandList() const
    {
        return m_commandLists[m_currentCommandListIndex].Get();
    }

    bool IsCreated() const override;

    RendererResult Create() override;

    bool IsRecording() const override
    {
        return m_isRecording;
    }

    void Begin() override;
    void End() override;

    void BindVertexBuffer(const DX12GpuBuffer* buffer) override;
    void BindIndexBuffer(const DX12GpuBuffer* buffer, GpuElemType elemType = GET_UNSIGNED_INT) override;

    void DrawIndexed(
        uint32 numIndices,
        uint32 numInstances = 1,
        uint32 instanceIndex = 0) const override;

    void DrawIndexedIndirect(
        const DX12GpuBuffer* buffer,
        uint32 bufferOffset) const override;

    void Submit(
        ID3D12CommandQueue* commandQueue,
        ID3D12Fence* fence = nullptr,
        uint64 fenceValue = 0);

    HYP_FORCE_INLINE ID3D12DescriptorHeap* GetBoundViewHeap() const
    {
        return m_boundViewHeap;
    }

    HYP_FORCE_INLINE ID3D12DescriptorHeap* GetBoundSamplerHeap() const
    {
        return m_boundSamplerHeap;
    }

    HYP_FORCE_INLINE void SetBoundDescriptorHeaps(ID3D12DescriptorHeap* viewHeap, ID3D12DescriptorHeap* samplerHeap)
    {
        m_boundViewHeap = viewHeap;
        m_boundSamplerHeap = samplerHeap;
    }

    void ResetBoundDescriptorHeaps()
    {
        m_boundViewHeap = nullptr;
        m_boundSamplerHeap = nullptr;
    }

    DX12GraphicsPipeline* m_boundGraphicsPipeline;

private:
    D3D12_COMMAND_LIST_TYPE m_type;
    FixedArray<ComPtr<ID3D12CommandAllocator>, NumFramesInFlight> m_commandAllocators;
    FixedArray<ComPtr<ID3D12GraphicsCommandList>, NumFramesInFlight> m_commandLists;
    bool m_isRecording;
    uint32 m_currentAllocatorIndex;
    uint32 m_currentCommandListIndex;

    // Track bound descriptor heaps to avoid redundant SetDescriptorHeaps() calls
    ID3D12DescriptorHeap* m_boundViewHeap;
    ID3D12DescriptorHeap* m_boundSamplerHeap;
};

} // namespace Hyperion
