/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/vulkan/VulkanShader.hpp>
#include <rendering/vulkan/VulkanDevice.hpp>
#include <rendering/vulkan/VulkanDescriptorSet.hpp>
#include <rendering/vulkan/VulkanRenderBackend.hpp>

#include <rendering/shader_compiler/ShaderCompiler.hpp>

#include <core/debug/Debug.hpp>

#include <core/utilities/Format.hpp>

#include <engine/EngineDriver.hpp>

#include <algorithm>

namespace hyperion {

extern IRenderBackend* g_renderBackend;

static inline VulkanRenderBackend* GetRenderBackend()
{
    return static_cast<VulkanRenderBackend*>(g_renderBackend);
}

#pragma region CreateShaderStage

VulkanShader::VulkanShader()
    : ShaderBase(),
      m_entryPointName("main")
{
}

VulkanShader::VulkanShader(const RC<CompiledShader>& compiledShader)
    : ShaderBase(compiledShader),
      m_entryPointName("main")
{
#ifdef HYP_DEBUG_MODE
    if (compiledShader != nullptr)
    {
        SetDebugName(compiledShader->GetName());
    }
#endif
}

VulkanShader::~VulkanShader()
{
    if (!IsCreated())
    {
        return;
    }

    for (const VulkanShaderModule& shaderModule : m_shaderModules)
    {
        vkDestroyShaderModule(GetRenderBackend()->GetDevice()->GetDevice(), shaderModule.handle, nullptr);
    }

    m_shaderModules.Clear();
}

bool VulkanShader::IsCreated() const
{
    return m_vkShaderStages.Size() != 0;
}

RendererResult VulkanShader::AttachSubShader(ShaderModuleType type, const ShaderObject& shaderObject)
{
    const ByteBuffer& spirv = shaderObject.bytes;

    VkShaderModuleCreateInfo createInfo { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    createInfo.codeSize = spirv.Size();
    createInfo.pCode = reinterpret_cast<const uint32*>(spirv.Data());

    VkShaderModule shaderModule;

    VULKAN_CHECK(vkCreateShaderModule(GetRenderBackend()->GetDevice()->GetDevice(), &createInfo, nullptr, &shaderModule));

    m_shaderModules.EmplaceBack(type, shaderObject.srcName, m_entryPointName, spirv, shaderModule);

    std::sort(m_shaderModules.Begin(), m_shaderModules.End());

    HYPERION_RETURN_OK;
}

RendererResult VulkanShader::AttachSubShaders()
{
    if (!m_compiledShader)
    {
        return HYP_MAKE_ERROR(RendererError, "No compiled shader attached");
    }

    if (!m_compiledShader->IsValid())
    {
        return HYP_MAKE_ERROR(RendererError, "Attached compiled shader is in invalid state");
    }

    for (SizeType index = 0; index < m_compiledShader->modules.Size(); index++)
    {
#ifdef HYP_DEBUG_MODE
        const Name srcName = NAME_FMT("{} ({})", m_compiledShader->GetName(), m_compiledShader->GetDefinition().GetProperties().ToString(/* includeVertexAttributes */ false));
#else
        const Name srcName = NAME("<unnamed shader>");
#endif

        ByteBuffer byteBuffer = m_compiledShader->modules[index];

        if (byteBuffer.Empty())
        {
            continue;
        }

        // since we reinterpret it as uint32 ptr we need to make sure it is aligned as uint32
        byteBuffer.SetSize(ByteUtil::AlignAs(byteBuffer.Size(), alignof(uint32)));

        HYP_GFX_CHECK(AttachSubShader(ShaderModuleType(index), ShaderObject { srcName, std::move(byteBuffer) }));
    }

    HYPERION_RETURN_OK;
}

RendererResult VulkanShader::CreateShaderGroups()
{
    m_shaderGroups.Clear();

    for (SizeType i = 0; i < m_shaderModules.Size(); i++)
    {
        const VulkanShaderModule& shaderModule = m_shaderModules[i];

        switch (shaderModule.type)
        {
        case SMT_RAY_MISS: /* fallthrough */
        case SMT_RAY_GEN:
            m_shaderGroups.PushBack({ shaderModule.type,
                VkRayTracingShaderGroupCreateInfoKHR {
                    .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                    .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
                    .generalShader = uint32(i),
                    .closestHitShader = VK_SHADER_UNUSED_KHR,
                    .anyHitShader = VK_SHADER_UNUSED_KHR,
                    .intersectionShader = VK_SHADER_UNUSED_KHR } });

            break;
        case SMT_RAY_CLOSEST_HIT:
            m_shaderGroups.PushBack({ shaderModule.type,
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

VkPipelineShaderStageCreateInfo VulkanShader::CreateShaderStage(const VulkanShaderModule& shaderModule)
{
    VkPipelineShaderStageCreateInfo createInfo { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    createInfo.module = shaderModule.handle;
    createInfo.pName = shaderModule.entryPointName.Data();

    switch (shaderModule.type)
    {
    case SMT_VERTEX:
        createInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        break;
    case SMT_FRAGMENT:
        createInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        break;
    case SMT_GEOMETRY:
        createInfo.stage = VK_SHADER_STAGE_GEOMETRY_BIT;
        break;
    case SMT_COMPUTE:
        createInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        break;
    case SMT_TASK:
        createInfo.stage = VK_SHADER_STAGE_TASK_BIT_NV;
        break;
    case SMT_MESH:
        createInfo.stage = VK_SHADER_STAGE_MESH_BIT_NV;
        break;
    case SMT_TESS_CONTROL:
        createInfo.stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        break;
    case SMT_TESS_EVAL:
        createInfo.stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        break;
    case SMT_RAY_GEN:
        createInfo.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        break;
    case SMT_RAY_INTERSECT:
        createInfo.stage = VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
        break;
    case SMT_RAY_ANY_HIT:
        createInfo.stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
        break;
    case SMT_RAY_CLOSEST_HIT:
        createInfo.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        break;
    case SMT_RAY_MISS:
        createInfo.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
        break;
    default:
        HYP_THROW("Not implemented");
    }

    return createInfo;
}

RendererResult VulkanShader::Create()
{
    if (IsCreated())
    {
        HYPERION_RETURN_OK;
    }

    HYP_GFX_CHECK(AttachSubShaders());

    bool isRaytracing = false;

    for (const VulkanShaderModule& shaderModule : m_shaderModules)
    {
        isRaytracing |= shaderModule.IsRaytracing();

        m_vkShaderStages.PushBack(CreateShaderStage(shaderModule));
    }

    if (isRaytracing)
    {
        HYP_GFX_CHECK(CreateShaderGroups());
    }

#ifdef HYP_DEBUG_MODE
    if (Name debugName = GetDebugName())
    {
        SetDebugName(debugName);
    }
#endif

    HYPERION_RETURN_OK;
}

#ifdef HYP_DEBUG_MODE

void VulkanShader::SetDebugName(Name name)
{
    ShaderBase::SetDebugName(name);

    if (!IsCreated())
    {
        return;
    }

    for (VulkanShaderModule& shaderModule : m_shaderModules)
    {
        if (!shaderModule.handle)
        {
            continue;
        }

        String moduleName = shaderModule.srcName.IsValid() ? shaderModule.srcName.LookupString() : name.LookupString();

        VkDebugUtilsObjectNameInfoEXT objectNameInfo { VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
        objectNameInfo.objectType = VK_OBJECT_TYPE_SHADER_MODULE;
        objectNameInfo.objectHandle = (uint64)shaderModule.handle;
        objectNameInfo.pObjectName = moduleName.Data();

        g_vulkanDynamicFunctions->vkSetDebugUtilsObjectNameEXT(GetRenderBackend()->GetDevice()->GetDevice(), &objectNameInfo);
    }
}

#endif

#pragma endregion VulkanShader

} // namespace hyperion
