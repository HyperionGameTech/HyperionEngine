#pragma once

#include "Fwd/Rx_Fwd_AddToDeletionQueue.hpp"
#include "Fwd/Rx_Fwd_GetGpuAllocator.hpp"
#include "Fwd/Rx_Fwd_SubmitUploadGpuCmd.hpp"

#include <Core/FxAnonArray.hpp>
#include <Core/FxSlice.hpp>
#include <Core/FxTypes.hpp>
#include <Core/FxUtil.hpp>

// #define FX_DEBUG_GPU_BUFFER_ALLOCATION_NAMES 1

class RxRawGpuBuffer;
class RxGpuBuffer;

enum class RxBufferUsageType
{
    Vertices = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
    Indices = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
};

enum class RxGpuBufferFlags : uint16
{
    eNone = 0x00,
    /** The buffer is mapped for the lifetime of the buffer. */
    ePersistentMapped = 0x01,
    eTransferReceiver = 0x02,

    FX_DEFINE_AS_FLAG_ENUM,
};

// FX_DEFINE_ENUM_AS_FLAGS(RxGpuBufferFlags);

enum class RxGpuBufferType
{
    eStorage,
    eStorageWithOffset,
    eUniform,
    eUniformWithOffset,
    eTransfer,
    eVertexBuffer,
    eIndexBuffer,
};

class RxGpuBufferUtil
{
public:
    static constexpr VkBufferUsageFlags BufferTypeToUnderlying(RxGpuBufferType type)
    {
        switch (type) {
        case RxGpuBufferType::eStorage:
        case RxGpuBufferType::eStorageWithOffset:
            return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

        case RxGpuBufferType::eUniform:
        case RxGpuBufferType::eUniformWithOffset:
            return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

        case RxGpuBufferType::eTransfer:
            return VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        case RxGpuBufferType::eVertexBuffer:
            return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        case RxGpuBufferType::eIndexBuffer:
            return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        }

        return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    }

    static constexpr VkDescriptorType BufferTypeToDescriptorType(RxGpuBufferType type)
    {
        switch (type) {
        case RxGpuBufferType::eStorage:
            return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case RxGpuBufferType::eStorageWithOffset:
            return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;

        case RxGpuBufferType::eUniform:
            return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case RxGpuBufferType::eUniformWithOffset:
            return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;

        default:;
        }

        return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    }
};


/**
 * @brief Provides a GPU buffer that can be created with more complex parameters without staging.
 */
class RxRawGpuBuffer
{
public:
    RxRawGpuBuffer() = default;

    RxRawGpuBuffer(RxRawGpuBuffer& other) = delete;

    RxRawGpuBuffer operator=(RxRawGpuBuffer& other) = delete;

    void Create(RxGpuBufferType buffer_type, uint64 size_in_bytes, VmaMemoryUsage memory_usage,
                RxGpuBufferFlags buffer_flags = RxGpuBufferFlags::eNone);

    void FlushToGpu(uint32 offset, uint32 size)
    {
        vmaFlushAllocation(Fx_Fwd_GetGpuAllocator(), Allocation, offset, size);
    }

    void Map();
    void UnMap();

    bool IsMapped() const { return pMappedBuffer != nullptr; }

    /**
     * @brief Uploads raw data buffer to the GPU buffer.
     * @param data The data to upload
     * @param size The size of the buffer in bytes
     */
    void Upload(void* data, uint64 size);

    template <typename TElementType>
    void Upload(const FxSizedArray<TElementType>& data)
    {
        Upload(reinterpret_cast<void*>(data.pData), data.GetSizeInBytes());
    }

    void Destroy();

    ~RxRawGpuBuffer() { Destroy(); }

public:
    RxGpuBufferType Type = RxGpuBufferType::eStorage;

    VkBuffer Buffer = nullptr;
    VmaAllocation Allocation = nullptr;

    void* pMappedBuffer = nullptr;

    std::atomic_bool Initialized = false;
    uint64 Size = 0;

private:
    RxGpuBufferFlags mBufferFlags = RxGpuBufferFlags::eNone;
};


class RxGpuBufferMapContext
{
public:
    RxGpuBufferMapContext(RxRawGpuBuffer* buffer) : mpGpuBuffer(buffer) { mpGpuBuffer->Map(); }

    /** Returns the raw pointer representation of the mapped data. */
    operator void*()
    {
        if (!mpGpuBuffer->pMappedBuffer) {
            return nullptr;
        }

        return mpGpuBuffer->pMappedBuffer;
    }

    /** Returns the pointer representation of the mapped data. */
    template <typename TElementType>
    TElementType* GetPtr()
    {
        FxDebugAssert(mpGpuBuffer->pMappedBuffer != nullptr);
        return static_cast<TElementType*>(mpGpuBuffer->pMappedBuffer);
    }

    ~RxGpuBufferMapContext() { mpGpuBuffer->UnMap(); }

    /**
     * Manually unmaps the buffer.
     */
    void UnMap() const { mpGpuBuffer->UnMap(); }

private:
    RxRawGpuBuffer* mpGpuBuffer = nullptr;
};


/**
 * @brief A GPU buffer that is created CPU side, and copied over to a GPU-only buffer. This is the default
 * buffer type.
 */
class RxGpuBuffer : public RxRawGpuBuffer
{
private:
    using RxRawGpuBuffer::Create;

public:
    RxGpuBuffer() = default;


    void Create(RxGpuBufferType buffer_type, void* data, uint64 size);
    void Create(RxGpuBufferType buffer_type, const FxAnonArray& data);

    template <typename TElementType>
    void Create(RxGpuBufferType buffer_type, const FxSlice<TElementType>& data)
    {
        Size = data.Size * sizeof(TElementType);
        Type = buffer_type;

        RxRawGpuBuffer staging_buffer;
        staging_buffer.Create(RxGpuBufferType::eTransfer, Size, VMA_MEMORY_USAGE_CPU_TO_GPU);
        staging_buffer.Upload(data, Size);

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
};
