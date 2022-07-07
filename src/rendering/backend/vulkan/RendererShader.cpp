#include "RendererShader.hpp"

#include <system/Debug.hpp>

#include <algorithm>

namespace hyperion {
namespace renderer {
ShaderProgram::ShaderProgram()
{
}

ShaderProgram::~ShaderProgram()
{
}

Result ShaderProgram::AttachShader(Device *device, ShaderModule::Type type, const ShaderObject &spirv)
{
    VkShaderModuleCreateInfo create_info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    create_info.codeSize = spirv.bytes.size();
    create_info.pCode    = reinterpret_cast<const uint32_t *>(spirv.bytes.data());

    VkShaderModule shader_module;

    HYPERION_VK_CHECK(vkCreateShaderModule(device->GetDevice(), &create_info, nullptr, &shader_module));

    m_shader_modules.emplace_back(type, spirv, shader_module);

    std::sort(m_shader_modules.begin(), m_shader_modules.end());

    HYPERION_RETURN_OK;
}

VkPipelineShaderStageCreateInfo
ShaderProgram::CreateShaderStage(const ShaderModule &shader_module)
{
    VkPipelineShaderStageCreateInfo create_info{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};

    create_info.module = shader_module.shader_module;
    create_info.pName  = entry_point;

    switch (shader_module.type) {
    case ShaderModule::Type::VERTEX:
        create_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
        break;
    case ShaderModule::Type::FRAGMENT:
        create_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        break;
    case ShaderModule::Type::GEOMETRY:
        create_info.stage = VK_SHADER_STAGE_GEOMETRY_BIT;
        break;
    case ShaderModule::Type::COMPUTE:
        create_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        break;
    case ShaderModule::Type::TASK:
        create_info.stage = VK_SHADER_STAGE_TASK_BIT_NV;
        break;
    case ShaderModule::Type::MESH:
        create_info.stage = VK_SHADER_STAGE_MESH_BIT_NV;
        break;
    case ShaderModule::Type::TESS_CONTROL:
        create_info.stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        break;
    case ShaderModule::Type::TESS_EVAL:
        create_info.stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        break;
    case ShaderModule::Type::RAY_GEN:
        create_info.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        break;
    case ShaderModule::Type::RAY_INTERSECT:
        create_info.stage = VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
        break;
    case ShaderModule::Type::RAY_ANY_HIT:
        create_info.stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
        break;
    case ShaderModule::Type::RAY_CLOSEST_HIT:
        create_info.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        break;
    case ShaderModule::Type::RAY_MISS:
        create_info.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
        break;
    default:
        HYP_THROW("Shader type " + std::to_string(int(shader_module.type)) + " is currently unimplemented!");
    }

    return create_info;
}

Result ShaderProgram::CreateShaderGroups()
{
    m_shader_groups.clear();

    for (size_t i = 0; i < m_shader_modules.size(); i++) {
        const auto &shader_module = m_shader_modules[i];
        
        switch (shader_module.type) {
        case ShaderModule::Type::RAY_MISS: /* fallthrough */
        case ShaderModule::Type::RAY_GEN:
            m_shader_groups.push_back({
                shader_module.type,
                VkRayTracingShaderGroupCreateInfoKHR{
                    .sType              = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                    .type               = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
                    .generalShader      = static_cast<uint32_t>(i),
                    .closestHitShader   = VK_SHADER_UNUSED_KHR,
                    .anyHitShader       = VK_SHADER_UNUSED_KHR,
                    .intersectionShader = VK_SHADER_UNUSED_KHR
                }
            });

            break;
        case ShaderModule::Type::RAY_CLOSEST_HIT:
            m_shader_groups.push_back({
                shader_module.type,
                VkRayTracingShaderGroupCreateInfoKHR{
                    .sType              = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                    .type               = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
                    .generalShader      = VK_SHADER_UNUSED_KHR,
                    .closestHitShader   = static_cast<uint32_t>(i),
                    .anyHitShader       = VK_SHADER_UNUSED_KHR,
                    .intersectionShader = VK_SHADER_UNUSED_KHR
                }
            });

            break;
        default:
            return {Result::RENDERER_ERR, "Unimplemented shader group type"};
        }
    }

    HYPERION_RETURN_OK;
}

Result ShaderProgram::Create(Device *device)
{
    bool is_raytracing = false;

    for (auto &shader_module : m_shader_modules) {
        is_raytracing = is_raytracing || shader_module.IsRaytracing();

        auto stage = CreateShaderStage(shader_module);

        m_shader_stages.push_back(stage);
    }

    if (is_raytracing) {
        HYPERION_BUBBLE_ERRORS(CreateShaderGroups());
    }

    HYPERION_RETURN_OK;
}

Result ShaderProgram::Destroy(Device *device)
{
    for (const auto &shader_module : m_shader_modules) {
        vkDestroyShaderModule(device->GetDevice(), shader_module.shader_module, nullptr);
    }

    HYPERION_RETURN_OK;
}

} // namespace renderer
} // namespace hyperion