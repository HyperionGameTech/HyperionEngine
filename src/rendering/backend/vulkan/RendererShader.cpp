/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/vulkan/RendererShader.hpp>
#include <rendering/backend/vulkan/RendererDescriptorSet.hpp>
#include <rendering/backend/vulkan/VulkanRenderingAPI.hpp>

#include <rendering/shader_compiler/ShaderCompiler.hpp>

#include <core/debug/Debug.hpp>

#include <Engine.hpp>

#include <algorithm>

namespace hyperion {

extern IRenderingAPI* g_rendering_api;

static inline VulkanRenderingAPI* GetRenderingAPI()
{
    return static_cast<VulkanRenderingAPI*>(g_rendering_api);
}

#pragma region CreateShaderStage

VulkanShader::VulkanShader(const RC<CompiledShader>& compiled_shader)
    : ShaderBase(compiled_shader),
      m_entry_point_name("main")
{
}

VulkanShader::~VulkanShader()
{
}

bool VulkanShader::IsCreated() const
{
    return m_vk_shader_stages.Size() != 0;
}

RendererResult VulkanShader::AttachSubShader(ShaderModuleType type, const ShaderObject& shader_object)
{
    const ByteBuffer& spirv = shader_object.bytes;

    VkShaderModuleCreateInfo create_info { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    create_info.codeSize = spirv.Size();
    create_info.pCode = reinterpret_cast<const uint32*>(spirv.Data());

    VkShaderModule shader_module;

    HYPERION_VK_CHECK(vkCreateShaderModule(GetRenderingAPI()->GetDevice()->GetDevice(), &create_info, nullptr, &shader_module));

    m_shader_modules.PushBack(VulkanShaderModule(type, m_entry_point_name, spirv, shader_module));

    std::sort(m_shader_modules.Begin(), m_shader_modules.End());

    HYPERION_RETURN_OK;
}

RendererResult VulkanShader::AttachSubShaders()
{
    if (!m_compiled_shader)
    {
        return HYP_MAKE_ERROR(RendererError, "No compiled shader attached");
    }

    if (!m_compiled_shader->IsValid())
    {
        return HYP_MAKE_ERROR(RendererError, "Attached compiled shader is in invalid state");
    }

    for (SizeType index = 0; index < m_compiled_shader->modules.Size(); index++)
    {
        const ByteBuffer& byte_buffer = m_compiled_shader->modules[index];

        if (byte_buffer.Empty())
        {
            continue;
        }

        HYPERION_BUBBLE_ERRORS(AttachSubShader(
            ShaderModuleType(index),
            { byte_buffer }));
    }

    HYPERION_RETURN_OK;
}

RendererResult VulkanShader::CreateShaderGroups()
{
    m_shader_groups.Clear();

    for (SizeType i = 0; i < m_shader_modules.Size(); i++)
    {
        const VulkanShaderModule& shader_module = m_shader_modules[i];

        switch (shader_module.type)
        {
        case SMT_RAY_MISS: /* fallthrough */
        case SMT_RAY_GEN:
            m_shader_groups.PushBack({ shader_module.type,
                VkRayTracingShaderGroupCreateInfoKHR {
                    .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                    .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
                    .generalShader = uint32(i),
                    .closestHitShader = VK_SHADER_UNUSED_KHR,
                    .anyHitShader = VK_SHADER_UNUSED_KHR,
                    .intersectionShader = VK_SHADER_UNUSED_KHR } });

            break;
        case SMT_RAY_CLOSEST_HIT:
            m_shader_groups.PushBack({ shader_module.type,
                VkRayTracingShaderGroupCreateInfoKHR {
                    .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                    .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
                    .generalShader = VK_SHADER_UNUSED_KHR,
                    .closestHitShader = uint32(i),
                    .anyHitShader = VK_SHADER_UNUSED_KHR,
                    .intersectionShader = VK_SHADER_UNUSED_KHR } });

            break;
        default:
            return HYP_MAKE_ERROR(RendererError, "Unimplemented shader group type");
        }
    }

    HYPERION_RETURN_OK;
}

VkPipelineShaderStageCreateInfo VulkanShader::CreateShaderStage(const VulkanShaderModule& shader_module)
{
    VkPipelineShaderStageCreateInfo create_info { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    create_info.module = shader_module.shader_module;
    create_info.pName = shader_module.entry_point_name.Data();

    switch (shader_module.type)
    {
    case SMT_VERTEX:
        create_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
        break;
    case SMT_FRAGMENT:
        create_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        break;
    case SMT_GEOMETRY:
        create_info.stage = VK_SHADER_STAGE_GEOMETRY_BIT;
        break;
    case SMT_COMPUTE:
        create_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        break;
    case SMT_TASK:
        create_info.stage = VK_SHADER_STAGE_TASK_BIT_NV;
        break;
    case SMT_MESH:
        create_info.stage = VK_SHADER_STAGE_MESH_BIT_NV;
        break;
    case SMT_TESS_CONTROL:
        create_info.stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        break;
    case SMT_TESS_EVAL:
        create_info.stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        break;
    case SMT_RAY_GEN:
        create_info.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        break;
    case SMT_RAY_INTERSECT:
        create_info.stage = VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
        break;
    case SMT_RAY_ANY_HIT:
        create_info.stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
        break;
    case SMT_RAY_CLOSEST_HIT:
        create_info.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        break;
    case SMT_RAY_MISS:
        create_info.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
        break;
    default:
        HYP_THROW("Not implemented");
    }

    return create_info;
}

RendererResult VulkanShader::Create()
{
    if (IsCreated())
    {
        HYPERION_RETURN_OK;
    }

    HYPERION_BUBBLE_ERRORS(AttachSubShaders());

    bool is_raytracing = false;

    for (const VulkanShaderModule& shader_module : m_shader_modules)
    {
        is_raytracing |= shader_module.IsRaytracing();

        m_vk_shader_stages.PushBack(CreateShaderStage(shader_module));
    }

    if (is_raytracing)
    {
        HYPERION_BUBBLE_ERRORS(CreateShaderGroups());
    }

    HYPERION_RETURN_OK;
}

RendererResult VulkanShader::Destroy()
{
    if (!IsCreated())
    {
        HYPERION_RETURN_OK;
    }

    for (const VulkanShaderModule& shader_module : m_shader_modules)
    {
        vkDestroyShaderModule(GetRenderingAPI()->GetDevice()->GetDevice(), shader_module.shader_module, nullptr);
    }

    m_shader_modules.Clear();

    HYPERION_RETURN_OK;
}

#pragma endregion VulkanShader

} // namespace hyperion