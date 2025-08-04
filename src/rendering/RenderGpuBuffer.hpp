/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/functional/Proc.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/HashSet.hpp>

#include <rendering/RenderResult.hpp>
#include <rendering/RenderShader.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/Shared.hpp>

#include <Types.hpp>

namespace hyperion {

class GpuBufferBase : public RenderObject<GpuBufferBase>
{
public:
    virtual ~GpuBufferBase() override = default;

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
    virtual RendererResult Destroy() = 0;

    virtual bool IsCreated() const = 0;
    virtual bool IsCpuAccessible() const = 0;

    virtual void InsertBarrier(CommandBufferBase* commandBuffer, ResourceState newState) const = 0;
    virtual void InsertBarrier(CommandBufferBase* commandBuffer, ResourceState newState, ShaderModuleType shaderType) const = 0;

    virtual void CopyFrom(
        CommandBufferBase* commandBuffer,
        const GpuBufferBase* srcBuffer,
        SizeType count) = 0;

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
          m_resourceState(RS_UNDEFINED)
    {
    }

    GpuBufferType m_type;
    SizeType m_size;
    SizeType m_alignment;

    mutable ResourceState m_resourceState;
};

} // namespace hyperion
