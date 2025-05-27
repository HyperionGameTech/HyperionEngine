/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_BACKEND_RENDERER_COMPUTE_PIPELINE_HPP
#define HYPERION_BACKEND_RENDERER_COMPUTE_PIPELINE_HPP

#include <rendering/backend/RenderObject.hpp>

#include <core/Defines.hpp>

namespace hyperion {
namespace renderer {

class ComputePipelineBase : public RenderObject<ComputePipelineBase>
{
public:
    virtual ~ComputePipelineBase() override = default;

    HYP_FORCE_INLINE const DescriptorTableRef& GetDescriptorTable() const
    {
        return m_descriptor_table;
    }

    HYP_FORCE_INLINE void SetDescriptorTable(const DescriptorTableRef& descriptor_table)
    {
        m_descriptor_table = descriptor_table;
    }

    HYP_FORCE_INLINE const ShaderRef& GetShader() const
    {
        return m_shader;
    }

    HYP_FORCE_INLINE void SetShader(const ShaderRef& shader)
    {
        m_shader = shader;
    }

    HYP_API virtual RendererResult Create() = 0;
    HYP_API virtual RendererResult Destroy() = 0;

    HYP_API virtual void Bind(CommandBufferBase* command_buffer) = 0;

    HYP_API virtual void Dispatch(CommandBufferBase* command_buffer, const Vec3u& group_size) const = 0;
    HYP_API virtual void DispatchIndirect(
        CommandBufferBase* command_buffer,
        const GPUBufferRef& indirect_buffer,
        SizeType offset = 0) const = 0;

    // Deprecated - will be removed to decouple from vulkan
    HYP_DEPRECATED HYP_API virtual void SetPushConstants(const void* data, SizeType size) = 0;

protected:
    ComputePipelineBase() = default;

    ComputePipelineBase(const ShaderRef& shader, const DescriptorTableRef& descriptor_table)
        : m_shader(shader),
          m_descriptor_table(descriptor_table)
    {
    }

    ShaderRef m_shader;
    DescriptorTableRef m_descriptor_table;
};

} // namespace renderer
} // namespace hyperion

#endif