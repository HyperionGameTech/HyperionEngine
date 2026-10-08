/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <WebGPUPch.hpp>

#include <Rendering/WebGPU/WebGPUGpuBuffer.hpp>
#include <Rendering/WebGPU/WebGPUCommandBuffer.hpp>
#include <Rendering/WebGPU/WebGPURenderInterface.hpp>
#include <Rendering/WebGPU/WebGPUHelpers.hpp>

#include <WebGPUGpuBuffer.generated.inl>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(RenderingBackend);

extern WebGPURenderInterface RI;

static constexpr uint64 BufferSizeAlignment = 4;

WebGPUGpuBuffer::WebGPUGpuBuffer(GpuBufferType type, size_t size, size_t alignment)
    : GpuBufferBase(type, size, alignment),
      m_buffer(nullptr),
      m_allocatedSize(0),
      m_shadowCopy(nullptr),
      m_dirtyBegin(0),
      m_dirtyEnd(0),
      m_isRegisteredDirty(false),
      m_isReadbackPending(false)
{
}

WebGPUGpuBuffer::~WebGPUGpuBuffer()
{
    DestroyBuffer();
}

void WebGPUGpuBuffer::DestroyBuffer()
{
    if (m_buffer == nullptr)
    {
        return;
    }

    WaitForReadback();

    RI.UnregisterDirtyBuffer(this);

    {
        Mutex::Guard guard(m_dirtyMutex);

        m_isRegisteredDirty = false;
        m_dirtyBegin = 0;
        m_dirtyEnd = 0;
    }

    wgpuBufferRelease(m_buffer);
    m_buffer = nullptr;

    if (m_shadowCopy != nullptr)
    {
        Memory::Free(m_shadowCopy);
        m_shadowCopy = nullptr;
    }

    m_allocatedSize = 0;
    m_resourceState = ResourceState::Undefined;
}

bool WebGPUGpuBuffer::HasShadowCopy() const
{
    return m_cpuAccessible || IsCpuWrittenBufferType(m_type);
}

RendererResult WebGPUGpuBuffer::Create()
{
    if (IsCreated())
    {
        return {};
    }

    if (m_size == 0)
    {
        return HYP_MAKE_ERROR(RendererError, "Cannot create a buffer of size zero");
    }

    m_allocatedSize = ByteUtil::AlignAs(uint64(m_size), BufferSizeAlignment);

    WGPUBufferDescriptor descriptor = WGPU_BUFFER_DESCRIPTOR_INIT;
    descriptor.usage = GetWGPUBufferUsage(m_type);
    descriptor.size = m_allocatedSize;
#ifdef HYP_RHI_DEBUG_NAMES
    if (m_debugName.IsValid())
    {
        descriptor.label = ToWGPUStringView(*m_debugName);
    }
#endif

    m_buffer = wgpuDeviceCreateBuffer(RI.GetDevice(), &descriptor);

    if (m_buffer == nullptr)
    {
        return HYP_MAKE_ERROR(RendererError, "Failed to create WebGPU buffer");
    }

    if (HasShadowCopy())
    {
        m_shadowCopy = static_cast<ubyte*>(Memory::Allocate(size_t(m_allocatedSize)));
        Memory::Zero(m_shadowCopy, size_t(m_allocatedSize));
    }

    m_resourceState = ResourceState::Common;

    return {};
}

bool WebGPUGpuBuffer::IsCreated() const
{
    return m_buffer != nullptr;
}

bool WebGPUGpuBuffer::IsCpuAccessible() const
{
    return m_shadowCopy != nullptr;
}

void WebGPUGpuBuffer::InsertBarrier(WebGPUCommandBuffer* commandBuffer, ResourceState newState) const
{
    m_resourceState = newState;
}

void WebGPUGpuBuffer::InsertBarrier(WebGPUCommandBuffer* commandBuffer, ResourceState newState, ShaderModuleType shaderType) const
{
    m_resourceState = newState;
}

void WebGPUGpuBuffer::CopyFrom(WebGPUCommandBuffer* commandBuffer, const WebGPUGpuBuffer* srcBuffer, uint32 count)
{
    CopyFrom(commandBuffer, srcBuffer, 0, 0, count);
}

