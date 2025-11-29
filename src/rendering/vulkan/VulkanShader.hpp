/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/RenderShader.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <vulkan/vulkan.h>

namespace hyperion {

struct VulkanShaderModule
{
    ShaderModuleType type;
    Name srcName;
    String entryPointName;
    ByteBuffer spirv;
    VkShaderModule handle;

    VulkanShaderModule(ShaderModuleType type, Name srcName, String entryPointName)
        : type(type),
          srcName(srcName),
          entryPointName(std::move(entryPointName)),
          spirv {},
          handle {}
    {
    }

    VulkanShaderModule(ShaderModuleType type, Name srcName, String entryPointName, const ByteBuffer& spirv, VkShaderModule handle = VK_NULL_HANDLE)
        : type(type),
          srcName(srcName),
          entryPointName(std::move(entryPointName)),
          spirv(spirv),
          handle(handle)
    {
    }

    VulkanShaderModule(const VulkanShaderModule& other) = default;
    ~VulkanShaderModule() = default;

    bool operator<(const VulkanShaderModule& other) const
    {
        return type < other.type;
    }

    bool IsRaytracing() const
    {
        return IsRaytracingShaderModule(type);
    }
};

struct VulkanShaderGroup
{
    ShaderModuleType type;
    VkRayTracingShaderGroupCreateInfoKHR raytracingGroupCreateInfo;
};

HYP_CLASS(NoScriptBindings)
class VulkanShader final : public ShaderBase
{
    HYP_OBJECT_BODY(VulkanShader);

public:
    VulkanShader();
    VulkanShader(const RC<CompiledShader>& compiledShader);
    virtual ~VulkanShader() override;

    HYP_FORCE_INLINE const String& GetEntryPointName() const
    {
        return m_entryPointName;
    }

    HYP_FORCE_INLINE const Array<VulkanShaderModule>& GetShaderModules() const
    {
        return m_shaderModules;
    }

    HYP_FORCE_INLINE const Array<VulkanShaderGroup>& GetShaderGroups() const
    {
        return m_shaderGroups;
    }

    HYP_FORCE_INLINE const Array<VkPipelineShaderStageCreateInfo>& GetVulkanShaderStages() const
    {
        return m_vkShaderStages;
    }

    virtual bool IsCreated() const override;

    virtual RendererResult Create() override;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;

        for (const VulkanShaderModule& shaderModule : m_shaderModules)
        {
            hc.Add(uint32(shaderModule.type));
            hc.Add(shaderModule.spirv.GetHashCode());
        }

        return hc;
    }

#ifdef HYP_DEBUG_MODE
    void SetDebugName(Name name) override;
#endif

private:
    RendererResult AttachSubShaders();
    RendererResult AttachSubShader(ShaderModuleType type, const ShaderObject& shaderObject);

    RendererResult CreateShaderGroups();

    VkPipelineShaderStageCreateInfo CreateShaderStage(const VulkanShaderModule&);

    String m_entryPointName;

    Array<VulkanShaderModule> m_shaderModules;
    Array<VulkanShaderGroup> m_shaderGroups;

    Array<VkPipelineShaderStageCreateInfo> m_vkShaderStages;
};

} // namespace hyperion
