/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/RenderObject.hpp>

#include <core/Defines.hpp>

namespace hyperion {
class ComputePipelineBase : public RenderObject<ComputePipelineBase>
{
public:
    virtual ~ComputePipelineBase() override = default;

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

    virtual RendererResult Create() = 0;
    virtual RendererResult Destroy() = 0;

    virtual void Bind(CommandBufferBase* commandBuffer) = 0;

    virtual void Dispatch(CommandBufferBase* commandBuffer, const Vec3u& groupSize) const = 0;
    virtual void DispatchIndirect(
        CommandBufferBase* commandBuffer,
        const GpuBufferRef& indirectBuffer,
        SizeType offset = 0) const = 0;

    // Deprecated - will be removed to decouple from vulkan
    HYP_DEPRECATED virtual void SetPushConstants(const void* data, SizeType size) = 0;

protected:
    ComputePipelineBase() = default;

    ComputePipelineBase(const ShaderRef& shader, const DescriptorTableRef& descriptorTable)
        : m_shader(shader),
          m_descriptorTable(descriptorTable)
    {
    }

    ShaderRef m_shader;
    DescriptorTableRef m_descriptorTable;
};

} // namespace hyperion
