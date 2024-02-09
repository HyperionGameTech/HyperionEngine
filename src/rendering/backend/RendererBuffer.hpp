#ifndef HYPERION_V2_BACKEND_RENDERER_BUFFER_H
#define HYPERION_V2_BACKEND_RENDERER_BUFFER_H

#include <util/Defines.hpp>
#include <rendering/backend/RendererResult.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/Platform.hpp>
#include <Types.hpp>

namespace hyperion {
namespace renderer {

enum class ResourceState : uint
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
class Device;

template <PlatformType PLATFORM>
class GPUMemory {};

template <PlatformType PLATFORM>
class GPUImageMemory : public GPUMemory<PLATFORM> {};

template <PlatformType PLATFORM>
class GPUBuffer : public GPUMemory<PLATFORM>
{
    GPUBuffer(GPUBufferType) { /* dummy */ }
};

template <PlatformType PLATFORM>
class VertexBuffer : public GPUBuffer<PLATFORM>
{
public:
    VertexBuffer()
        : GPUBuffer<PLATFORM>(GPUBufferType::MESH_VERTEX_BUFFER)
    {
    }

    void Bind(CommandBuffer<PLATFORM> *command_buffer);
};

template <PlatformType PLATFORM>
class IndexBuffer : public GPUBuffer<PLATFORM>
{
public:
    IndexBuffer()
        : GPUBuffer<PLATFORM>(GPUBufferType::MESH_INDEX_BUFFER)
    {
    }

    void Bind(CommandBuffer<PLATFORM> *command_buffer);

    DatumType GetDatumType() const { return m_datum_type; }
    void SetDatumType(DatumType datum_type) { m_datum_type = datum_type; }

private:
    DatumType m_datum_type = DatumType::UNSIGNED_INT;
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

    void DispatchIndirect(CommandBuffer<PLATFORM> *command_buffer, SizeType offset = 0) const;
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

using GPUMemory                             = platform::GPUMemory<Platform::CURRENT>;

using GPUImageMemory                        = platform::GPUImageMemory<Platform::CURRENT>;

using GPUBuffer                             = platform::GPUBuffer<Platform::CURRENT>;
using VertexBuffer                          = platform::VertexBuffer<Platform::CURRENT>;
using IndexBuffer                           = platform::IndexBuffer<Platform::CURRENT>;
using UniformBuffer                         = platform::UniformBuffer<Platform::CURRENT>;
using StorageBuffer                         = platform::StorageBuffer<Platform::CURRENT>;
using AtomicCounterBuffer                   = platform::AtomicCounterBuffer<Platform::CURRENT>;
using StagingBuffer                         = platform::StagingBuffer<Platform::CURRENT>;
using IndirectBuffer                        = platform::IndirectBuffer<Platform::CURRENT>;
using ShaderBindingTableBuffer              = platform::ShaderBindingTableBuffer<Platform::CURRENT>;
using AccelerationStructureBuffer           = platform::AccelerationStructureBuffer<Platform::CURRENT>;
using AccelerationStructureInstancesBuffer  = platform::AccelerationStructureInstancesBuffer<Platform::CURRENT>;
using PackedVertexStorageBuffer             = platform::PackedVertexStorageBuffer<Platform::CURRENT>;
using PackedIndexStorageBuffer              = platform::PackedIndexStorageBuffer<Platform::CURRENT>;
using ScratchBuffer                         = platform::ScratchBuffer<Platform::CURRENT>;

// Forward declared
using Device                                = platform::Device<Platform::CURRENT>;

class StagingBufferPool
{
    struct StagingBufferRecord
    {
        SizeType size;
        std::unique_ptr<StagingBuffer> buffer;
        std::time_t last_used;
    };

public:
    class Context
    {
        friend class StagingBufferPool;

        StagingBufferPool                   *m_pool;
        renderer::Device                    *m_device;
        std::vector<StagingBufferRecord>    m_staging_buffers;
        std::unordered_set<StagingBuffer *> m_used;

        Context(StagingBufferPool *pool, renderer::Device *device)
            : m_pool(pool),
              m_device(device)
        {
        }

        StagingBuffer *CreateStagingBuffer(SizeType size);

    public:
        /* \brief Acquire a staging buffer from the pool of at least \ref required_size bytes,
         * creating one if it does not exist yet. */
        StagingBuffer *Acquire(SizeType required_size);
    };

    using UseFunction = std::function<renderer::Result(Context &context)>;

    static constexpr time_t hold_time = 1000;
    static constexpr uint gc_threshold = 5; /* run every 5 Use() calls */

    /* \brief Use the staging buffer pool. GC will not run until after the given function
     * is called, and the staging buffers created will not be able to be reused.
     * This will allow the staging buffer(s) acquired by this to be used in sequence
     * in a single time command buffer
     */
    renderer::Result Use(renderer::Device *device, UseFunction &&fn);

    renderer::Result GC(renderer::Device *device);

    /* \brief Destroy all remaining staging buffers in the pool */
    renderer::Result Destroy(renderer::Device *device);

private:
    StagingBuffer *FindStagingBuffer(SizeType size);

    std::vector<StagingBufferRecord> m_staging_buffers;
    uint use_calls = 0;
};

} // namespace renderer
} // namespace hyperion
#endif