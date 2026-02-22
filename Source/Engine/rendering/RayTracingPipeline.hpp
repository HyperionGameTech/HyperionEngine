/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/RenderObject.hpp>
#include <core/Defines.hpp>

namespace Hyperion {

struct ShaderDesc;

HYP_CLASS(Abstract, NoScriptBindings)
class RayTracingPipelineBase : public ObjectBase
{
    HYP_OBJECT_BODY(RayTracingPipelineBase);

public:
    static Pool* GetAllocator() { return g_rhiPool; }
    
    virtual ~RayTracingPipelineBase() override = default;

    Name GetDebugName() const
    {
        return m_debugName;
    }

    virtual void SetDebugName(Name name)
    {
        m_debugName = name;
    }

    HYP_FORCE_INLINE const ShaderInstanceRef& GetShader() const
    {
        return m_shaderInstance;
    }

    HYP_FORCE_INLINE void SetShader(const ShaderInstanceRef& shaderInstance)
    {
        m_shaderInstance = shaderInstance;
    }

    virtual bool IsCreated() const = 0;

    HYP_API virtual RendererResult Create() = 0;

    HYP_API virtual void Bind(CommandBuffer* commandBuffer) = 0;

    HYP_API virtual void TraceRays(
        CommandBuffer* commandBuffer,
        const Vec3u& extent) const = 0;

    // Deprecated - will be removed to decouple from vulkan
    HYP_DEPRECATED HYP_API virtual void SetPushConstants(const void* data, SizeType size) = 0;

    bool MatchesSignature(const ShaderDesc& shaderDesc) const;
    
    uint32 lastFrame = uint32(-1);

protected:
    RayTracingPipelineBase() = default;

    explicit RayTracingPipelineBase(const ShaderInstanceRef& shaderInstance)
        : m_shaderInstance(shaderInstance)
    {
    }

    ShaderInstanceRef m_shaderInstance;

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
