/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <DX12Pch.hpp>

#include <Rendering/DX12/DX12GpuBuffer.hpp>
#include <Rendering/DX12/DX12RenderInterface.hpp>
#include <Rendering/DX12/DX12Helpers.hpp>

#include <DX12GpuBuffer.generated.inl>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(RenderingBackend);

extern DX12RenderInterface RI;

static D3D12_HEAP_TYPE GetHeapType(GpuBufferType bufferType, bool cpuAccessible)
{
    switch (bufferType)
    {
    case GpuBufferType::StagingBuffer:  // fallthrough
    case GpuBufferType::ConstantBuffer:
        return D3D12_HEAP_TYPE_UPLOAD;
    case GpuBufferType::ReadbackBuffer:
        return D3D12_HEAP_TYPE_READBACK;
    default:
        break;
    }

    if (cpuAccessible)
    {
        return D3D12_HEAP_TYPE_UPLOAD;
    }

    return D3D12_HEAP_TYPE_DEFAULT;
}

DX12GpuBuffer::DX12GpuBuffer(GpuBufferType type, size_t size, size_t alignment)
    : GpuBufferBase(type, size, alignment),
      m_mapping(nullptr)
{
}

DX12GpuBuffer::DX12GpuBuffer(DX12GpuBuffer&& other) noexcept
    : GpuBufferBase(other.m_type, other.m_size, other.m_alignment),
      m_resource(other.m_resource),
      m_allocation(other.m_allocation),
      m_mapping(other.m_mapping)
{
    other.m_resource.Reset();
    other.m_allocation.Reset();
    other.m_mapping = nullptr;
    other.m_resourceState = RS_UNDEFINED;
}

DX12GpuBuffer& DX12GpuBuffer::operator=(DX12GpuBuffer&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    if (IsCreated())
    {
        if (m_mapping != nullptr)
        {
            Unmap();
        }

        m_resource.Reset();
        m_allocation.Reset();
        m_resourceState = RS_UNDEFINED;
    }

    m_type = other.m_type;
    m_size = other.m_size;
    m_alignment = other.m_alignment;
    m_resourceState = other.m_resourceState;
    m_resource = other.m_resource;
    m_allocation = other.m_allocation;
    m_mapping = other.m_mapping;

    other.m_resource.Reset();
    other.m_allocation.Reset();
    other.m_mapping = nullptr;
    other.m_resourceState = RS_UNDEFINED;

    return *this;
}

DX12GpuBuffer::~DX12GpuBuffer()
{
    if (!IsCreated())
    {
        return;
    }

    if (m_mapping != nullptr)
    {
        Unmap();
    }

    m_resource.Reset();
    m_allocation.Reset();
    m_resourceState = RS_UNDEFINED;
}

RendererResult DX12GpuBuffer::Create()
{
    if (IsCreated())
    {
        return {};
    }

    D3D12MA::Allocator* allocator = RI.GetAllocator();
    AssertDebug(allocator != nullptr);

    D3D12_HEAP_TYPE heapType = GetHeapType(m_type, m_cpuAccessible);

    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;

    // Buffers on UPLOAD or READBACK heaps cannot have ALLOW_UNORDERED_ACCESS or ALLOW_RENDER_TARGET flags
    const bool canHazUAV = (heapType != D3D12_HEAP_TYPE_UPLOAD && heapType != D3D12_HEAP_TYPE_READBACK);

    switch (m_type)
    {
        case GpuBufferType::RWStructuredBuffer:             // fallthrough
        case GpuBufferType::RWByteAddressBuffer:            // fallthrough
        case GpuBufferType::ScratchBuffer:                 // fallthrough
        case GpuBufferType::AccelerationStructureBuffer:   // fallthrough
        case GpuBufferType::IndirectArgsBuffer:
            if (canHazUAV)
            {
                flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            }
            break;
        case GpuBufferType::ConstantBuffer: // fallthrough
        default:
            break;
    }

    UINT64 finalSize = m_size;

    if (m_type == GpuBufferType::ConstantBuffer)
    {
        finalSize = ByteUtil::AlignAs(m_size, 256);
    }

    D3D12_RESOURCE_STATES finalState = ToDX12ResourceStates(m_resourceState);

    if (heapType == D3D12_HEAP_TYPE_UPLOAD)
    {
        finalState = D3D12_RESOURCE_STATE_GENERIC_READ;
    }
    else if (m_type == GpuBufferType::AccelerationStructureBuffer)
    {
        finalState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
    }

    D3D12_RESOURCE_DESC bufferDesc {};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Alignment = (m_alignment > 0) ? D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT : 0;
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

    m_resourceState = finalState == D3D12_RESOURCE_STATE_GENERIC_READ
        ? RS_READ_GENERIC
        : RS_COMMON;

#if HYP_DEBUG_MODE
    if (m_debugName && m_resource)
    {
        m_resource->SetName(*WideString(*m_debugName));
        m_allocation->SetName(*WideString(*m_debugName));
    }
#endif

    return {};
}

