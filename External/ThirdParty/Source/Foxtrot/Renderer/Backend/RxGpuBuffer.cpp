#include "RxGpuBuffer.hpp"

#include <Renderer/RxGlobals.hpp>
#include <Renderer/RxRenderBackend.hpp>

void RxRawGpuBuffer::Create(RxGpuBufferType buffer_type, uint64 size_in_bytes, VmaMemoryUsage memory_usage,
                            RxGpuBufferFlags buffer_flags)
{
    FxAssert(size_in_bytes > 0);

    Size = size_in_bytes;
    Type = buffer_type;
    mBufferFlags = buffer_flags;

    VmaAllocationCreateFlags vma_create_flags = 0;

    if ((mBufferFlags & RxGpuBufferFlags::ePersistentMapped)) {
        vma_create_flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }

    VkBufferUsageFlags usage_flags = RxGpuBufferUtil::BufferTypeToUnderlying(buffer_type);

    if (buffer_flags & RxGpuBufferFlags::eTransferReceiver) {
        usage_flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }

    const VkBufferCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .flags = 0,
        .size = size_in_bytes,
        .usage = usage_flags,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    const VmaAllocationCreateInfo alloc_create_info = { .flags = vma_create_flags, .usage = memory_usage };

    VmaAllocationInfo allocation_info;
    const VkResult status = vmaCreateBuffer(gRenderer->GpuAllocator, &create_info, &alloc_create_info, &Buffer,
                                            &Allocation, &allocation_info);

    if (status != VK_SUCCESS) {
        FxPanicVulkan("GPUBuffer", "Error allocating GPU buffer!", status);
    }


    // FxLogInfo("Create Buffer  (Buffer={:p}, Allocation={:p}, Size={})", reinterpret_cast<void*>(Buffer),
    //           reinterpret_cast<void*>(Allocation), Size);


    if ((mBufferFlags & RxGpuBufferFlags::ePersistentMapped) != 0) {
        // Get the pointer from VMA for the mapped GPU buffer
        pMappedBuffer = allocation_info.pMappedData;
    }

    Initialized = true;
}

void RxRawGpuBuffer::Map()
{
    if (IsMapped()) {
        FxLogWarning("Buffer {:p} is already mapped!", reinterpret_cast<void*>(Buffer));
        return;
    }

    const VkResult status = vmaMapMemory(gRenderer->GpuAllocator, Allocation, &pMappedBuffer);

    if (status != VK_SUCCESS) {
        FxLogError("Could not map GPU memory! (BufferType=0x{:x}, Error={})", static_cast<uint32>(Type),
                   RxUtil::ResultToStr(status));
        return;
    }
}


void RxRawGpuBuffer::UnMap()
{
    if (!IsMapped()) {
        return;
    }

    vmaUnmapMemory(gRenderer->GpuAllocator, Allocation);
    pMappedBuffer = nullptr;
}

void RxRawGpuBuffer::Destroy()
{
    if (!(Initialized.load()) || Allocation == nullptr || Buffer == nullptr) {
        return;
    }

    gRenderer->AddGpuBufferToDeletionQueue(Buffer, Allocation);

    Initialized.store(false);
    Size = 0;

    Allocation = nullptr;
    Buffer = nullptr;
}

void RxRawGpuBuffer::Upload(void* data, uint64 size)
{
    FxDebugAssert(size > 0);
    FxDebugAssert(data != nullptr);

    FxAssertMsg(this->Size >= size, "GPU buffer is smaller than source buffer!");

    Map();
    memcpy(pMappedBuffer, data, size);
    UnMap();
}

/////////////////////////////////////
// Staged Gpu Buffer Functions
/////////////////////////////////////

void RxGpuBuffer::Create(RxGpuBufferType buffer_type, void* data, uint64 size)
{
    Size = size;
    Type = buffer_type;

    RxRawGpuBuffer staging_buffer;
    staging_buffer.Create(RxGpuBufferType::eTransfer, Size, VMA_MEMORY_USAGE_CPU_TO_GPU);
    staging_buffer.Upload(data, size);

    // Create the GPU-only buffer as a transfer destination
    this->Create(buffer_type, this->Size, VMA_MEMORY_USAGE_GPU_ONLY, RxGpuBufferFlags::eTransferReceiver);

    // Transfer
    Fx_Fwd_SubmitUploadCmd(
        [&](RxCommandBuffer& cmd)
        {
            VkBufferCopy copy = { .srcOffset = 0, .dstOffset = 0, .size = Size };
            vkCmdCopyBuffer(cmd.CommandBuffer, staging_buffer.Buffer, this->Buffer, 1, &copy);
        });

    staging_buffer.Destroy();
}

void RxGpuBuffer::Create(RxGpuBufferType buffer_type, const FxAnonArray& data)
{
    Create(buffer_type, data.pData, data.Size * data.ObjectSize);
}
