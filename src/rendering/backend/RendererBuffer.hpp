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

enum class GPUBufferType : uint8
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

class GPUBufferBase : public RenderObject<GPUBufferBase>
{
public:
    virtual ~GPUBufferBase() override = default;

    HYP_FORCE_INLINE GPUBufferType GetBufferType() const
    {
        return m_type;
    }

    HYP_FORCE_INLINE uint32 Size() const
    {
        return uint32(m_size);
    }

    HYP_FORCE_INLINE ResourceState GetResourceState() const
    {
        return m_resource_state;
    }

    HYP_API virtual RendererResult Create() = 0;
    HYP_API virtual RendererResult Destroy() = 0;

    HYP_API virtual bool IsCreated() const = 0;
    HYP_API virtual bool IsCPUAccessible() const = 0;

    HYP_API virtual void InsertBarrier(CommandBufferBase* command_buffer, ResourceState new_state) const = 0;
    HYP_API virtual void InsertBarrier(CommandBufferBase* command_buffer, ResourceState new_state, ShaderModuleType shader_type) const = 0;

    HYP_API virtual void CopyFrom(
        CommandBufferBase* command_buffer,
        const GPUBufferBase* src_buffer,
        SizeType count) = 0;

    HYP_API virtual RendererResult EnsureCapacity(
        SizeType minimum_size,
        bool* out_size_changed = nullptr) = 0;

    HYP_API virtual RendererResult EnsureCapacity(
        SizeType minimum_size,
        SizeType alignment,
        bool* out_size_changed = nullptr) = 0;

    HYP_API virtual void Memset(SizeType count, ubyte value) = 0;

    HYP_API virtual void Copy(SizeType count, const void* ptr) = 0;
    HYP_API virtual void Copy(SizeType offset, SizeType count, const void* ptr) = 0;

    HYP_API virtual void Read(SizeType count, void* out_ptr) const = 0;
    HYP_API virtual void Read(SizeType offset, SizeType count, void* out_ptr) const = 0;

    HYP_API virtual void Map() const = 0;
    HYP_API virtual void Unmap() const = 0;

protected:
    GPUBufferBase(GPUBufferType type, SizeType size, SizeType alignment = 0)
        : m_type(type),
          m_size(size),
          m_alignment(alignment),
          m_resource_state(ResourceState::UNDEFINED)
    {
    }

    GPUBufferType m_type;
    SizeType m_size;
    SizeType m_alignment;

    mutable ResourceState m_resource_state;
};

} // namespace renderer
} // namespace hyperion

#endif