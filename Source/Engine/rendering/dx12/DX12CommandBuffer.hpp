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
        return m_commandList.Get();
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

private:
    D3D12_COMMAND_LIST_TYPE m_type;
    ComPtr<ID3D12CommandAllocator> m_commandAllocator;
    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    bool m_isRecording;
};

} // namespace Hyperion