void WebGPUGpuBuffer::CopyFrom(
    WebGPUCommandBuffer* commandBuffer,
    const WebGPUGpuBuffer* srcBuffer,
    uint32 srcOffset, uint32 dstOffset,
    uint32 count)
{
    if (!IsCreated() || srcBuffer == nullptr || !srcBuffer->IsCreated())
    {
        HYP_LOG(RenderingBackend, Warning, "Attempt to copy between buffers that have not been created");

        return;
    }

    const uint64 alignedCount = ByteUtil::AlignAs(uint64(count), BufferSizeAlignment);

    Assert(srcOffset % BufferSizeAlignment == 0 && dstOffset % BufferSizeAlignment == 0,
        "Buffer copy offsets must be multiples of 4 (src: {}, dst: {})", srcOffset, dstOffset);

    Assert(srcOffset + alignedCount <= srcBuffer->GetAllocatedSize() && dstOffset + alignedCount <= m_allocatedSize,
        "Buffer copy out of bounds");

    commandBuffer->EndActivePass();

    wgpuCommandEncoderCopyBufferToBuffer(
        commandBuffer->GetEncoder(),
        srcBuffer->GetWGPUBuffer(), srcOffset,
        m_buffer, dstOffset,
        alignedCount);

    if (m_type == GpuBufferType::ReadbackBuffer)
    {
        commandBuffer->AddPendingReadback(this);
    }
}

RendererResult WebGPUGpuBuffer::EnsureCapacity(size_t minimumSize, bool* outSizeChanged)
{
    return EnsureCapacity(minimumSize, 0, outSizeChanged);
}

