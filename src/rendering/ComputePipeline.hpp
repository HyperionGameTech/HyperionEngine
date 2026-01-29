/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <rendering/RenderObject.hpp>

#include <core/reflection/ObjectBase.hpp>
#include <core/reflection/Handle.hpp>

namespace Hyperion {

HYP_CLASS(Abstract, NoScriptBindings)
class ComputePipelineBase : public ObjectBase
{
    HYP_OBJECT_BODY(ComputePipelineBase);

public:
    virtual ~ComputePipelineBase() override = default;

    HYP_FORCE_INLINE const ShaderRef& GetShader() const
    {
        return m_shader;
    }

    HYP_FORCE_INLINE void SetShader(const ShaderRef& shader)
    {
        m_shader = shader;
    }

    Name GetDebugName() const
    {
        return m_debugName;
    }

    virtual void SetDebugName(Name name)
    {
        m_debugName = name;
    }

    virtual bool IsCreated() const = 0;

    virtual RendererResult Create() = 0;

    virtual void Bind(CommandBuffer* commandBuffer) = 0;

    virtual void Dispatch(CommandBuffer* commandBuffer, const Vec3u& groupSize) const = 0;
    virtual void DispatchIndirect(
        CommandBuffer* commandBuffer,
        const GpuBufferRef& indirectBuffer,
        SizeType offset = 0) const = 0;

    // Deprecated - will be removed to decouple from vulkan
    HYP_DEPRECATED virtual void SetPushConstants(const void* data, SizeType size) = 0;
    
    bool MatchesSignature(const ShaderDesc& shaderDesc) const;

    uint32 lastFrame = uint32(-1);

protected:
    ComputePipelineBase() = default;

    explicit ComputePipelineBase(const ShaderRef& shader)
        : m_shader(shader)
    {
    }

    ShaderRef m_shader;

    Name m_debugName;
};

} // namespace Hyperion

#ifndef INCLUDE_FROM_RHI
#define INCLUDE_FROM_RHI_BASE

#if HYP_VULKAN
#include <rendering/vulkan/VulkanComputePipeline.hpp>
#elif HYP_DX12
#include <rendering/dx12/DX12ComputePipeline.hpp>
#endif

#undef INCLUDE_FROM_RHI_BASE
#else
#undef INCLUDE_FROM_RHI
#endif
