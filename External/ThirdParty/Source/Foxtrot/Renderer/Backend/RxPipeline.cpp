
#include "RxPipeline.hpp"

#include "RxCommands.hpp"
#include "RxShader.hpp"
#include "RxVertexDescription.hpp"

#include <Core/FxDefines.hpp>
#include <Core/FxPanic.hpp>
#include <Core/FxStackArray.hpp>
#include <Renderer/RxDeferred.hpp>
#include <Renderer/RxGlobals.hpp>
#include <Renderer/RxRenderBackend.hpp>

FX_SET_MODULE_NAME("Pipeline")

static VkPipeline spBoundPipeline = nullptr;

void RxPipeline::Create(const std::string& name, const FxSlice<FxRef<RxShaderProgram>>& shaders,
                        const FxSlice<VkAttachmentDescription>& attachments,
                        const FxSlice<VkPipelineColorBlendAttachmentState>& color_blend_attachments,
                        RxVertexDescription* vertex_info, const RxRenderPass& render_pass,
                        const RxPipelineProperties& properties)
{
    mDevice = gRenderer->GetDevice();

    bool has_depth_attachment = false;

    // XXX: TEMP
    VertexShader = shaders[0];
    FragmentShader = shaders[1];

    // Depth attachment is usually the last attachment, check last first
    for (int32 i = attachments.Size - 1; i >= 0; i--) {
        if (RxUtil::IsFormatDepth(attachments[i].format)) {
            has_depth_attachment = true;
        }
    }

    if (!has_depth_attachment) {
        FxLogInfo("Pipeline '{}' does not have a depth attachment", name);
    }

    VkSpecializationInfo specialization_info = {
        .mapEntryCount = 0,
        .pMapEntries = nullptr,
        .dataSize = 0,
        .pData = nullptr,
    };

    // Shaders
    FxSizedArray<VkPipelineShaderStageCreateInfo> shader_create_info(shaders.Size);

    for (const FxRef<RxShaderProgram>& shader_program : shaders) {
        const VkPipelineShaderStageCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = RxShaderUtil::ToUnderlyingType(shader_program->ShaderType),
            .module = shader_program->Get(),
            .pName = "main",
            .pSpecializationInfo = &specialization_info,
        };

        FxLogDebug("Added shader (Vertex?: {})", (create_info.stage == VK_SHADER_STAGE_VERTEX_BIT));

        shader_create_info.Insert(create_info);
    }

    // Dynamic states (scissor & viewport updates dynamically)
    FxSizedArray<VkDynamicState> dynamic_states = {};

    const VkPipelineDynamicStateCreateInfo dynamic_state_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = static_cast<uint32>(dynamic_states.Size),
        .pDynamicStates = dynamic_states,
    };

    FxVec2u viewport_size = properties.ViewportSize;

    // If there is no viewport size passed in, assume the swapchain size.
    if (properties.ViewportSize.X == 0 || properties.ViewportSize.Y == 0) {
        viewport_size = gRenderer->Swapchain.Extent;
    }


    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float32>(viewport_size.X),
        .height = static_cast<float32>(viewport_size.Y),
        .minDepth = 1.0f,
        .maxDepth = 0.0f,
    };

    VkRect2D scissor = {
        .offset = { 0, 0 },
        .extent = { .width = viewport_size.X, .height = viewport_size.Y },
    };

    const VkPipelineViewportStateCreateInfo viewport_state_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor,
    };

    // Vertex info + input info
    VkPipelineVertexInputStateCreateInfo vertex_input_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
    };

    if (vertex_info != nullptr) {
        vertex_input_info.vertexBindingDescriptionCount = 1;
        vertex_input_info.pVertexBindingDescriptions = &vertex_info->Binding;
        vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32>(vertex_info->Attributes.Size);
        vertex_input_info.pVertexAttributeDescriptions = vertex_info->Attributes.pData;
    }

    const VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };

    const VkPipelineRasterizationStateCreateInfo rasterizer_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = properties.PolygonMode,
        .cullMode = properties.CullMode,
        .frontFace = properties.WindingOrder,
        .depthBiasEnable = VK_FALSE,
        .lineWidth = 1.0f,
    };


    const VkPipelineMultisampleStateCreateInfo multisampling_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE,
    };


    const VkPipelineColorBlendStateCreateInfo color_blend_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .attachmentCount = color_blend_attachments.Size,
        .pAttachments = color_blend_attachments,
    };

    VkBool32 depth_test_enabled = VK_TRUE;
    VkBool32 depth_write_enabled = VK_TRUE;

    if (properties.bDisableDepthTest) {
        depth_test_enabled = VK_FALSE;
    }
    if (properties.bDisableDepthWrite) {
        depth_write_enabled = VK_FALSE;
    }

    // Unused if has_depth_attachment is false
    const VkPipelineDepthStencilStateCreateInfo depth_stencil_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = depth_test_enabled,
        .depthWriteEnable = depth_write_enabled,
        .depthCompareOp = properties.DepthCompareOp,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
    };

    const VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,

        .stageCount = (uint32)shader_create_info.Size,
        .pStages = shader_create_info,

        .pVertexInputState = &vertex_input_info,
        .pInputAssemblyState = &input_assembly_info,
        .pViewportState = &viewport_state_info,
        .pRasterizationState = &rasterizer_info,
        .pMultisampleState = &multisampling_info,
        .pDepthStencilState = has_depth_attachment ? &depth_stencil_info : nullptr,
        .pColorBlendState = &color_blend_info,

        .pDynamicState = &dynamic_state_info,

        .layout = Layout,

        .renderPass = render_pass.RenderPass,
        .subpass = 0,
    };

    const VkResult status = vkCreateGraphicsPipelines(mDevice->Device, nullptr, 1, &pipeline_info, nullptr, &Pipeline);

    if (status != VK_SUCCESS) {
        FxModulePanicVulkan("Could not create graphics pipeline", status);
    }

    RxUtil::SetDebugLabel(name.c_str(), VK_OBJECT_TYPE_PIPELINE, Pipeline);

    FxLogInfo("Creating pipeline for shader '{}' -> LayoutHandle={:p}", shaders[0]->pShader->GetName(),
              reinterpret_cast<void*>(Layout));
}

