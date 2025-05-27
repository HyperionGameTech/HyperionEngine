/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_BACKEND_VULKAN_SHADER_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_SHADER_HPP

#include <rendering/backend/RendererShader.hpp>

namespace hyperion {
namespace renderer {

struct VulkanShaderModule
{
    ShaderModuleType type;
    String entry_point_name;
    ByteBuffer spirv;
    VkShaderModule shader_module;

    VulkanShaderModule(ShaderModuleType type, String entry_point_name)
        : type(type),
          entry_point_name(std::move(entry_point_name)),
          spirv {},
          shader_module {}
    {
    }

    VulkanShaderModule(ShaderModuleType type, String entry_point_name, const ByteBuffer& spirv, VkShaderModule shader_module = nullptr)
        : type(type),
          entry_point_name(std::move(entry_point_name)),
          spirv(spirv),
          shader_module(shader_module)
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
    VkRayTracingShaderGroupCreateInfoKHR raytracing_group_create_info;
};

class VulkanShader : public ShaderBase
{
public:
    HYP_API VulkanShader();
    HYP_API VulkanShader(const RC<CompiledShader>& compiled_shader);
    HYP_API virtual ~VulkanShader() override;

    HYP_FORCE_INLINE const String& GetEntryPointName() const
    {
        return m_entry_point_name;
    }

    HYP_FORCE_INLINE const Array<VulkanShaderModule>& GetShaderModules() const
    {
        return m_shader_modules;
    }

    HYP_FORCE_INLINE const Array<VulkanShaderGroup>& GetShaderGroups() const
    {
        return m_shader_groups;
    }

    HYP_FORCE_INLINE const Array<VkPipelineShaderStageCreateInfo>& GetVulkanShaderStages() const
    {
        return m_vk_shader_stages;
    }

    HYP_API virtual bool IsCreated() const override;

    HYP_API virtual RendererResult Create() override;
    HYP_API virtual RendererResult Destroy() override;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;

        for (const VulkanShaderModule& shader_module : m_shader_modules)
        {
            hc.Add(uint32(shader_module.type));
            hc.Add(shader_module.spirv.GetHashCode());
        }

        return hc;
    }

private:
    RendererResult AttachSubShaders();
    RendererResult AttachSubShader(ShaderModuleType type, const ShaderObject& shader_object);

    RendererResult CreateShaderGroups();

    VkPipelineShaderStageCreateInfo CreateShaderStage(const VulkanShaderModule&);

    String m_entry_point_name;

    Array<VulkanShaderModule> m_shader_modules;
    Array<VulkanShaderGroup> m_shader_groups;

    Array<VkPipelineShaderStageCreateInfo> m_vk_shader_stages;
};

} // namespace renderer
} // namespace hyperion

#endif // HYPERION_RENDERER_BACKEND_VULKAN_SHADER_HPP
