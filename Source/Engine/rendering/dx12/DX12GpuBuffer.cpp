/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12GpuBuffer.hpp>
#include <rendering/dx12/DX12RenderInterface.hpp>
#include <rendering/dx12/DX12Helpers.hpp>

#include <DX12GpuBuffer.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(RenderingBackend);

extern DX12RenderInterface* g_renderInterface;

static D3D12_HEAP_TYPE GetHeapType(GpuBufferType bufferType, bool requireCpuAccessible)
{
    if (bufferType == GpuBufferType::STAGING_BUFFER)
    {
        return D3D12_HEAP_TYPE_UPLOAD;
    }

    if (requireCpuAccessible)
    {
        return D3D12_HEAP_TYPE_UPLOAD;
    }

    return D3D12_HEAP_TYPE_DEFAULT;
}

DX12GpuBuffer::DX12GpuBuffer(GpuBufferType type, SizeType size, SizeType alignment)
    : GpuBufferBase(type, size, alignment)
{
}

DX12GpuBuffer::~DX12GpuBuffer()
{
}

RendererResult DX12GpuBuffer::Create()
{
    D3D12MA::Allocator* allocator = g_renderInterface->GetAllocator();
    AssertDebug(allocator != nullptr);

    D3D12_HEAP_TYPE heapType = GetHeapType(m_type, m_requireCpuAccessible);

    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;

    switch (m_type)
    {
        case GpuBufferType::STORAGE_BUFFER:                 // fallthrough
        case GpuBufferType::ATOMIC_COUNTER:                 // fallthrough
        case GpuBufferType::SCRATCH_BUFFER:                 // fallthrough
        case GpuBufferType::ACCELERATION_STRUCTURE_BUFFER:  // fallthrough
            flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            break;
        case GpuBufferType::CONSTANT_BUFFER: // fallthrough
        default:
            break;
    }

    UINT64 finalSize = m_size;
    if (m_type == GpuBufferType::CONSTANT_BUFFER)
    {
        finalSize = ByteUtil::AlignAs(m_size, 256);
    }

    D3D12_RESOURCE_STATES finalState = ToDX12ResourceStates(m_resourceState);

    if (heapType == D3D12_HEAP_TYPE_UPLOAD)
    {
        finalState = D3D12_RESOURCE_STATE_GENERIC_READ;
    }
    else if (m_type == GpuBufferType::ACCELERATION_STRUCTURE_BUFFER)
    {
        finalState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
    }

    D3D12_RESOURCE_DESC bufferDesc {};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Alignment = 0;
    bufferDesc.Width = finalSize;
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.SampleDesc.Quality = 0;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    bufferDesc.Flags = flags;

    D3D12MA::ALLOCATION_DESC allocDesc {};
    allocDesc.HeapType = heapType;
    
    HRESULT hr = allocator->CreateResource(
        &allocDesc,
        &bufferDesc,
        finalState,
        nullptr,
        &m_allocation,
        __uuidof(ID3D12Resource), 
        (void**)&m_resource
    );

    if (FAILED(hr))
    {
        return HYP_MAKE_ERROR(RendererError, "Failed to create D3D12MA buffer", hr);
    }

    if (m_debugName && m_resource)
    {
        m_resource->SetName(*WideString(*m_debugName));
        m_allocation->SetName(*WideString(*m_debugName));
    }

    return {};
}

bool DX12GpuBuffer::IsCreated() const
{
    return m_resource != nullptr;
}

bool DX12GpuBuffer::IsCpuAccessible() const
{
    D3D12_HEAP_PROPERTIES heapProperties;
    m_resource->GetHeapProperties(&heapProperties, nullptr);

    return heapProperties.Type == D3D12_HEAP_TYPE_UPLOAD || heapProperties.Type == D3D12_HEAP_TYPE_READBACK;
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
    if (m_type == GpuBufferType::CONSTANT_BUFFER)
    {
        minimumSize = ByteUtil::AlignAs(minimumSize, 256);
    }

    // @TODO

    return {};
}

RendererResult DX12GpuBuffer::EnsureCapacity(
    SizeType minimumSize,
    SizeType alignment,
    bool* outSizeChanged)
{
    if (m_type == GpuBufferType::CONSTANT_BUFFER)
    {
        minimumSize = ByteUtil::AlignAs(minimumSize, 256);
    }

    // @TODO

    return {};
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

void* DX12GpuBuffer::Map() const
{
    // @TODO
    HYP_LOG(RenderingBackend, Warning, "DX12GpuBuffer::Map() not implemented");

    return nullptr;
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
}
#endif

} // namespace Hyperion
