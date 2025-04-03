/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_BUFFER_HPP
#define HYPERION_BACKEND_RENDERER_BUFFER_HPP

#include <core/Defines.hpp>

#include <core/functional/Proc.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/HashSet.hpp>

#include <rendering/backend/RendererResult.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RendererShader.hpp>
#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/Platform.hpp>

#include <Types.hpp>

namespace hyperion {
namespace renderer {

enum class ResourceState : uint32
{
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

enum class GPUBufferType
{
    NONE = 0,
    MESH_INDEX_BUFFER,
    MESH_VERTEX_BUFFER,
    CONSTANT_BUFFER,
    STORAGE_BUFFER,
    ATOMIC_COUNTER,
    STAGING_BUFFER,
    INDIRECT_ARGS_BUFFER,
    SHADER_BINDING_TABLE,
    ACCELERATION_STRUCTURE_BUFFER,
    ACCELERATION_STRUCTURE_INSTANCE_BUFFER,
    RT_MESH_INDEX_BUFFER,
    RT_MESH_VERTEX_BUFFER,
    SCRATCH_BUFFER,
    MAX
};

enum BufferIDMask : uint64
{
    ID_MASK_BUFFER = (0x1ull << 32ull),
    ID_MASK_IMAGE = (0x2ull << 32ull)
};

namespace platform {

template <PlatformType PLATFORM>
class CommandBuffer;

template <PlatformType PLATFORM>
class Instance;

template <PlatformType PLATFORM>
class Device;

template <PlatformType PLATFORM>
struct GPUBufferPlatformImpl;

template <PlatformType PLATFORM>
class GPUBuffer
{
public:
    static constexpr PlatformType platform = PLATFORM;

    HYP_API GPUBuffer(GPUBufferType type);

    GPUBuffer(const GPUBuffer &other)                   = delete;
    GPUBuffer &operator=(const GPUBuffer &other)        = delete;

    HYP_API GPUBuffer(GPUBuffer &&other) noexcept;
    GPUBuffer &operator=(GPUBuffer &&other) noexcept    = delete;

    HYP_API ~GPUBuffer();

    HYP_FORCE_INLINE GPUBufferPlatformImpl<PLATFORM> &GetPlatformImpl()
        { return m_platform_impl; }

    HYP_FORCE_INLINE const GPUBufferPlatformImpl<PLATFORM> &GetPlatformImpl() const
        { return m_platform_impl; }

    HYP_FORCE_INLINE GPUBufferType GetBufferType() const
        { return m_type; }

    HYP_FORCE_INLINE ResourceState GetResourceState() const
        { return m_resource_state; }

    HYP_FORCE_INLINE void SetResourceState(ResourceState resource_state)
        { m_resource_state = resource_state; }

    HYP_API bool IsCreated() const;
    HYP_API bool IsCPUAccessible() const;
    HYP_API uint32 Size() const;

    HYP_API void InsertBarrier(
        CommandBuffer<PLATFORM> *command_buffer,
        ResourceState new_state
    ) const;

    HYP_API void InsertBarrier(
        CommandBuffer<PLATFORM> *command_buffer,
        ResourceState new_state,
        ShaderModuleType shader_type
    ) const;

    HYP_API void CopyFrom(
        CommandBuffer<PLATFORM> *command_buffer,
        const GPUBuffer *src_buffer,
        SizeType count
    );

    HYP_API RendererResult CheckCanAllocate(Device<PLATFORM> *device, SizeType size) const;

    HYP_API uint64 GetBufferDeviceAddress(Device<PLATFORM> *device) const;

    HYP_API RendererResult Create(
        Device<PLATFORM> *device,
        SizeType buffer_size,
        SizeType buffer_alignment = 0
    );
    HYP_API RendererResult Destroy(Device<PLATFORM> *device);

    HYP_API RendererResult EnsureCapacity(
        Device<PLATFORM> *device,
        SizeType minimum_size,
        bool *out_size_changed = nullptr
    );

    HYP_API RendererResult EnsureCapacity(
        Device<PLATFORM> *device,
        SizeType minimum_size,
        SizeType alignment,
        bool *out_size_changed = nullptr
    );

    HYP_API void Memset(Device<PLATFORM> *device, SizeType count, ubyte value);

    HYP_API void Copy(Device<PLATFORM> *device, SizeType count, const void *ptr);
    HYP_API void Copy(Device<PLATFORM> *device, SizeType offset, SizeType count, const void *ptr);

    HYP_API void Map(Device<PLATFORM> *device);
    HYP_API void Unmap(Device<PLATFORM> *device);

    HYP_API void Read(Device<PLATFORM> *device, SizeType count, void *out_ptr) const;
    HYP_API void Read(Device<PLATFORM> *device, SizeType offset, SizeType count, void *out_ptr) const;
    
private:
    GPUBufferPlatformImpl<PLATFORM> m_platform_impl;

