/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <VulkanPch.hpp>

#include <rendering/vulkan/VulkanShader.hpp>
#include <rendering/vulkan/VulkanDevice.hpp>
#include <rendering/vulkan/VulkanDescriptorSet.hpp>
#include <rendering/vulkan/VulkanRenderInterface.hpp>
#include <rendering/vulkan/VulkanResult.hpp>

#include <rendering/util/ShaderCompiler.hpp>

#include <core/debug/Debug.hpp>

#include <core/utilities/Format.hpp>

#include <engine/EngineDriver.hpp>

#include <algorithm>

#include <VulkanShader.generated.inl>

namespace Hyperion {

extern VulkanRenderInterface* g_renderInterface;

#pragma region CreateShaderStage

VulkanShader::VulkanShader()
    : ShaderBase()
{
}

VulkanShader::VulkanShader(const CompiledShader* compiledShader)
    : ShaderBase(compiledShader)
{
#ifdef HYP_DEBUG_MODE
    if (compiledShader != nullptr)
    {
        SetDebugName(compiledShader->name);
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
        vkDestroyShaderModule(g_renderInterface->GetDevice()->GetDevice(), shaderModule.handle, nullptr);
    }

    m_shaderModules.Clear();
}

bool VulkanShader::IsCreated() const
{
    return m_vkShaderStages.Size() != 0;
}

RendererResult VulkanShader::AttachShaderModule(
    ShaderModuleType type,
    UTF8StringView moduleName,
    UTF8StringView entryPointName,
    ConstByteView shaderBlobView)
{
    Assert(m_compiledShader != nullptr);
    Assert(shaderBlobView.Size() % sizeof(uint32) == 0);

    VkShaderModuleCreateInfo createInfo { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    createInfo.codeSize = shaderBlobView.Size();
    createInfo.pCode = reinterpret_cast<const uint32*>(shaderBlobView.Data());

    VkShaderModule vkShaderModule;
    VULKAN_CHECK(vkCreateShaderModule(g_renderInterface->GetDevice()->GetDevice(), &createInfo, nullptr, &vkShaderModule));

    VulkanShaderModule& shaderModule = m_shaderModules.EmplaceBack();
    shaderModule.type = type;
    shaderModule.moduleName = moduleName;
    shaderModule.entryPointName = entryPointName;
    shaderModule.blobHashCode = shaderBlobView.GetHashCode();
    shaderModule.handle = vkShaderModule;

    std::sort(m_shaderModules.Begin(), m_shaderModules.End());

    return {};
}

RendererResult VulkanShader::AttachShaderModules()
{
    if (!m_compiledShader)
    {
        return HYP_MAKE_ERROR(RendererError, "No compiled shader attached");
    }

    if (!m_compiledShader->IsValid())
    {
        return HYP_MAKE_ERROR(RendererError, "Attached compiled shader is in invalid state");
    }

    for (SizeType index = 0; index < m_compiledShader->moduleTypes.Size(); index++)
    {
#ifdef HYP_DEBUG_MODE
        const Name srcName = NAME_FMT("{}", m_compiledShader->name);
#else
        const Name srcName = NAME("<unnamed shader>");
#endif

        ShaderModuleType moduleType;
        String moduleName;
        String entryPointName;
        ConstByteView blob;

        if (!m_compiledShader->GetShaderModuleInfo(index, moduleType, moduleName, entryPointName, blob))
        {
            continue;
        }

        Assert(blob.Size() != 0);

        CheckResultOrReturn(AttachShaderModule(moduleType, moduleName, entryPointName, blob));
    }

    return {};
}

RendererResult VulkanShader::CreateShaderGroups()
{
    m_shaderGroups.Clear();

    for (SizeType i = 0; i < m_shaderModules.Size(); i++)
    {
        const VulkanShaderModule& shaderModule = m_shaderModules[i];

        switch (shaderModule.type)
        {
        case ShaderModuleType::Miss: /* fallthrough */
        case ShaderModuleType::RayGen:
            m_shaderGroups.PushBack({ shaderModule.type,
                VkRayTracingShaderGroupCreateInfoKHR {
                    .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                    .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
                    .generalShader = uint32(i),
                    .closestHitShader = VK_SHADER_UNUSED_KHR,
                    .anyHitShader = VK_SHADER_UNUSED_KHR,
                    .intersectionShader = VK_SHADER_UNUSED_KHR } });

            break;
        case ShaderModuleType::ClosestHit:
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

    return {};
}

VkPipelineShaderStageCreateInfo VulkanShader::CreateShaderStage(const VulkanShaderModule& shaderModule)
{
    VkPipelineShaderStageCreateInfo createInfo { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    createInfo.module = shaderModule.handle;
    createInfo.pName = shaderModule.entryPointName.Data();

    switch (shaderModule.type)
    {
    case ShaderModuleType::Vertex:
        createInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        break;
    case ShaderModuleType::Pixel:
        createInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        break;
    case ShaderModuleType::Geometry:
        createInfo.stage = VK_SHADER_STAGE_GEOMETRY_BIT;
        break;
    case ShaderModuleType::Compute:
        createInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        break;
    case ShaderModuleType::Task:
        createInfo.stage = VK_SHADER_STAGE_TASK_BIT_NV;
        break;
    case ShaderModuleType::Mesh:
        createInfo.stage = VK_SHADER_STAGE_MESH_BIT_NV;
        break;
    case ShaderModuleType::TessControl:
        createInfo.stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        break;
    case ShaderModuleType::TessEval:
        createInfo.stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        break;
    case ShaderModuleType::RayGen:
        createInfo.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        break;
    case ShaderModuleType::Intersect:
        createInfo.stage = VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
        break;
    case ShaderModuleType::AnyHit:
        createInfo.stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
        break;
    case ShaderModuleType::ClosestHit:
        createInfo.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        break;
    case ShaderModuleType::Miss:
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
        return {};
    }

    CheckResultOrReturn(AttachShaderModules());

    bool isRayTracing = false;

    for (const VulkanShaderModule& shaderModule : m_shaderModules)
    {
        isRayTracing |= shaderModule.IsRayTracing();

        m_vkShaderStages.PushBack(CreateShaderStage(shaderModule));
    }

    if (isRayTracing)
    {
        CheckResultOrReturn(CreateShaderGroups());
    }

#ifdef HYP_DEBUG_MODE
    if (Name debugName = GetDebugName())
    {
        SetDebugName(debugName);
    }
#endif

    return {};
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

        VkDebugUtilsObjectNameInfoEXT objectNameInfo { VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
        objectNameInfo.objectType = VK_OBJECT_TYPE_SHADER_MODULE;
        objectNameInfo.objectHandle = (uint64)shaderModule.handle;
        objectNameInfo.pObjectName = shaderModule.moduleName.Data();

        g_vulkanDynamicFunctions->vkSetDebugUtilsObjectNameEXT(g_renderInterface->GetDevice()->GetDevice(), &objectNameInfo);
    }
}

#endif

#pragma endregion VulkanShader

} // namespace Hyperion
