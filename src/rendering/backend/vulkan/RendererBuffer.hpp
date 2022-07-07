#ifndef HYPERION_RENDERER_BUFFER_H
#define HYPERION_RENDERER_BUFFER_H

#include "RendererResult.hpp"
#include "RendererStructs.hpp"
#include "RendererShader.hpp"

#include <system/vma/VmaUsage.hpp>

#include <functional>
#include <unordered_set>
#include <unordered_map>

namespace hyperion {
namespace renderer {

class Device;
class CommandBuffer;
class StagingBuffer;

class StagingBufferPool {
    struct StagingBufferRecord {
        size_t size;
        std::unique_ptr<StagingBuffer> buffer;
        std::time_t last_used;
    };

public:
    class Context {
        friend class StagingBufferPool;

        StagingBufferPool *m_pool;
        Device            *m_device;
        std::vector<StagingBufferRecord>    m_staging_buffers;
        std::unordered_set<StagingBuffer *> m_used;

        Context(StagingBufferPool *pool, Device *device)
            : m_pool(pool),
              m_device(device)
        {
        }

        StagingBuffer *CreateStagingBuffer(size_t size);

    public:
        /* \brief Acquire a staging buffer from the pool of at least \ref required_size bytes,
         * creating one if it does not exist yet. */
        StagingBuffer *Acquire(size_t required_size);
    };

    using UseFunction = std::function<Result(Context &context)>;

    static constexpr time_t hold_time = 1000;
    static constexpr uint32_t gc_threshold = 5; /* run every 5 Use() calls */

    /* \brief Use the staging buffer pool. GC will not run until after the given function
     * is called, and the staging buffers created will not be able to be reused.
     * This will allow the staging buffer(s) acquired by this to be used in sequence
     * in a single time command buffer
     */
    Result Use(Device *device, UseFunction &&fn);

    Result GC(Device *device);

    /* \brief Destroy all remaining staging buffers in the pool */
    Result Destroy(Device *device);

private:
    StagingBuffer *FindStagingBuffer(size_t size);

    std::vector<StagingBufferRecord> m_staging_buffers;
    uint32_t                         use_calls = 0;
};

class GPUMemory {
public:
    struct Stats {
        size_t gpu_memory_used = 0;
        size_t last_gpu_memory_used = 0;
        int64_t last_diff = 0;
        std::time_t last_timestamp = 0;
        const int64_t time_diff = 10000;

        HYP_FORCE_INLINE void IncMemoryUsage(size_t amount)
        {
            gpu_memory_used += amount;

            UpdateStats();
        }

        HYP_FORCE_INLINE void DecMemoryUsage(size_t amount)
        {
            AssertThrow(gpu_memory_used >= amount);

            gpu_memory_used -= amount;

            UpdateStats();
        }

        HYP_FORCE_INLINE void UpdateStats()
        {
            const std::time_t now = std::time(nullptr);

            if (last_timestamp == 0 || now - last_timestamp >= time_diff) {
                if (gpu_memory_used > last_gpu_memory_used) {
                    last_diff = gpu_memory_used - last_gpu_memory_used;
                } else {
                    last_diff = last_gpu_memory_used - gpu_memory_used;
                }

                last_timestamp = now;
                last_gpu_memory_used = gpu_memory_used;
            }
        }
    };

    enum class ResourceState : uint32_t {
        UNDEFINED,
        PRE_INITIALIZED,
        COMMON,
        VERTEX_BUFFER,
        CONSTANT_BUFFER,
        INDEX_BUFFER,
        RENDER_TARGET,
        UNORDERED_ACCESS,
        DEPTH_STENCIL,
        SHADER_RESOURCE,
        STREAM_OUT,
        INDIRECT_ARG,
        COPY_DST,
        COPY_SRC,
        RESOLVE_DST,
        RESOLVE_SRC,
        PRESENT,
        READ_GENERIC,
        PREDICATION
    };

    static uint32_t FindMemoryType(Device *device, uint32_t vk_type_filter, VkMemoryPropertyFlags properties);
    static VkImageLayout GetImageLayout(ResourceState state);
    static VkAccessFlags GetAccessMask(ResourceState state);
    static VkPipelineStageFlags GetShaderStageMask(
        ResourceState state,
        bool src,
        ShaderModule::Type shader_type = ShaderModule::Type::UNSET
    );

    GPUMemory();
    GPUMemory(const GPUMemory &other) = delete;
    GPUMemory &operator=(const GPUMemory &other) = delete;
    ~GPUMemory();

    inline ResourceState GetResourceState() const
        { return resource_state; }

    inline void SetResourceState(ResourceState new_state)
        { resource_state = new_state; }

    inline void *GetMapping(Device *device) const
    {
        if (!map) {
            Map(device, &map);
        }

        return map;
    }

    void Copy(Device *device, size_t count, const void *ptr);
    void Copy(Device *device, size_t offset, size_t count, const void *ptr);

    void Read(Device *device, size_t count, void *out_ptr) const;
    
    VmaAllocation allocation;
    VkDeviceSize size;
    
    static Stats stats;

protected:
    void Map(Device *device, void **ptr) const;
    void Unmap(Device *device) const;
    void Create();
    void Destroy();

