/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/RenderObject.hpp>
#include <core/Defines.hpp>

namespace Hyperion {

HYP_CLASS(Abstract, NoScriptBindings)
class RayTracingPipelineBase : public ObjectBase
{
    HYP_OBJECT_BODY(RayTracingPipelineBase);

public:
    virtual ~RayTracingPipelineBase() override = default;

    Name GetDebugName() const
    {
        return m_debugName;
    }

    virtual void SetDebugName(Name name)
    {
        m_debugName = name;
    }

    HYP_FORCE_INLINE const DescriptorTableRef& GetDescriptorTable() const
    {
        return m_descriptorTable;
    }

    HYP_FORCE_INLINE void SetDescriptorTable(const DescriptorTableRef& descriptorTable)
    {
        m_descriptorTable = descriptorTable;
    }

    HYP_FORCE_INLINE const ShaderRef& GetShader() const
    {
        return m_shader;
    }

    HYP_FORCE_INLINE void SetShader(const ShaderRef& shader)
    {
        m_shader = shader;
    }

    virtual bool IsCreated() const = 0;

    HYP_API virtual RendererResult Create() = 0;

    HYP_API virtual void Bind(CommandBuffer* commandBuffer) = 0;

    HYP_API virtual void TraceRays(
        CommandBuffer* commandBuffer,
        const Vec3u& extent) const = 0;

    // Deprecated - will be removed to decouple from vulkan
    HYP_DEPRECATED HYP_API virtual void SetPushConstants(const void* data, SizeType size) = 0;

    uint32 lastFrame = uint32(-1);

protected:
    RayTracingPipelineBase() = default;

    RayTracingPipelineBase(const ShaderRef& shader, const DescriptorTableRef& descriptorTable)
        : m_shader(shader),
          m_descriptorTable(descriptorTable)
    {
    }

    ShaderRef m_shader;
    DescriptorTableRef m_descriptorTable;

    Name m_debugName;
};

} // namespace Hyperion

#ifndef INCLUDE_FROM_RHI
#define INCLUDE_FROM_RHI_BASE

#if HYP_VULKAN
#include <rendering/vulkan/VulkanRayTracingPipeline.hpp>
#elif HYP_DX12
#include <rendering/dx12/DX12RayTracingPipeline.hpp>
#endif

#undef INCLUDE_FROM_RHI_BASE
#else
#undef INCLUDE_FROM_RHI
#endif