RendererResult WebGPUGpuBuffer::EnsureCapacity(size_t minimumSize, size_t alignment, bool* outSizeChanged)
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

    const bool shouldCreate = IsCreated();

    if (shouldCreate)
    {
        DestroyBuffer();
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

void WebGPUGpuBuffer::MarkDirty(size_t offset, size_t count) const
{
    if (count == 0 || m_type == GpuBufferType::ReadbackBuffer)
    {
        return;
    }

    bool shouldRegister = false;

    {
        Mutex::Guard guard(m_dirtyMutex);

        if (m_dirtyEnd == m_dirtyBegin)
        {
            m_dirtyBegin = offset;
            m_dirtyEnd = offset + count;
        }
        else
        {
            m_dirtyBegin = MathUtil::Min(m_dirtyBegin, offset);
            m_dirtyEnd = MathUtil::Max(m_dirtyEnd, offset + count);
        }

        if (!m_isRegisteredDirty)
        {
            m_isRegisteredDirty = true;
            shouldRegister = true;
        }
    }

    if (shouldRegister)
    {
        RI.RegisterDirtyBuffer(const_cast<WebGPUGpuBuffer*>(this));
    }
}

void WebGPUGpuBuffer::UploadDirtyRange(WGPUQueue queue)
{
    size_t dirtyBegin;
    size_t dirtyEnd;

    {
        Mutex::Guard guard(m_dirtyMutex);

        dirtyBegin = m_dirtyBegin;
        dirtyEnd = m_dirtyEnd;

        m_dirtyBegin = 0;
        m_dirtyEnd = 0;
        m_isRegisteredDirty = false;
    }

    if (dirtyEnd == dirtyBegin || m_buffer == nullptr || m_shadowCopy == nullptr)
    {
        return;
    }

    const uint64 alignedBegin = uint64(dirtyBegin) & ~(BufferSizeAlignment - 1);
    const uint64 alignedEnd = MathUtil::Min(ByteUtil::AlignAs(uint64(dirtyEnd), BufferSizeAlignment), m_allocatedSize);

    wgpuQueueWriteBuffer(queue, m_buffer, alignedBegin, m_shadowCopy + alignedBegin, size_t(alignedEnd - alignedBegin));
}

void WebGPUGpuBuffer::Memset(size_t count, ubyte value)
{
    if (void* mappedPtr = Map())
    {
        AssertDebug(count <= m_size);

        Memory::Fill(mappedPtr, value, count);
    }
}

void WebGPUGpuBuffer::Copy(size_t count, const void* ptr)
{
    Copy(0, count, ptr);
}

void WebGPUGpuBuffer::Copy(size_t offset, size_t count, const void* ptr)
{
    if (m_shadowCopy == nullptr)
    {
        HYP_LOG(RenderingBackend, Warning, "Attempt to copy into a buffer that is not CPU accessible!");

        return;
    }

    AssertDebug(offset + count <= m_size);

    Memory::Copy(m_shadowCopy + offset, ptr, count);

    MarkDirty(offset, count);
}

void WebGPUGpuBuffer::Read(size_t count, void* outPtr) const
{
    Read(0, count, outPtr);
}

void WebGPUGpuBuffer::Read(size_t offset, size_t count, void* outPtr) const
{
    if (m_shadowCopy == nullptr)
    {
        HYP_LOG(RenderingBackend, Warning, "Attempt to read a buffer that is not CPU accessible!");

        return;
    }

    AssertDebug(offset + count <= m_size);

    WaitForReadback();

    Memory::Copy(outPtr, m_shadowCopy + offset, count);
}

void* WebGPUGpuBuffer::Map() const
{
    if (m_shadowCopy == nullptr)
    {
        HYP_LOG(RenderingBackend, Warning, "Attempt to map a buffer that is not CPU accessible!");

        return nullptr;
    }

    WaitForReadback();

    MarkDirty(0, m_size);

    return m_shadowCopy;
}

void WebGPUGpuBuffer::Unmap() const
{
}

void WebGPUGpuBuffer::Flush(size_t offset, size_t count)
{
    if (m_shadowCopy == nullptr)
    {
        return;
    }

    MarkDirty(offset, MathUtil::Min(count, m_size - MathUtil::Min(offset, m_size)));
}

void WebGPUGpuBuffer::BeginReadback()
{
    if (m_buffer == nullptr || m_type != GpuBufferType::ReadbackBuffer)
    {
        return;
    }

    Assert(!m_isReadbackPending.load(std::memory_order_acquire), "Readback buffer was copied into again before its previous readback completed");

    m_isReadbackPending.store(true, std::memory_order_release);

    WGPUBufferMapCallbackInfo callbackInfo = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
    callbackInfo.mode = WGPUCallbackMode_AllowProcessEvents;
    callbackInfo.callback = &WebGPUGpuBuffer::OnReadbackMapped;
    callbackInfo.userdata1 = this;

    wgpuBufferMapAsync(m_buffer, WGPUMapMode_Read, 0, size_t(m_allocatedSize), callbackInfo);
}

void WebGPUGpuBuffer::OnReadbackMapped(WGPUMapAsyncStatus status, WGPUStringView message, void* userdata1, void* userdata2)
{
    WebGPUGpuBuffer* buffer = static_cast<WebGPUGpuBuffer*>(userdata1);

    if (status == WGPUMapAsyncStatus_Success)
    {
        const void* mappedRange = wgpuBufferGetConstMappedRange(buffer->m_buffer, 0, size_t(buffer->m_allocatedSize));

        if (mappedRange != nullptr && buffer->m_shadowCopy != nullptr)
        {
            Memory::Copy(buffer->m_shadowCopy, mappedRange, size_t(buffer->m_allocatedSize));
        }

        wgpuBufferUnmap(buffer->m_buffer);
    }
    else
    {
        HYP_LOG(RenderingBackend, Error, "Buffer readback failed: {}", ToStringView(message));
    }

    buffer->m_isReadbackPending.store(false, std::memory_order_release);
}

void WebGPUGpuBuffer::WaitForReadback() const
{
    if (!m_isReadbackPending.load(std::memory_order_acquire))
    {
        return;
    }

    // not reached for readbacks made during a frame, the frame slot is held until they have landed
    RI.NoteBlockingReadbackWait();

    while (m_isReadbackPending.load(std::memory_order_acquire))
    {
        RI.ProcessEvents();
    }
}

#ifdef HYP_RHI_DEBUG_NAMES
void WebGPUGpuBuffer::SetDebugName(Name name)
{
    GpuBufferBase::SetDebugName(name);

    if (m_buffer != nullptr && name.IsValid())
    {
        wgpuBufferSetLabel(m_buffer, ToWGPUStringView(*name));
    }
}
#endif

} // namespace Hyperion