    GPUBufferType                   m_type;
    mutable ResourceState           m_resource_state;
};

template <PlatformType PLATFORM>
class UniformBuffer : public GPUBuffer<PLATFORM>
{
public:
    UniformBuffer()
        : GPUBuffer<PLATFORM>(GPUBufferType::CONSTANT_BUFFER)
    {
    }
};

template <PlatformType PLATFORM>
class StorageBuffer : public GPUBuffer<PLATFORM>
{
public:
    StorageBuffer()
        : GPUBuffer<PLATFORM>(GPUBufferType::STORAGE_BUFFER)
    {
    }
};

template <PlatformType PLATFORM>
class AtomicCounterBuffer : public GPUBuffer<PLATFORM>
{
public:
    AtomicCounterBuffer()
        : GPUBuffer<PLATFORM>(GPUBufferType::ATOMIC_COUNTER)
    {
    }
};

template <PlatformType PLATFORM>
class StagingBuffer : public GPUBuffer<PLATFORM>
{
public:
    StagingBuffer()
        : GPUBuffer<PLATFORM>(GPUBufferType::STAGING_BUFFER)
    {
    }
};

template <PlatformType PLATFORM>
class IndirectBuffer : public GPUBuffer<PLATFORM>
{
public:
    IndirectBuffer()
        : GPUBuffer<PLATFORM>(GPUBufferType::INDIRECT_ARGS_BUFFER)
    {
    }

    HYP_API void DispatchIndirect(CommandBuffer<PLATFORM> *command_buffer, SizeType offset = 0) const;
};

template <PlatformType PLATFORM>
class ShaderBindingTableBuffer : public GPUBuffer<PLATFORM>
{
public:
    ShaderBindingTableBuffer()
        : GPUBuffer<PLATFORM>(GPUBufferType::SHADER_BINDING_TABLE)
    {
    }
};

template <PlatformType PLATFORM>
class AccelerationStructureBuffer : public GPUBuffer<PLATFORM>
{
public:
    AccelerationStructureBuffer()
        : GPUBuffer<PLATFORM>(GPUBufferType::ACCELERATION_STRUCTURE_BUFFER)
    {
    }
};

template <PlatformType PLATFORM>
class AccelerationStructureInstancesBuffer : public GPUBuffer<PLATFORM>
{
public:
    AccelerationStructureInstancesBuffer()
        : GPUBuffer<PLATFORM>(GPUBufferType::ACCELERATION_STRUCTURE_INSTANCE_BUFFER)
    {
    }
};

template <PlatformType PLATFORM>
class PackedVertexStorageBuffer : public GPUBuffer<PLATFORM>
{
public:
    PackedVertexStorageBuffer()
        : GPUBuffer<PLATFORM>(GPUBufferType::RT_MESH_VERTEX_BUFFER)
    {
    }
};

template <PlatformType PLATFORM>
class PackedIndexStorageBuffer : public GPUBuffer<PLATFORM>
{
public:
    PackedIndexStorageBuffer()
        : GPUBuffer<PLATFORM>(GPUBufferType::RT_MESH_INDEX_BUFFER)
    {
    }
};

template <PlatformType PLATFORM>
class ScratchBuffer : public GPUBuffer<PLATFORM>
{
public:
    ScratchBuffer()
        : GPUBuffer<PLATFORM>(GPUBufferType::SCRATCH_BUFFER)
    {
    }
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererBuffer.hpp>
#else
#error Unsupported rendering backend
#endif

namespace hyperion {
namespace renderer {

using GPUBuffer = platform::GPUBuffer<Platform::CURRENT>;
using UniformBuffer = platform::UniformBuffer<Platform::CURRENT>;
using StorageBuffer = platform::StorageBuffer<Platform::CURRENT>;
using StagingBuffer = platform::StagingBuffer<Platform::CURRENT>;
using IndirectBuffer = platform::IndirectBuffer<Platform::CURRENT>;
using ShaderBindingTableBuffer = platform::ShaderBindingTableBuffer<Platform::CURRENT>;

// Forward declared
using Device = platform::Device<Platform::CURRENT>;

class StagingBufferPool
{
    struct StagingBufferRecord
    {
        SizeType                        size;
        std::unique_ptr<StagingBuffer>  buffer;
        std::time_t                     last_used;
    };

public:
    class Context
    {
        friend class StagingBufferPool;

        StagingBufferPool           *m_pool;
        Device                      *m_device;
        Array<StagingBufferRecord>  m_staging_buffers;
        HashSet<StagingBuffer *>    m_used;

        Context(StagingBufferPool *pool, Device *device)
            : m_pool(pool),
              m_device(device)
        {
        }

        StagingBuffer *CreateStagingBuffer(SizeType size);

    public:
        /*! \brief Acquire a staging buffer from the pool of at least \ref{required_size} bytes,
         * creating one if it does not exist yet. */
        StagingBuffer *Acquire(SizeType required_size);
    };

    using UseFunction = Proc<RendererResult(Context &)>;

    static constexpr time_t hold_time = 1000;
    static constexpr uint32 gc_threshold = 5; /* run every 5 Use() calls */

    /*! \brief Use the staging buffer pool. GC will not run until after the given function
     * is called, and the staging buffers created will not be able to be reused.
     * This will allow the staging buffer(s) acquired by this to be used in sequence
     * in a single time command buffer */
    HYP_DEPRECATED RendererResult Use(Device *device, UseFunction &&fn);

    HYP_DEPRECATED RendererResult GC(Device *device);

    /*! \brief Destroy all remaining staging buffers in the pool */
    HYP_DEPRECATED RendererResult Destroy(Device *device);

private:
    StagingBuffer *FindStagingBuffer(SizeType size);

    Array<StagingBufferRecord>  m_staging_buffers;
    uint32                      use_calls = 0;
};

} // namespace renderer
} // namespace hyperion
#endif