/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <Core/Functional/Proc.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/Set.hpp>

#include <Rendering/RenderResult.hpp>
#include <Rendering/RenderTypes.hpp>
#include <Rendering/Shared.hpp>

namespace Hyperion {

HYP_CLASS(Abstract, NoScriptBindings)
class GpuBufferBase : public ObjectBase
{
    HYP_OBJECT_BODY(GpuBufferBase);

public:
    static Pool* GetAllocator()
    {
        return g_rhiPool;
    }

    virtual ~GpuBufferBase() override = default;

#ifdef HYP_RHI_DEBUG_NAMES
    Name GetDebugName() const
    {
        return m_debugName;
    }

    virtual void SetDebugName(Name name)
    {
        m_debugName = name;
    }
#else
    static constexpr NoOpFunction<Name> GetDebugName;
#endif

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

    HYP_FORCE_INLINE void SetIsCpuAccessible(bool cpuAccessible)
    {
        Assert(!IsCreated(), "Cannot set cpuAccessible after the buffer has been created!");

        m_cpuAccessible = cpuAccessible;
    }

    virtual void Flush(size_t offset, size_t count)
    {
    }

    virtual void Invalidate(size_t offset, size_t count)
    {
    }

    HYP_FORCE_INLINE void Invalidate()
    {
        Invalidate(0, m_size);
    }

    virtual void InsertBarrier(CommandBuffer* commandBuffer, ResourceState newState) const = 0;
    virtual void InsertBarrier(CommandBuffer* commandBuffer, ResourceState newState, ShaderModuleType shaderType) const = 0;

    virtual void CopyFrom(
        CommandBuffer* commandBuffer,
        const GpuBuffer* srcBuffer,
        uint32 count) = 0;

    virtual void CopyFrom(
        CommandBuffer* commandBuffer,
        const GpuBuffer* srcBuffer,
        uint32 srcOffset, uint32 dstOffset,
        uint32 count) = 0;

    virtual RendererResult EnsureCapacity(
        size_t minimumSize,
        bool* outSizeChanged = nullptr) = 0;

    virtual RendererResult EnsureCapacity(
        size_t minimumSize,
        size_t alignment,
        bool* outSizeChanged = nullptr) = 0;

    virtual void Memset(size_t count, ubyte value) = 0;

    virtual void Copy(size_t count, const void* ptr) = 0;
    virtual void Copy(size_t offset, size_t count, const void* ptr) = 0;

    virtual void Read(size_t count, void* outPtr) const = 0;
    virtual void Read(size_t offset, size_t count, void* outPtr) const = 0;

    virtual void* Map() const = 0;
    virtual void Unmap() const = 0;

protected:
    GpuBufferBase(GpuBufferType type, size_t size, size_t alignment = 0)
        : m_type(type),
          m_size(size),
          m_alignment(alignment),
          m_resourceState(RS_UNDEFINED),
          m_cpuAccessible(false)
    {
    }

    GpuBufferType m_type;
    size_t m_size;
    size_t m_alignment;

    mutable ResourceState m_resourceState;

#ifdef HYP_RHI_DEBUG_NAMES
    Name m_debugName;
#endif

    bool m_cpuAccessible : 1;
};

} // namespace Hyperion

#ifndef INCLUDE_FROM_RHI
#define INCLUDE_FROM_RHI_BASE

#if HYP_VULKAN
#include <Rendering/Vulkan/VulkanGpuBuffer.hpp>
#elif HYP_DX12
#include <Rendering/DX12/DX12GpuBuffer.hpp>
#endif

#undef INCLUDE_FROM_RHI_BASE
#else
#undef INCLUDE_FROM_RHI
#endif
