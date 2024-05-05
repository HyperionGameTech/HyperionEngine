/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_BACKEND_VULKAN_SHADER_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_SHADER_HPP

namespace hyperion {
namespace renderer {

namespace platform {

template <>
struct ShaderModule<Platform::VULKAN>
{
    ShaderModuleType    type;
    String              entry_point_name;
    ByteBuffer          spirv;
    VkShaderModule      shader_module;

    ShaderModule(ShaderModuleType type, String entry_point_name)
        : type(type),
          entry_point_name(std::move(entry_point_name)),
          spirv { },
          shader_module { }
    {
    }

    ShaderModule(ShaderModuleType type, String entry_point_name, const ByteBuffer &spirv, VkShaderModule shader_module = nullptr)
        : type(type),
          entry_point_name(std::move(entry_point_name)),
          spirv(spirv),
          shader_module(shader_module)
    {
    }

    ShaderModule(const ShaderModule &other) = default;
    ~ShaderModule() = default;

    bool operator<(const ShaderModule &other) const
        { return type < other.type; }

    bool IsRaytracing() const
        { return IsRaytracingType(type); }

    static bool IsRaytracingType(ShaderModuleType type)
    {
        return type == ShaderModuleType::RAY_GEN
            || type == ShaderModuleType::RAY_INTERSECT
            || type == ShaderModuleType::RAY_ANY_HIT
            || type == ShaderModuleType::RAY_CLOSEST_HIT
            || type == ShaderModuleType::RAY_MISS;
    }
};

template <>
struct ShaderGroup<Platform::VULKAN>
{
    ShaderModuleType type;
    VkRayTracingShaderGroupCreateInfoKHR raytracing_group_create_info;
};

template <>
struct ShaderPlatformImpl<Platform::VULKAN>
{
    Shader<Platform::VULKAN>         *self = nullptr;

    Array<VkPipelineShaderStageCreateInfo>  vk_shader_stages;

    VkPipelineShaderStageCreateInfo CreateShaderStage(const ShaderModule<Platform::VULKAN> &);
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#endif //HYPERION_RENDERER_BACKEND_VULKAN_SHADER_HPP
