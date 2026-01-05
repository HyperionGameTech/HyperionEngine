/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12GpuBuffer.hpp>

#include <DX12GpuBuffer.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(RenderingBackend);

extern DX12RenderBackend* g_renderBackend;

DX12GpuBuffer::DX12GpuBuffer(GpuBufferType type, SizeType size, SizeType alignment)
    : GpuBufferBase(type, size, alignment)
{
}

DX12GpuBuffer::~DX12GpuBuffer()
{
}

RendererResult DX12GpuBuffer::Create()
{
    // @TODO
    HYP_LOG(RenderingBackend, Warning, "DX12GpuBuffer::Create() not implemented");

    HYPERION_RETURN_OK;
}

bool DX12GpuBuffer::IsCreated() const
{
    // @TODO
    HYP_LOG(RenderingBackend, Warning, "DX12GpuBuffer::IsCreated() not implemented");

    return false;
}

bool DX12GpuBuffer::IsCpuAccessible() const
{
    // @TODO
    HYP_LOG(RenderingBackend, Warning, "DX12GpuBuffer::IsCpuAccessible() not implemented");

    return false;
}

void DX12GpuBuffer::InsertBarrier(DX12CommandBuffer* commandBuffer, ResourceState newState) const
{
    // @TODO
    HYP_LOG(RenderingBackend, Warning, "DX12GpuBuffer::InsertBarrier() not implemented");
}

void DX12GpuBuffer::InsertBarrier(DX12CommandBuffer* commandBuffer, ResourceState newState, ShaderModuleType shaderType) const
{
    // @TODO
    HYP_LOG(RenderingBackend, Warning, "DX12GpuBuffer::InsertBarrier() not implemented");
}

void DX12GpuBuffer::CopyFrom(
    DX12CommandBuffer* commandBuffer,
    const DX12GpuBuffer* srcBuffer,
    uint32 count)
{
    // @TODO
    HYP_LOG(RenderingBackend, Warning, "DX12GpuBuffer::CopyFrom() not implemented");
}

void DX12GpuBuffer::CopyFrom(
    DX12CommandBuffer* commandBuffer,
    const DX12GpuBuffer* srcBuffer,
    uint32 srcOffset, uint32 dstOffset,
    uint32 count)
{
    // @TODO
    HYP_LOG(RenderingBackend, Warning, "DX12GpuBuffer::CopyFrom() not implemented");
}

RendererResult DX12GpuBuffer::EnsureCapacity(
    SizeType minimumSize,
    bool* outSizeChanged)
{
    // @TODO
    HYP_LOG(RenderingBackend, Warning, "DX12GpuBuffer::EnsureCapacity() not implemented");

    HYPERION_RETURN_OK;
}

RendererResult DX12GpuBuffer::EnsureCapacity(
    SizeType minimumSize,
    SizeType alignment,
    bool* outSizeChanged)
{
    // @TODO
    HYP_LOG(RenderingBackend, Warning, "DX12GpuBuffer::EnsureCapacity() not implemented");

    HYPERION_RETURN_OK;
}

void DX12GpuBuffer::Memset(SizeType count, ubyte value)
{
    // @TODO
    HYP_LOG(RenderingBackend, Warning, "DX12GpuBuffer::Memset() not implemented");
}

void DX12GpuBuffer::Copy(SizeType count, const void* ptr)
{
    // @TODO
    HYP_LOG(RenderingBackend, Warning, "DX12GpuBuffer::Copy() not implemented");
}

void DX12GpuBuffer::Copy(SizeType offset, SizeType count, const void* ptr)
{
    // @TODO
    HYP_LOG(RenderingBackend, Warning, "DX12GpuBuffer::Copy() not implemented");
}

void DX12GpuBuffer::Read(SizeType count, void* outPtr) const
{
    // @TODO
    HYP_LOG(RenderingBackend, Warning, "DX12GpuBuffer::Read() not implemented");
}

void DX12GpuBuffer::Read(SizeType offset, SizeType count, void* outPtr) const
{
    // @TODO
    HYP_LOG(RenderingBackend, Warning, "DX12GpuBuffer::Read() not implemented");
}

void DX12GpuBuffer::Map() const
{
    // @TODO
    HYP_LOG(RenderingBackend, Warning, "DX12GpuBuffer::Map() not implemented");
}

void DX12GpuBuffer::Unmap() const
{
    // @TODO
    HYP_LOG(RenderingBackend, Warning, "DX12GpuBuffer::Unmap() not implemented");
}

void DX12GpuBuffer::Flush(SizeType offset, SizeType count)
{
    // @TODO
    HYP_LOG(RenderingBackend, Warning, "DX12GpuBuffer::Flush() not implemented");
}

#ifdef HYP_DEBUG_MODE
void DX12GpuBuffer::SetDebugName(Name name)
{
    GpuBufferBase::SetDebugName(name);

    // @TODO
    HYP_LOG(RenderingBackend, Warning, "DX12GpuBuffer::SetDebugName() not implemented");
}
#endif

} // namespace Hyperion
