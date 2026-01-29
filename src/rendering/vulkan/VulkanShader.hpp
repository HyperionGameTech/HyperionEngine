/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/Shader.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <rendering/Shared.hpp>

#include <vulkan/vulkan.h>

namespace Hyperion {

struct VulkanShaderModule
{
    ShaderModuleType type = ShaderModuleType::None;

    String moduleName;
    String entryPointName;

    HashCode blobHashCode;

    VkShaderModule handle = VK_NULL_HANDLE;

    bool operator<(const VulkanShaderModule& other) const
    {
        return type < other.type;
    }

    bool IsRayTracing() const
    {
        return IsRayTracingShaderModule(type);
    }
};

struct VulkanShaderGroup
{
    ShaderModuleType type;
    VkRayTracingShaderGroupCreateInfoKHR rayTracingGroupCreateInfo;
};

HYP_CLASS(NoScriptBindings)
class VulkanShader final : public ShaderBase
{
    HYP_OBJECT_BODY(VulkanShader);

public:
    VulkanShader();
    explicit VulkanShader(const CompiledShader* compiledShader);
    ~VulkanShader() override;

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

    bool IsCreated() const override;

    RendererResult Create() override;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;

        for (const VulkanShaderModule& shaderModule : m_shaderModules)
        {
            hc.Add(uint32(shaderModule.type));
            hc.Add(shaderModule.moduleName);
            hc.Add(shaderModule.entryPointName);
            hc.Add(shaderModule.blobHashCode);
        }

        return hc;
    }

#ifdef HYP_DEBUG_MODE
    void SetDebugName(Name name) override;
#endif

private:
    RendererResult AttachShaderModules();
    RendererResult AttachShaderModule(
        ShaderModuleType type,
        UTF8StringView moduleName,
        UTF8StringView entryPointName,
        ConstByteView shaderBlobView);

    RendererResult CreateShaderGroups();

    VkPipelineShaderStageCreateInfo CreateShaderStage(const VulkanShaderModule&);

    Array<VulkanShaderModule> m_shaderModules;
    Array<VulkanShaderGroup> m_shaderGroups;

    Array<VkPipelineShaderStageCreateInfo> m_vkShaderStages;
};

} // namespace Hyperion
