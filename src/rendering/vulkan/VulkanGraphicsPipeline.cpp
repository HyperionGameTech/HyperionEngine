/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <VulkanPch.hpp>

#include <rendering/vulkan/VulkanGraphicsPipeline.hpp>
#include <rendering/vulkan/VulkanRenderPass.hpp>
#include <rendering/vulkan/VulkanFramebuffer.hpp>
#include <rendering/vulkan/VulkanShader.hpp>
#include <rendering/vulkan/VulkanDevice.hpp>
#include <rendering/vulkan/VulkanFeatures.hpp>
#include <rendering/vulkan/VulkanRenderBackend.hpp>

#include <rendering/RenderInterface.hpp>

#include <rendering/util/SafeDeleter.hpp>
#include <rendering/util/ShaderCompiler.hpp> // For CompiledShader

#include <core/debug/Debug.hpp>

#include <core/math/MathUtil.hpp>
#include <core/math/Transform.hpp>

#include <cstring>

#include <VulkanGraphicsPipeline.generated.inl>

namespace Hyperion {

extern VulkanRenderBackend* g_renderBackend;

template <>
Array<VkDescriptorSetLayout> GetVkDescriptorSetLayouts<VulkanGraphicsPipeline>(const VulkanGraphicsPipeline& pipeline)
{
    Array<VkDescriptorSetLayout> usedLayouts;

    VulkanShader* shader = pipeline.GetShader();
    AssertDebug(shader != nullptr && shader->GetCompiledShader() != nullptr);

    const DescriptorTableDeclaration* decl = shader->GetCompiledShader()->GetDescriptorTableDeclaration();
    Assert(decl != nullptr);

    for (const DescriptorSetDeclaration& setDecl : decl->elements)
    {
        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
        Assert(g_renderBackend->GetOrCreateVkDescriptorSetLayout(DescriptorSetLayout(&setDecl), layout));

        Assert(layout != VK_NULL_HANDLE);

        usedLayouts.PushBack(layout);
    }

    return usedLayouts;
}

#pragma region GraphicsPipeline

VulkanGraphicsPipeline::VulkanGraphicsPipeline()
    : VulkanPipelineBase(),
      GraphicsPipelineBase()
{
}

VulkanGraphicsPipeline::VulkanGraphicsPipeline(const VulkanShaderRef& shader)
    : VulkanPipelineBase(),
      GraphicsPipelineBase(shader)
{
}

VulkanGraphicsPipeline::~VulkanGraphicsPipeline()
{
}

void VulkanGraphicsPipeline::Bind(VulkanCommandBuffer* cmd)
{
    Vec2i viewportOffset = Vec2i::Zero();
    Vec2u viewportExtent = Vec2u::One();

    viewportExtent = m_renderTargetDesc.extent;

    Bind(cmd, viewportOffset, viewportExtent);
}

void VulkanGraphicsPipeline::Bind(VulkanCommandBuffer* commandBuffer, Vec2i viewportOffset, Vec2u viewportExtent)
{
    HYP_GFX_ASSERT(m_handle != VK_NULL_HANDLE);

    VulkanCommandBuffer* vulkanCommandBuffer = commandBuffer;

    vulkanCommandBuffer->ResetBoundDescriptorSets();

    vkCmdBindPipeline(
        vulkanCommandBuffer->GetVulkanHandle(),
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        VulkanPipelineBase::m_handle);

    if (viewportExtent != Vec2u::Zero())
    {
        Viewport viewport;
        viewport.position = viewportOffset;
        viewport.extent = viewportExtent;

        UpdateViewport(vulkanCommandBuffer, viewport);
    }

    if (m_pushConstants)
    {
        vkCmdPushConstants(
            vulkanCommandBuffer->GetVulkanHandle(),
            VulkanPipelineBase::m_layout,
            VK_SHADER_STAGE_ALL_GRAPHICS,
            0,
            m_pushConstants.Size(),
            m_pushConstants.Data());
    }

    if (m_stencilWrite || m_stencilFunction.HasValue())
    {
        vkCmdSetStencilReference(
            vulkanCommandBuffer->GetVulkanHandle(),
            VK_STENCIL_FRONT_AND_BACK,
            g_renderInterface->state.stencilReference);
    }

    if (m_stencilFunction.HasValue())
    {
        vkCmdSetStencilCompareMask(
            vulkanCommandBuffer->GetVulkanHandle(),
            VK_STENCIL_FRONT_AND_BACK,
            g_renderInterface->state.stencilCompareMask);

        vkCmdSetStencilWriteMask(
            vulkanCommandBuffer->GetVulkanHandle(),
            VK_STENCIL_FRONT_AND_BACK,
            g_renderInterface->state.stencilWriteMask);
    }
}

RendererResult VulkanGraphicsPipeline::Rebuild()
{
    Array<VkVertexInputAttributeDescription> vkVertexAttributes;
    Array<VkVertexInputBindingDescription> vkVertexBindingDescriptions;

    BuildVertexAttributes(m_vertexAttributes, vkVertexAttributes, vkVertexBindingDescriptions);

    VkPipelineVertexInputStateCreateInfo vertexInputInfo { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    vertexInputInfo.vertexBindingDescriptionCount = uint32(vkVertexBindingDescriptions.Size());
    vertexInputInfo.pVertexBindingDescriptions = vkVertexBindingDescriptions.Data();
    vertexInputInfo.vertexAttributeDescriptionCount = uint32(vkVertexAttributes.Size());
    vertexInputInfo.pVertexAttributeDescriptions = vkVertexAttributes.Data();

    VkPipelineInputAssemblyStateCreateInfo inputAsmInfo { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    inputAsmInfo.primitiveRestartEnable = VK_FALSE;

    switch (m_topology)
    {
    case TOP_TRIANGLES:
        inputAsmInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        break;
#ifndef HYP_APPLE
    case TOP_TRIANGLE_FAN:
        inputAsmInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN; // not supported on metal
        break;
#endif
    case TOP_TRIANGLE_STRIP:
        inputAsmInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        break;
    case TOP_LINES:
        inputAsmInfo.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        break;
    case TOP_POINTS:
        inputAsmInfo.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        break;
    default:
        inputAsmInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        break;
    }

    VulkanRenderPassRef renderPass = CreateObject<VulkanRenderPass>(
        m_renderTargetDesc, VulkanRenderPassMode::RenderTarget);

    HYP_GFX_CHECK(renderPass->Create());

    m_viewport = { m_renderTargetDesc.extent, Vec2i::Zero() };

    VkViewport vkViewport {};
    vkViewport.x = float(m_viewport.position.x);
    vkViewport.y = float(m_viewport.position.y);
    vkViewport.width = float(m_viewport.extent.x);
    vkViewport.height = float(m_viewport.extent.y);
    vkViewport.minDepth = 0.0f;
    vkViewport.maxDepth = 1.0f;

    VkRect2D vkScissor {};
    vkScissor.offset = { m_viewport.position.x, m_viewport.position.y };
    vkScissor.extent = { uint32(m_viewport.extent.x), uint32(m_viewport.extent.y) };

    VkPipelineViewportStateCreateInfo viewportState { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    viewportState.viewportCount = 1;
    viewportState.pViewports = &vkViewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &vkScissor;

    VkPipelineRasterizationStateCreateInfo rasterizer { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    switch (m_faceCullMode)
    {
    case FCM_BACK:
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        break;
    case FCM_FRONT:
        rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
        break;
    case FCM_NONE:
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        break;
    default:
        return HYP_MAKE_ERROR(RendererError, "Invalid value for face cull mode!");
    }

    switch (m_fillMode)
    {
    case FM_LINE:
        rasterizer.polygonMode = VK_POLYGON_MODE_LINE;
        rasterizer.lineWidth = 1.0f; // 2.5f; // have to set VK_DYNAMIC_STATE_LINE_WIDTH and wideLines feature to use any non-1.0 value
        break;
    case FM_FILL: // fallthrough
    default:
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        break;
    }

    /* Also visit for shadow mapping! Along with other optional parameters such as
     * depthBiasClamp, slopeFactor etc. */
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f;
    multisampling.pSampleMask = nullptr;
    multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
    multisampling.alphaToOneEnable = VK_FALSE;      // Optional

    Array<VkPipelineColorBlendAttachmentState> colorBlendAttachments;
    colorBlendAttachments.Reserve(m_renderTargetDesc.numAttachments);

    const BlendFunction* pBlendFunction = &m_blendFunction;

    for (uint32 attachmentIdx = 0; attachmentIdx < m_renderTargetDesc.numAttachments; attachmentIdx++)
    {
        const AttachmentDesc& attachmentDesc = m_renderTargetDesc.attachments[attachmentIdx];

        if (TextureUtils::IsDepthFormat(attachmentDesc.format))
        {
            continue;
        }

        const BlendFunction* pAttachmentBlendFunction = pBlendFunction;

        if (attachmentDesc.blendFunction != BlendFunction::None())
        {
            pAttachmentBlendFunction = &attachmentDesc.blendFunction;
        }

        const bool blendEnabled = *pAttachmentBlendFunction != BlendFunction::None()
            && TextureUtils::FormatSupportsBlending(attachmentDesc.format);

        static constexpr VkBlendOp ColorBlendOps[] = { VK_BLEND_OP_ADD, VK_BLEND_OP_ADD, VK_BLEND_OP_ADD };
        static constexpr VkBlendOp AlphaBlendOps[] = { VK_BLEND_OP_ADD, VK_BLEND_OP_ADD, VK_BLEND_OP_ADD };

        colorBlendAttachments.PushBack(VkPipelineColorBlendAttachmentState {
            .blendEnable = blendEnabled,
            .srcColorBlendFactor = ToVkBlendFactor(pAttachmentBlendFunction->GetSrcColor()),
            .dstColorBlendFactor = ToVkBlendFactor(pAttachmentBlendFunction->GetDstColor()),
            .colorBlendOp = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = ToVkBlendFactor(pAttachmentBlendFunction->GetSrcAlpha()),
            .dstAlphaBlendFactor = ToVkBlendFactor(pAttachmentBlendFunction->GetDstAlpha()),
            .alphaBlendOp = VK_BLEND_OP_ADD,
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
        });
    }

    VkPipelineColorBlendStateCreateInfo colorBlending { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = uint32(colorBlendAttachments.Size());
    colorBlending.pAttachments = colorBlendAttachments.Data();
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    // Allow updating viewport and scissor at runtime
    Array<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    if (m_stencilWrite || m_stencilFunction.HasValue())
    {
        dynamicStates.PushBack(VK_DYNAMIC_STATE_STENCIL_REFERENCE);
    }

    if (m_stencilFunction.HasValue())
    {
        dynamicStates.PushBack(VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK);
        dynamicStates.PushBack(VK_DYNAMIC_STATE_STENCIL_WRITE_MASK);
    }

    VkPipelineDynamicStateCreateInfo dynamicState { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dynamicState.dynamicStateCount = uint32(dynamicStates.Size());
    dynamicState.pDynamicStates = dynamicStates.Data();

    VkPipelineLayoutCreateInfo layoutInfo { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };

    const uint32 maxSetLayouts = g_renderBackend->GetDevice()->GetFeatures().GetPhysicalDeviceProperties().limits.maxBoundDescriptorSets;

    Array<VkDescriptorSetLayout> usedLayouts = GetVkDescriptorSetLayouts(*this);

    for (VkDescriptorSetLayout vkDescriptorSetLayout : usedLayouts)
    {
        if (vkDescriptorSetLayout == VK_NULL_HANDLE)
        {
            return HYP_MAKE_ERROR(RendererError, "Null descriptor set layout in pipeline");
        }
    }

    if (usedLayouts.Size() > maxSetLayouts)
    {
        return HYP_MAKE_ERROR(RendererError, "Device max bound descriptor sets exceeded");
    }

    layoutInfo.setLayoutCount = uint32(usedLayouts.Size());
    layoutInfo.pSetLayouts = usedLayouts.Data();

    /* Push constants */
    const VkPushConstantRange pushConstantRanges[] = {
        {
            .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
            .offset = 0,
            .size = uint32(g_renderBackend->GetDevice()->GetFeatures().PaddedSize<PushConstantData>())
        }
    };

    layoutInfo.pushConstantRangeCount = uint32(std::size(pushConstantRanges));
    layoutInfo.pPushConstantRanges = pushConstantRanges;

    VULKAN_CHECK_MSG(
        vkCreatePipelineLayout(g_renderBackend->GetDevice()->GetDevice(), &layoutInfo, nullptr, &m_layout),
        "Failed to create graphics pipeline layout");

    /* Depth / stencil */
    VkPipelineDepthStencilStateCreateInfo depthStencil { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    depthStencil.depthTestEnable = m_depthTest;
    depthStencil.depthWriteEnable = m_depthWrite;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.minDepthBounds = 0.0f; // Optional
    depthStencil.maxDepthBounds = 1.0f; // Optional
    depthStencil.stencilTestEnable = VK_FALSE;
    depthStencil.front = {};
    depthStencil.back = {};

    if (m_stencilFunction.HasValue())
    {
        depthStencil.stencilTestEnable = VK_TRUE;

        depthStencil.back = {
            .failOp = ToVkStencilOp(m_stencilFunction->failOp),
            .passOp = ToVkStencilOp(m_stencilFunction->passOp),
            .depthFailOp = ToVkStencilOp(m_stencilFunction->depthFailOp),
            .compareOp = ToVkCompareOp(m_stencilFunction->compareOp),
            .compareMask = 0xFF,
            .writeMask = 0xFF,
            .reference = 0
        };

        depthStencil.front = depthStencil.back;
    }

    VkGraphicsPipelineCreateInfo pipelineInfo { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };

    const Array<VkPipelineShaderStageCreateInfo>& stages = m_shader->GetVulkanShaderStages();
    HYP_GFX_ASSERT(stages.Any(), "No shader stages found");

    pipelineInfo.stageCount = uint32(stages.Size());
    pipelineInfo.pStages = stages.Data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAsmInfo;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_layout;
    pipelineInfo.renderPass = renderPass->GetVulkanHandle();
    pipelineInfo.subpass = 0; /* Index of the subpass */
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;

    float specializationInfoData = 0.0f;

    VkSpecializationMapEntry specializationMapEntry { 0, 0, sizeof(float) };

    VkSpecializationInfo specializationInfo {
        .mapEntryCount = 1,
        .pMapEntries = &specializationMapEntry,
        .dataSize = sizeof(float),
        .pData = &specializationInfoData
    };

    VULKAN_CHECK_MSG(
        vkCreateGraphicsPipelines(g_renderBackend->GetDevice()->GetDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_handle),
        "Failed to create graphics pipeline");

#ifdef HYP_DEBUG_MODE
    if (Name debugName = GetDebugName())
    {
        VulkanPipelineBase::SetDebugName(debugName);
    }
#endif

    return {};
}

void VulkanGraphicsPipeline::SetPushConstants(const void* data, SizeType size)
{
    VulkanPipelineBase::SetPushConstants(data, size);
}

void VulkanGraphicsPipeline::UpdateViewport(
    VulkanCommandBuffer* commandBuffer,
    const Viewport& viewport)
{
    // if (viewport == this->viewport) {
    //    return;
    // }

    VkViewport vkViewport {};
    vkViewport.x = float(viewport.position.x);
    vkViewport.y = float(viewport.position.y);
    vkViewport.width = float(viewport.extent.x);
    vkViewport.height = float(viewport.extent.y);
    vkViewport.minDepth = 0.0f;
    vkViewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer->GetVulkanHandle(), 0, 1, &vkViewport);

    VkRect2D vkScissor {};
    vkScissor.offset = { viewport.position.x, viewport.position.y };
    vkScissor.extent = { uint32(viewport.extent.x), uint32(viewport.extent.y) };
    vkCmdSetScissor(commandBuffer->GetVulkanHandle(), 0, 1, &vkScissor);

    m_viewport = viewport;
}

void VulkanGraphicsPipeline::BuildVertexAttributes(
    const VertexAttributeSet& attributeSet,
    Array<VkVertexInputAttributeDescription>& outVkVertexAttributes,
    Array<VkVertexInputBindingDescription>& outVkVertexBindingDescriptions)
{
    static constexpr VkFormat sizeToFormat[] = {
        VK_FORMAT_UNDEFINED,
        VK_FORMAT_R32_SFLOAT,
        VK_FORMAT_R32G32_SFLOAT,
        VK_FORMAT_R32G32B32_SFLOAT,
        VK_FORMAT_R32G32B32A32_SFLOAT
    };

    FlatMap<uint32, uint32> bindingSizes {};

    const Array<const VertexAttribute*> attributeTypes = attributeSet.BuildAttributes();
    outVkVertexAttributes.Resize(attributeTypes.Size());

    for (SizeType i = 0; i < attributeTypes.Size(); i++)
    {
        AssertDebug(attributeTypes[i] != nullptr);

        const VertexAttribute& attribute = *attributeTypes[i];

        outVkVertexAttributes[i] = VkVertexInputAttributeDescription {
            .location = attribute.location,
            .binding = attribute.binding,
            .format = sizeToFormat[attribute.size / sizeof(float)],
            .offset = bindingSizes[attribute.binding]
        };

        bindingSizes[attribute.binding] += attribute.size;
    }

    outVkVertexBindingDescriptions.Clear();
    outVkVertexBindingDescriptions.Reserve(bindingSizes.Size());

    for (const auto& it : bindingSizes)
    {
        outVkVertexBindingDescriptions.PushBack(VkVertexInputBindingDescription {
            .binding = it.first,
            .stride = it.second,
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
        });
    }
}

#ifdef HYP_DEBUG_MODE

void VulkanGraphicsPipeline::SetDebugName(Name name)
{
    GraphicsPipelineBase::SetDebugName(name);

    if (!IsCreated())
    {
        return;
    }

    VulkanPipelineBase::SetDebugName(name);
}

#endif

#pragma endregion GraphicsPipeline

} // namespace Hyperion
