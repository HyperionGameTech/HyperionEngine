/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12GpuBuffer.hpp>
#include <rendering/dx12/DX12GpuImage.hpp>
#include <rendering/dx12/DX12CommandBuffer.hpp>
#include <rendering/dx12/DX12RenderInterface.hpp>

#include <DX12CommandBuffer.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(RenderingBackend);

extern DX12RenderInterface* g_renderInterface;

DX12CommandBuffer::DX12CommandBuffer(D3D12_COMMAND_LIST_TYPE type)
    : m_type(type)
{
}

DX12CommandBuffer::~DX12CommandBuffer()
{
}

bool DX12CommandBuffer::IsCreated() const
{
    return false; // @TODO
}

RendererResult DX12CommandBuffer::Create()
{
    ID3D12Device* device = g_renderInterface->GetDevice();

    const DX12QueueData* queueData = g_renderInterface->GetQueueData(m_type);
    Assert(queueData != nullptr);

    ID3D12CommandAllocator* allocator = queueData->commandAllocators[0].Get();
    Assert(allocator != nullptr);

    HRESULT res = device->CreateCommandList(0, m_type, allocator, nullptr, IID_PPV_ARGS(&m_commandList));
    if (!SUCCEEDED(res))
        return HYP_MAKE_ERROR(RendererError, "Failed to create command list!", res);

    m_commandList->Close();

    return {};
}

void DX12CommandBuffer::BindVertexBuffer(const DX12GpuBuffer* buffer)
{
    // @TODO
    HYP_LOG(RenderingBackend, Warning, "DX12CommandBuffer::BindVertexBuffer() not implemented");
}

void DX12CommandBuffer::BindIndexBuffer(const DX12GpuBuffer* buffer, GpuElemType elemType)
{
    // @TODO
    HYP_LOG(RenderingBackend, Warning, "DX12CommandBuffer::BindIndexBuffer() not implemented");
}

void DX12CommandBuffer::DrawIndexed(
    uint32 numIndices,
    uint32 numInstances,
    uint32 instanceIndex) const
{
    // @TODO
    HYP_LOG(RenderingBackend, Warning, "DX12CommandBuffer::DrawIndexed() not implemented");
}

void DX12CommandBuffer::DrawIndexedIndirect(
    const DX12GpuBuffer* buffer,
    uint32 bufferOffset) const
{
    // @TODO
    HYP_LOG(RenderingBackend, Warning, "DX12CommandBuffer::DrawIndexedIndirect() not implemented");
}

} // namespace Hyperion
