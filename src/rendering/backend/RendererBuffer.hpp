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

    HYP_API RendererResult CheckCanAllocate(SizeType size) const;

    HYP_API uint64 GetBufferDeviceAddress() const;

    HYP_API RendererResult Create(
        SizeType buffer_size,
        SizeType buffer_alignment = 0
    );
    HYP_API RendererResult Destroy();

    HYP_API RendererResult EnsureCapacity(
        SizeType minimum_size,
        bool *out_size_changed = nullptr
    );

    HYP_API RendererResult EnsureCapacity(
        SizeType minimum_size,
        SizeType alignment,
        bool *out_size_changed = nullptr
    );

    HYP_API void Memset(SizeType count, ubyte value);

    HYP_API void Copy(SizeType count, const void *ptr);
    HYP_API void Copy(SizeType offset, SizeType count, const void *ptr);

    HYP_API void Map();
    HYP_API void Unmap();

    HYP_API void Read(SizeType count, void *out_ptr) const;
    HYP_API void Read(SizeType offset, SizeType count, void *out_ptr) const;
    
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

} // namespace renderer
} // namespace hyperion
#endif