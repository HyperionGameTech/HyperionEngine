/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Rendering/Device.hpp>
#include <Rendering/GpuBuffer.hpp>

#include <Core/reflection/ObjectBase.hpp>
#include <Core/reflection/Handle.hpp>

#include <Core/Defines.hpp>

namespace Hyperion {

HYP_CLASS(Abstract, NoScriptBindings)
class CommandBufferBase : public ObjectBase
{
    HYP_OBJECT_BODY(CommandBufferBase);

public:
    virtual ~CommandBufferBase() override = default;

    static Pool* GetAllocator() { return g_rhiPool; }

    virtual bool IsCreated() const = 0;

    virtual RendererResult Create() = 0;

    virtual bool IsRecording() const = 0;

    virtual void Begin() = 0;
    virtual void End() = 0;

    virtual void BindVertexBuffer(const GpuBuffer* buffer) = 0;
    virtual void BindIndexBuffer(const GpuBuffer* buffer, GpuElemType elemType = GET_UNSIGNED_INT) = 0;

    virtual void DrawIndexed(
        uint32 numIndices,
        uint32 numInstances = 1,
        uint32 instanceIndex = 0) const = 0;

    virtual void DrawIndexedIndirect(
        const GpuBuffer* buffer,
        uint32 bufferOffset) const = 0;

#if HYP_DEBUG_MODE
    HYP_FORCE_INLINE Name GetDebugName() const
    {
        return m_debugName;
    }

    virtual void SetDebugName(const Name& name)
    {
        m_debugName = name;
    }
#endif

protected:
#if HYP_DEBUG_MODE
    Name m_debugName;
#endif
};

} // namespace Hyperion

#ifndef INCLUDE_FROM_RHI
#define INCLUDE_FROM_RHI_BASE

#if HYP_VULKAN
#include <Rendering/vulkan/VulkanCommandBuffer.hpp>
#elif HYP_DX12
#include <Rendering/dx12/DX12CommandBuffer.hpp>
#endif

#undef INCLUDE_FROM_RHI_BASE
#else
#undef INCLUDE_FROM_RHI
#endif