    uint32_t sharing_mode;
    uint32_t index;
    mutable void *map;
    mutable ResourceState resource_state;
};

/* buffers */

class GPUBuffer : public GPUMemory {
public:
    GPUBuffer(
        VkBufferUsageFlags usage_flags,
        VmaMemoryUsage vma_usage = VMA_MEMORY_USAGE_AUTO,
        VmaAllocationCreateFlags vma_allocation_create_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
    );
    GPUBuffer(const GPUBuffer &other) = delete;
    GPUBuffer &operator=(const GPUBuffer &other) = delete;
    ~GPUBuffer();

    void InsertBarrier(
        CommandBuffer *command_buffer,
        ResourceState new_state
    ) const;

    void CopyFrom(
        CommandBuffer *command_buffer,
        const GPUBuffer *src_buffer,
        size_t count
    );

    [[nodiscard]] Result CopyStaged(
        Instance *instance,
        const void *ptr,
        size_t count
    );

    Result CheckCanAllocate(Device *device, size_t size) const;

    /* \brief Calls vkGetBufferDeviceAddressKHR. Only use this if the extension is enabled */
    uint64_t GetBufferDeviceAddress(Device *device) const;

    [[nodiscard]] Result Create(Device *device, size_t buffer_size);
    [[nodiscard]] Result Destroy(Device *device);
    [[nodiscard]] Result EnsureCapacity(Device *device,
        size_t minimum_size,
        bool *out_size_changed = nullptr);

#if HYP_DEBUG_MODE
    void DebugLogBuffer(Device *device) const;

    template <class T = uint8_t>
    std::vector<T> DebugReadBytes(Device *device) const
    {
        std::vector<T> data;
        data.resize(size / sizeof(T));

        Read(device, size, &data[0]);

        return data;
    }
#endif

    VkBuffer buffer;

private:
    Result CheckCanAllocate(Device *device,
        const VkBufferCreateInfo &buffer_create_info,
        const VmaAllocationCreateInfo &allocation_create_info,
        size_t size) const;

    VmaAllocationCreateInfo GetAllocationCreateInfo(Device *device) const;
    VkBufferCreateInfo GetBufferCreateInfo(Device *device) const;

    VkBufferUsageFlags       usage_flags;
    VmaMemoryUsage           vma_usage;
    VmaAllocationCreateFlags vma_allocation_create_flags;
};

class VertexBuffer : public GPUBuffer {
public:
    VertexBuffer();

    void Bind(CommandBuffer *command_buffer);
};

class IndexBuffer : public GPUBuffer {
public:
    IndexBuffer();

    void Bind(CommandBuffer *command_buffer);

    DatumType GetDatumType() const          { return m_datum_type; }
    void SetDatumType(DatumType datum_type) { m_datum_type = datum_type; }

private:
    DatumType m_datum_type = DatumType::UNSIGNED_INT;
};

class UniformBuffer : public GPUBuffer {
public:
    UniformBuffer();
};

class StorageBuffer : public GPUBuffer {
public:
    StorageBuffer();
};

class AtomicCounterBuffer : public GPUBuffer {
public:
    AtomicCounterBuffer();
};

class StagingBuffer : public GPUBuffer {
public:
    StagingBuffer();
};

class IndirectBuffer : public GPUBuffer {
public:
    IndirectBuffer();

    void DispatchIndirect(CommandBuffer *command_buffer, size_t offset = 0) const;
};

class ShaderBindingTableBuffer : public GPUBuffer {
public:
    ShaderBindingTableBuffer();

    VkStridedDeviceAddressRegionKHR region;
};

class AccelerationStructureBuffer : public GPUBuffer {
public:
    AccelerationStructureBuffer();
};

class AccelerationStructureInstancesBuffer : public GPUBuffer {
public:
    AccelerationStructureInstancesBuffer();
};

class PackedVertexStorageBuffer : public GPUBuffer {
public:
    PackedVertexStorageBuffer();
};

class PackedIndexStorageBuffer : public GPUBuffer {
public:
    PackedIndexStorageBuffer();
};

class ScratchBuffer : public GPUBuffer {
public:
    ScratchBuffer();
};

/* images */

class GPUImageMemory : public GPUMemory {
public:

    enum class Aspect {
        COLOR,
        DEPTH
    };

    GPUImageMemory();
    GPUImageMemory(const GPUImageMemory &other) = delete;
    GPUImageMemory &operator=(const GPUImageMemory &other) = delete;
    ~GPUImageMemory();

    ResourceState GetSubResourceState(const ImageSubResource &sub_resource) const;
    void SetSubResourceState(const ImageSubResource &sub_resource, ResourceState new_state);

    void SetResourceState(ResourceState new_state);

    void InsertBarrier(
        CommandBuffer *command_buffer,
        ResourceState new_state
    );

    void InsertBarrier(
        CommandBuffer *command_buffer,
        const ImageSubResource &sub_resource,
        ResourceState new_state
    );

    void InsertSubResourceBarrier(
        CommandBuffer *command_buffer,
        const ImageSubResource &sub_resource,
        ResourceState new_state
    );

    [[nodiscard]] Result Create(Device *device, size_t size, VkImageCreateInfo *image_info);
    [[nodiscard]] Result Destroy(Device *device);

    VkImage image;

private:
    std::unordered_map<ImageSubResource, ResourceState> sub_resources;
};

} // namespace renderer
} // namespace hyperion

#endif //HYPERION_RENDERER_BUFFER_H
