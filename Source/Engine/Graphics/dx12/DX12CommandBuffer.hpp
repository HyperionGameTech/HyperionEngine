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
class DX12DescriptorSet;

struct DX12CachedDescriptorSetBinding
{
    static constexpr uint32 MaxDynamicEntries = 4;

    const DX12DescriptorSet* descriptorSet = nullptr;
    uint16 updateVersion = 0;
    uint32 dynamicEntryCount = 0;
    UINT64 dynamicEntryAddresses[MaxDynamicEntries];
};

HYP_CLASS(NoScriptBindings)
class DX12CommandBuffer final : public CommandBufferBase
{
    HYP_OBJECT_BODY(DX12CommandBuffer);

    friend class DX12DescriptorSet;

public:
    explicit DX12CommandBuffer(D3D12_COMMAND_LIST_TYPE type);

    DX12CommandBuffer(DX12CommandBuffer&& other) noexcept;
    DX12CommandBuffer& operator=(DX12CommandBuffer&& other) noexcept;

    ~DX12CommandBuffer();

    HYP_FORCE_INLINE D3D12_COMMAND_LIST_TYPE GetType() const
    {
        return m_type;
    }

    HYP_FORCE_INLINE ID3D12GraphicsCommandList* GetCommandList() const
    {
        return m_commandList.Get();
    }

    HYP_FORCE_INLINE ID3D12CommandAllocator* GetCommandAllocator() const
    {
        return m_allocator.Get();
    }

    bool IsCreated() const override;

    RendererResult Create() override;

    void SetDebugName(const wchar_t* name);

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

    void ResetBoundDescriptorSets()
    {
        m_boundDescriptorSets.Clear();
    }

    DX12GraphicsPipeline* m_boundGraphicsPipeline;

private:
    D3D12_COMMAND_LIST_TYPE m_type;
    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    ComPtr<ID3D12CommandAllocator> m_allocator;

    bool m_isRecording;

    // Track bound descriptor heaps to avoid redundant SetDescriptorHeaps() calls
    ID3D12DescriptorHeap* m_boundViewHeap;
    ID3D12DescriptorHeap* m_boundSamplerHeap;

    // Track bound descriptor sets to avoid redundant SetGraphicsRootDescriptorTable() calls
    Array<DX12CachedDescriptorSetBinding, DX12Allocator> m_boundDescriptorSets;

    ID3D12CommandSignature* m_indirectCommandSignature;
};

} // namespace Hyperion
