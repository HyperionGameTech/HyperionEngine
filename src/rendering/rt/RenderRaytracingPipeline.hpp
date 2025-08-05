/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/RenderObject.hpp>
#include <core/Defines.hpp>

namespace hyperion {
class RaytracingPipelineBase : public RenderObject<RaytracingPipelineBase>
{
public:
    virtual ~RaytracingPipelineBase() override = default;

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
    HYP_API virtual RendererResult Destroy() = 0;

    HYP_API virtual void Bind(CommandBufferBase* commandBuffer) = 0;

    HYP_API virtual void TraceRays(
        CommandBufferBase* commandBuffer,
        const Vec3u& extent) const = 0;

    // Deprecated - will be removed to decouple from vulkan
    HYP_DEPRECATED HYP_API virtual void SetPushConstants(const void* data, SizeType size) = 0;

protected:
    RaytracingPipelineBase() = default;

    RaytracingPipelineBase(const ShaderRef& shader, const DescriptorTableRef& descriptorTable)
        : m_shader(shader),
          m_descriptorTable(descriptorTable)
    {
    }

    ShaderRef m_shader;
    DescriptorTableRef m_descriptorTable;
};

} // namespace hyperion
