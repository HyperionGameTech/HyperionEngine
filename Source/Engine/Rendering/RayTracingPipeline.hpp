/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Rendering/RenderTypes.hpp>
#include <Rendering/RenderResult.hpp>
#include <Rendering/RenderMemory.hpp>

#include <Core/Defines.hpp>

namespace Hyperion {

struct ShaderDesc;

HYP_CLASS(Abstract, NoScriptBindings)
class RayTracingPipelineBase : public ObjectBase
{
    HYP_OBJECT_BODY(RayTracingPipelineBase);

public:
    static Pool* GetAllocator() { return g_rhiPool; }

    virtual ~RayTracingPipelineBase() override = default;

#if HYP_DEBUG_MODE
    Name GetDebugName() const
    {
        return m_debugName;
    }

    virtual void SetDebugName(Name name)
    {
        m_debugName = name;
    }
#endif

    HYP_FORCE_INLINE const ShaderInstanceRef& GetShader() const
    {
        return m_shaderInstance;
    }

    HYP_FORCE_INLINE void SetShader(const ShaderInstanceRef& shaderInstance)
    {
        m_shaderInstance = shaderInstance;
    }

    virtual bool IsCreated() const = 0;

    virtual RendererResult Create() = 0;

    virtual void Bind(CommandBuffer* commandBuffer) = 0;

    virtual void TraceRays(
        CommandBuffer* commandBuffer,
        const Vec3u& extent) const = 0;

    bool MatchesSignature(const ShaderDesc& shaderDesc) const;

    uint32 lastFrame = uint32(-1);

protected:
    RayTracingPipelineBase() = default;

    explicit RayTracingPipelineBase(const ShaderInstanceRef& shaderInstance)
        : m_shaderInstance(shaderInstance)
    {
    }

    ShaderInstanceRef m_shaderInstance;

#if HYP_DEBUG_MODE
    Name m_debugName;
#endif
};

} // namespace Hyperion

#ifndef INCLUDE_FROM_RHI
#define INCLUDE_FROM_RHI_BASE

#if HYP_VULKAN
#include <Rendering/Vulkan/VulkanRayTracingPipeline.hpp>
#elif HYP_DX12
#include <Rendering/DX12/DX12RayTracingPipeline.hpp>
#endif

#undef INCLUDE_FROM_RHI_BASE
#else
#undef INCLUDE_FROM_RHI
#endif
