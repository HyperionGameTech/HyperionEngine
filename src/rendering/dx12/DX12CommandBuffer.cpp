/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12CommandBuffer.hpp>

#include <DX12CommandBuffer.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(RenderingBackend);

extern DX12RenderBackend* g_renderBackend;

DX12CommandBuffer::DX12CommandBuffer()
{
}

DX12CommandBuffer::~DX12CommandBuffer()
{
}

RendererResult DX12CommandBuffer::Create()
{
    // @TODO
    HYP_LOG(RenderingBackend, Warning, "DX12CommandBuffer::Create() not implemented");

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
