/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/functional/Proc.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/HashSet.hpp>

#include <rendering/RenderResult.hpp>
#include <rendering/Shared.hpp>
#include <rendering/RenderShader.hpp>
#include <rendering/RenderObject.hpp>

#include <core/Types.hpp>

namespace hyperion {

HYP_ENUM()
enum class GpuBufferType : uint8
{
    NONE = 0,
    MESH_INDEX_BUFFER,
    MESH_VERTEX_BUFFER,
    CBUFF,
    SSBO,
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

HYP_CLASS(Abstract, NoScriptBindings)
class GpuBufferBase : public HypObjectBase
{
    HYP_OBJECT_BODY(GpuBufferBase);

public:
    virtual ~GpuBufferBase() override = default;

    Name GetDebugName() const
    {
        return m_debugName;
    }
    
    virtual void SetDebugName(Name name)
    {
        m_debugName = name;
    }

    HYP_FORCE_INLINE GpuBufferType GetBufferType() const
    {
        return m_type;
    }

    HYP_FORCE_INLINE uint32 Size() const
    {
        return uint32(m_size);
    }

    HYP_FORCE_INLINE ResourceState GetResourceState() const
    {
        return m_resourceState;
    }

    virtual RendererResult Create() = 0;

    virtual bool IsCreated() const = 0;

    virtual bool IsCpuAccessible() const = 0;

    HYP_FORCE_INLINE void SetRequireCpuAccessible(bool requireCpuAccessible)
    {
        HYP_GFX_ASSERT(!IsCreated(), "Cannot set RequireCpuAccessible after the buffer has been created!");

        m_requireCpuAccessible = requireCpuAccessible;
    }

    virtual void InsertBarrier(CommandBufferBase* commandBuffer, ResourceState newState) const = 0;
    virtual void InsertBarrier(CommandBufferBase* commandBuffer, ResourceState newState, ShaderModuleType shaderType) const = 0;

    virtual void CopyFrom(
        CommandBufferBase* commandBuffer,
        const GpuBufferBase* srcBuffer,
        uint32 count) = 0;

    virtual void CopyFrom(
        CommandBufferBase* commandBuffer,
        const GpuBufferBase* srcBuffer,
        uint32 srcOffset, uint32 dstOffset,
        uint32 count) = 0;

    virtual RendererResult EnsureCapacity(
        SizeType minimumSize,
        bool* outSizeChanged = nullptr) = 0;

    virtual RendererResult EnsureCapacity(
        SizeType minimumSize,
        SizeType alignment,
        bool* outSizeChanged = nullptr) = 0;

    virtual void Memset(SizeType count, ubyte value) = 0;

    virtual void Copy(SizeType count, const void* ptr) = 0;
    virtual void Copy(SizeType offset, SizeType count, const void* ptr) = 0;

    virtual void Read(SizeType count, void* outPtr) const = 0;
    virtual void Read(SizeType offset, SizeType count, void* outPtr) const = 0;

    virtual void Map() const = 0;
    virtual void Unmap() const = 0;

protected:
    GpuBufferBase(GpuBufferType type, SizeType size, SizeType alignment = 0)
        : m_type(type),
          m_size(size),
          m_alignment(alignment),
          m_resourceState(RS_UNDEFINED),
          m_requireCpuAccessible(false)
    {
    }

    GpuBufferType m_type;
    SizeType m_size;
    SizeType m_alignment;

    mutable ResourceState m_resourceState;

    Name m_debugName;

    bool m_requireCpuAccessible : 1;
};

} // namespace hyperion