bool DX12GpuBuffer::IsCreated() const
{
    return m_resource != nullptr;
}

bool DX12GpuBuffer::IsCpuAccessible() const
{
    AssertDebug(IsCreated());

    if (HYP_UNLIKELY(!IsCreated()))
    {
        return false;
    }

    D3D12_HEAP_PROPERTIES heapProperties;
    m_resource->GetHeapProperties(&heapProperties, nullptr);

    return heapProperties.Type == D3D12_HEAP_TYPE_UPLOAD
        || heapProperties.Type == D3D12_HEAP_TYPE_READBACK;
}

void DX12GpuBuffer::InsertBarrier(DX12CommandBuffer* commandBuffer, ResourceState newState) const
{
    if (!IsCreated())
    {
        HYP_LOG(RenderingBackend, Warning, "Attempt to insert a resource barrier but buffer was not created");
        return;
    }

    // Resources on UPLOAD/READBACK heaps are implicitly pinned to GENERIC_READ /
    // COPY_DEST respectively and must not be transitioned.  Only update the
    // logical state tracker so that downstream code that reads m_resourceState
    // stays consistent, but do not emit a D3D12 barrier.
    const D3D12_HEAP_TYPE heapType = GetHeapType(m_type, m_cpuAccessible);
    if (heapType == D3D12_HEAP_TYPE_UPLOAD || heapType == D3D12_HEAP_TYPE_READBACK)
    {
        m_resourceState = newState;
        return;
    }

    D3D12_RESOURCE_STATES srcState = ToDX12ResourceStates(m_resourceState);
    D3D12_RESOURCE_STATES dstState = ToDX12ResourceStates(newState);

    if (srcState == dstState)
    {
        return;
    }

    D3D12_RESOURCE_BARRIER barrier {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = m_resource.Get();
    barrier.Transition.StateBefore = srcState;
    barrier.Transition.StateAfter = dstState;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    commandBuffer->GetCommandList()->ResourceBarrier(1, &barrier);

    m_resourceState = newState;
}

void DX12GpuBuffer::InsertBarrier(DX12CommandBuffer* commandBuffer, ResourceState newState, ShaderModuleType shaderType) const
{
    InsertBarrier(commandBuffer, newState);
}

void DX12GpuBuffer::CopyFrom(
    DX12CommandBuffer* commandBuffer,
    const DX12GpuBuffer* srcBuffer,
    uint32 count)
{
    if (!IsCreated())
    {
        HYP_LOG(RenderingBackend, Warning, "Attempt to copy from buffer but dst buffer was not created");
        return;
    }

    if (!srcBuffer->IsCreated())
    {
        HYP_LOG(RenderingBackend, Warning, "Attempt to copy from buffer but src buffer was not created");
        return;
    }

    Assert(count <= Size(), "Copy count exceeds destination buffer size!");

    commandBuffer->GetCommandList()->CopyBufferRegion(
        m_resource.Get(),
        0,
        srcBuffer->GetResource(),
        0,
        count);
}

void DX12GpuBuffer::CopyFrom(
    DX12CommandBuffer* commandBuffer,
    const DX12GpuBuffer* srcBuffer,
    uint32 srcOffset, uint32 dstOffset,
    uint32 count)
{
    if (!IsCreated())
    {
        HYP_LOG(RenderingBackend, Warning, "Attempt to copy from buffer but dst buffer was not created");
        return;
    }

    if (!srcBuffer->IsCreated())
    {
        HYP_LOG(RenderingBackend, Warning, "Attempt to copy from buffer but src buffer was not created");
        return;
    }

    Assert((srcOffset + count <= srcBuffer->Size()) && (dstOffset + count <= Size()), "Copy out of bounds!");

    commandBuffer->GetCommandList()->CopyBufferRegion(
        m_resource.Get(),
        dstOffset,
        srcBuffer->GetResource(),
        srcOffset,
        count);
}

RendererResult DX12GpuBuffer::EnsureCapacity(
    size_t minimumSize,
    size_t alignment,
    bool* outSizeChanged)
{
    if (minimumSize == 0)
    {
        return {};
    }

    if (minimumSize <= m_size)
    {
        if (outSizeChanged != nullptr)
        {
            *outSizeChanged = false;
        }

        return {};
    }

    bool shouldCreate = IsCreated();

    if (shouldCreate)
    {
        m_resource.Reset();
        m_allocation.Reset();
        m_resourceState = RS_UNDEFINED;
    }

    m_size = minimumSize;
    m_alignment = alignment;

    if (outSizeChanged != nullptr)
    {
        *outSizeChanged = true;
    }

    if (shouldCreate)
    {
        CheckResultOrReturn(Create());
    }

    return {};
}

RendererResult DX12GpuBuffer::EnsureCapacity(
    size_t minimumSize,
    bool* outSizeChanged)
{
    return EnsureCapacity(minimumSize, 0, outSizeChanged);
}

void DX12GpuBuffer::Memset(size_t count, ubyte value)
{
    void* ptr = Map();
    if (ptr != nullptr)
    {
        Memory::Fill(ptr, value, count);
    }
}

void DX12GpuBuffer::Copy(size_t count, const void* ptr)
{
    void* mappedPtr = Map();
    if (mappedPtr != nullptr)
    {
        Memory::Copy(mappedPtr, ptr, count);
    }
}

void DX12GpuBuffer::Copy(size_t offset, size_t count, const void* ptr)
{
    void* mappedPtr = Map();
    if (mappedPtr != nullptr)
    {
        AssertDebug(offset + count <= m_size);
        Memory::Copy(reinterpret_cast<void*>(UIntPtr(mappedPtr) + offset), ptr, count);
    }
}

void DX12GpuBuffer::Read(size_t count, void* outPtr) const
{
    const void* ptr = Map();
    if (ptr != nullptr)
    {
        AssertDebug(count <= m_size);
        Memory::Copy(outPtr, ptr, count);
    }
}

void DX12GpuBuffer::Read(size_t offset, size_t count, void* outPtr) const
{
    const void* ptr = Map();
    if (ptr != nullptr)
    {
        AssertDebug(offset + count <= m_size);
        Memory::Copy(outPtr, reinterpret_cast<void*>(UIntPtr(ptr) + UIntPtr(offset)), count);
    }
}

void* DX12GpuBuffer::Map() const
{
    if (m_mapping != nullptr)
    {
        return m_mapping;
    }

    if (!IsCpuAccessible())
    {
        HYP_LOG(RenderingBackend, Warning, "Attempt to map a buffer that is not CPU accessible!");
        return nullptr;
    }

    D3D12_RANGE range {};
    range.Begin = 0;
    range.End = m_size;

    void* ptr;
    HRESULT hr = m_resource->Map(0, &range, &ptr);
    if (FAILED(hr))
    {
        HYP_LOG(RenderingBackend, Warning, "Failed to map buffer: {}", hr);
        return nullptr;
    }

    m_mapping = ptr;
    return ptr;
}

void DX12GpuBuffer::Unmap() const
{
    if (m_mapping == nullptr)
    {
        return;
    }

    D3D12_RANGE range {};
    range.Begin = 0;
    range.End = 0;

    m_resource->Unmap(0, &range);
    m_mapping = nullptr;
}

uint64 DX12GpuBuffer::GetBufferDeviceAddress() const
{
    Assert(IsCreated(), "Attempt to get buffer device address but buffer was not created");

    D3D12_GPU_VIRTUAL_ADDRESS address = m_resource->GetGPUVirtualAddress();
    return address;
}

void DX12GpuBuffer::Flush(size_t offset, size_t count)
{
    /* no-op in D3D */
}

#ifdef HYP_DEBUG_MODE
void DX12GpuBuffer::SetDebugName(Name name)
{
    GpuBufferBase::SetDebugName(name);

    if (!name.IsValid())
    {
        return;
    }

    WideString ws = *name;

    if (m_resource)
    {
        m_resource->SetName(ws.Data());
    }

    if (m_allocation)
    {
        m_allocation->SetName(ws.Data());
    }
}
#endif

} // namespace Hyperion