void RxPipeline::Bind(const RxCommandBuffer& cmd)
{
    if (this->Pipeline == spBoundPipeline) {
        return;
    }

    vkCmdBindPipeline(cmd.CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Pipeline);

    spBoundPipeline = this->Pipeline;
}

void RxPipeline::Destroy()
{
    if (!mDevice || !mDevice->Device) {
        return;
    }

    mDevice->WaitForIdle();

    if (Pipeline) {
        vkDestroyPipeline(mDevice->Device, Pipeline, nullptr);
        Pipeline = nullptr;
    }
    if (Layout && !mbDoNotDestroyLayout) {
        vkDestroyPipelineLayout(mDevice->Device, Layout, nullptr);
    }

    Layout = nullptr;
    // RenderPass.Destroy();
}


VkPipelineLayout RxPipeline::CreateLayout(const FxSlice<const RxPushConstants>& push_constant_defs,
                                          const FxSlice<VkDescriptorSetLayout>& descriptor_set_layouts)
{
    FxStackArray<VkPushConstantRange, 3> push_const_ranges;

    // VkPushConstantRange pc_ranges[3];
    // uint32 pc_ranges_count = 0;
    //
    uint32 current_pc_offset = 0;

    for (int i = 0; i < push_constant_defs.Size; i++) {
        const RxPushConstants& pc_def = push_constant_defs[i];

        FxLogDebug("Adding push constant (Off={}, Sz={})", current_pc_offset, pc_def.Size);

        VkPushConstantRange range {
            .stageFlags = pc_def.StageFlags,
            .offset = current_pc_offset,
            .size = pc_def.Size,
        };
        push_const_ranges.Insert(range);

        current_pc_offset += pc_def.Size;
    }

    VkPipelineLayoutCreateInfo create_info {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = descriptor_set_layouts.Size,
        .pSetLayouts = descriptor_set_layouts.pData,

        .pushConstantRangeCount = push_const_ranges.Size,
        .pPushConstantRanges = push_const_ranges.pData,
    };

    VkPipelineLayout layout;
    VkResult status = vkCreatePipelineLayout(gRenderer->GetDevice()->Device, &create_info, nullptr, &layout);

    if (status != VK_SUCCESS) {
        FxModulePanicVulkan("Failed to create pipeline layout", status);
    }

    FxLogDebug("Creating pipeline layout {:p}", reinterpret_cast<void*>(layout));

    return layout;
}
