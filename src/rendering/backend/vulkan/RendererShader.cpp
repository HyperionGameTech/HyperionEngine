/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/RendererShader.hpp>
#include <rendering/backend/RendererDescriptorSet2.hpp>

#include <system/Debug.hpp>

#include <algorithm>

namespace hyperion {
namespace renderer {
namespace platform {

ShaderProgram<Platform::VULKAN>::ShaderProgram()
    : m_entry_point_name("main")
{
}

ShaderProgram<Platform::VULKAN>::ShaderProgram(String entry_point_name)
    : m_entry_point_name(std::move(entry_point_name))
{
}

ShaderProgram<Platform::VULKAN>::~ShaderProgram()
{
}

Result ShaderProgram<Platform::VULKAN>::AttachShader(Device<Platform::VULKAN> *device, ShaderModuleType type, const ShaderObject &shader_object)
{
    const ByteBuffer &spirv = shader_object.bytes;

    VkShaderModuleCreateInfo create_info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    create_info.codeSize = spirv.Size();
    create_info.pCode = reinterpret_cast<const uint32 *>(spirv.Data());

    VkShaderModule shader_module;

    HYPERION_VK_CHECK(vkCreateShaderModule(device->GetDevice(), &create_info, nullptr, &shader_module));

    m_shader_modules.PushBack(ShaderModule<Platform::VULKAN>(type, m_entry_point_name, spirv, shader_module));

    std::sort(m_shader_modules.Begin(), m_shader_modules.End());

    HYPERION_RETURN_OK;
}

VkPipelineShaderStageCreateInfo
ShaderProgram<Platform::VULKAN>::CreateShaderStage(const ShaderModule<Platform::VULKAN> &shader_module)
{
    VkPipelineShaderStageCreateInfo create_info{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};

    create_info.module = shader_module.shader_module;
    create_info.pName = shader_module.entry_point_name.Data();

    switch (shader_module.type) {
    case ShaderModuleType::VERTEX:
        create_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
        break;
    case ShaderModuleType::FRAGMENT:
        create_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        break;
    case ShaderModuleType::GEOMETRY:
        create_info.stage = VK_SHADER_STAGE_GEOMETRY_BIT;
        break;
    case ShaderModuleType::COMPUTE:
        create_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        break;
    case ShaderModuleType::TASK:
        create_info.stage = VK_SHADER_STAGE_TASK_BIT_NV;
        break;
    case ShaderModuleType::MESH:
        create_info.stage = VK_SHADER_STAGE_MESH_BIT_NV;
        break;
    case ShaderModuleType::TESS_CONTROL:
        create_info.stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        break;
    case ShaderModuleType::TESS_EVAL:
        create_info.stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        break;
    case ShaderModuleType::RAY_GEN:
        create_info.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        break;
    case ShaderModuleType::RAY_INTERSECT:
        create_info.stage = VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
        break;
    case ShaderModuleType::RAY_ANY_HIT:
        create_info.stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
        break;
    case ShaderModuleType::RAY_CLOSEST_HIT:
        create_info.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        break;
    case ShaderModuleType::RAY_MISS:
        create_info.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
        break;
    default:
        HYP_THROW("Shader type " + std::to_string(int(shader_module.type)) + " is currently unimplemented!");
    }

    return create_info;
}

Result ShaderProgram<Platform::VULKAN>::CreateShaderGroups()
{
    m_shader_groups.Clear();

    for (SizeType i = 0; i < m_shader_modules.Size(); i++) {
        const auto &shader_module = m_shader_modules[i];
        
        switch (shader_module.type) {
        case ShaderModuleType::RAY_MISS: /* fallthrough */
        case ShaderModuleType::RAY_GEN:
            m_shader_groups.PushBack({
                shader_module.type,
                VkRayTracingShaderGroupCreateInfoKHR{
                    .sType              = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                    .type               = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
                    .generalShader      = uint32(i),
                    .closestHitShader   = VK_SHADER_UNUSED_KHR,
                    .anyHitShader       = VK_SHADER_UNUSED_KHR,
                    .intersectionShader = VK_SHADER_UNUSED_KHR
                }
            });

            break;
        case ShaderModuleType::RAY_CLOSEST_HIT:
            m_shader_groups.PushBack({
                shader_module.type,
                VkRayTracingShaderGroupCreateInfoKHR{
                    .sType              = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                    .type               = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
                    .generalShader      = VK_SHADER_UNUSED_KHR,
                    .closestHitShader   = uint32(i),
                    .anyHitShader       = VK_SHADER_UNUSED_KHR,
                    .intersectionShader = VK_SHADER_UNUSED_KHR
                }
            });

            break;
        default:
            return { Result::RENDERER_ERR, "Unimplemented shader group type" };
        }
    }

    HYPERION_RETURN_OK;
}

Result ShaderProgram<Platform::VULKAN>::Create(Device<Platform::VULKAN> *device)
{
    bool is_raytracing = false;

    for (const auto &shader_module : m_shader_modules) {
        is_raytracing = is_raytracing || shader_module.IsRaytracing();

        auto stage = CreateShaderStage(shader_module);

        m_shader_stages.PushBack(stage);
    }

    if (is_raytracing) {
        HYPERION_BUBBLE_ERRORS(CreateShaderGroups());
    }

    HYPERION_RETURN_OK;
}

Result ShaderProgram<Platform::VULKAN>::Destroy(Device<Platform::VULKAN> *device)
{
    for (const auto &shader_module : m_shader_modules) {
        vkDestroyShaderModule(device->GetDevice(), shader_module.shader_module, nullptr);
    }

    HYPERION_RETURN_OK;
}

} // namespace platform
} // namespace renderer
} // namespace hyperion