//
// Created by emd22 on 2022-02-20.
//

#include "renderer_pipeline.h"

#include "../../system/debug.h"

namespace hyperion {

RendererPipeline::RendererPipeline(RendererDevice *_device, RendererSwapchain *_swapchain) {
    this->primitive = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    this->swapchain = _swapchain;
    this->device = _device;

    auto width = (float) swapchain->extent.width;
    auto height = (float) swapchain->extent.height;
    this->SetViewport(0.0f, 0.0f, width, height, 0.0f, 1.0f);
    this->SetScissor(0, 0, _swapchain->extent.width, _swapchain->extent.height);

    std::vector<VkDynamicState> default_dynamic_states = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_LINE_WIDTH,
    };
    this->SetDynamicStates(default_dynamic_states);
}

void RendererPipeline::SetPrimitive(VkPrimitiveTopology _primitive) {
    this->primitive = _primitive;
}

VkPrimitiveTopology RendererPipeline::GetPrimitive() {
    return this->primitive;
}

void RendererPipeline::SetViewport(float x, float y, float width, float height, float min_depth, float max_depth) {
    VkViewport *vp = &this->viewport;
    vp->x = x;
    vp->y = y;
    vp->width = width;
    vp->height = height;
    vp->minDepth = min_depth;
    vp->maxDepth = max_depth;
}

void RendererPipeline::SetScissor(int x, int y, uint32_t width, uint32_t height) {
    this->scissor.offset = {x, y};
    this->scissor.extent = {width, height};
}

void RendererPipeline::SetDynamicStates(const std::vector<VkDynamicState> &_states) {
    this->dynamic_states = _states;
}

std::vector<VkDynamicState> RendererPipeline::GetDynamicStates() {
    return this->dynamic_states;
}

VkRenderPass *RendererPipeline::GetRenderPass() {
    return &this->render_pass;
}

void RendererPipeline::CreateCommandPool() {
    QueueFamilyIndices family_indices = this->device->FindQueueFamilies();

    VkCommandPoolCreateInfo pool_info{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pool_info.queueFamilyIndex = family_indices.graphics_family.value();
    /* TODO: look into VK_COMMAND_POOL_CREATE_TRANSIENT_BIT for constantly changing objects */
    pool_info.flags = 0;

    auto result = vkCreateCommandPool(this->device->GetDevice(), &pool_info, nullptr, &this->command_pool);

    AssertThrowMsg(result == VK_SUCCESS, "Could not create Vulkan command pool\n");
}

void RendererPipeline::CreateCommandBuffers() {
    this->command_buffers.resize(this->swapchain->framebuffers.size());

    VkCommandBufferAllocateInfo alloc_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    alloc_info.commandPool = this->command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = (uint32_t) this->command_buffers.size();

    auto result = vkAllocateCommandBuffers(this->device->GetDevice(), &alloc_info, this->command_buffers.data());

    AssertThrowMsg(result == VK_SUCCESS, "Could not create Vulkan command buffers\n");
}

void RendererPipeline::DoRenderPass() {
    for (size_t i = 0; i < this->command_buffers.size(); i++) {
        VkCommandBufferBeginInfo begin_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        begin_info.pInheritanceInfo = nullptr;
        /* Begin recording our command buffer */
        auto result = vkBeginCommandBuffer(this->command_buffers[i], &begin_info);
        AssertThrowMsg(result == VK_SUCCESS, "Failed to start recording command buffer [%d]!\n", i);

        VkRenderPassBeginInfo render_pass_info{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        render_pass_info.renderPass = this->render_pass;
        render_pass_info.framebuffer = this->swapchain->framebuffers[i];
        render_pass_info.renderArea.offset = {0, 0};
        render_pass_info.renderArea.extent = this->swapchain->extent;

        VkClearValue clear_colour = {0.5f, 0.7f, 0.5f, 1.0f};
        render_pass_info.clearValueCount = 1;
        render_pass_info.pClearValues = &clear_colour;
        /* Start adding draw commands  */
        vkCmdBeginRenderPass(this->command_buffers[i], &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
        /* Bind the graphics pipeline */
        vkCmdBindPipeline(this->command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, this->pipeline);
        vkCmdDraw(this->command_buffers[i], 3, 1, 0, 0);
        /* Finish the render pass */
        vkCmdEndRenderPass(this->command_buffers[i]);

        result = vkEndCommandBuffer(this->command_buffers[i]);
        AssertThrowMsg(result == VK_SUCCESS, "Failed to record command buffer [%d]!\n", i);
    }
}

void RendererPipeline::CreateRenderPass(VkSampleCountFlagBits sample_count) {
    VkAttachmentDescription attachment{};
    attachment.format = this->swapchain->image_format;
    attachment.samples = sample_count;
    /* Options to change what happens before and after our render pass */
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    /* Images are presented to the swapchain */
    attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference attachment_ref{};
    attachment_ref.attachment = 0; /* Our attachment array(only one pass, so it will be 0) */
    attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    /* Subpasses for post processing, etc. */
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &attachment_ref;

    const VkAttachmentDescription attachments[] = {attachment};
    const VkSubpassDescription subpasses[] = {subpass};

    /* Create render pass */
    VkRenderPassCreateInfo render_pass_info{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments = attachments;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = subpasses;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies = &dependency;

    if (vkCreateRenderPass(this->device->GetDevice(), &render_pass_info, nullptr, &this->render_pass) != VK_SUCCESS) {
        DebugLog(LogType::Error, "Could not create render pass!\n");
        throw std::runtime_error("Error creating render pass");
    }
    DebugLog(LogType::Info, "Renderpass created!\n");
}

void RendererPipeline::Rebuild(RendererShader *shader) {
    VkPipelineVertexInputStateCreateInfo vertex_input_info{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertex_input_info.vertexBindingDescriptionCount = 0;
    vertex_input_info.pVertexBindingDescriptions = nullptr;
    vertex_input_info.vertexAttributeDescriptionCount = 0;
    vertex_input_info.pVertexAttributeDescriptions = nullptr;

    VkPipelineInputAssemblyStateCreateInfo input_asm_info{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    input_asm_info.topology = this->GetPrimitive();
    input_asm_info.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewport_state{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    VkViewport viewports[] = {this->viewport};
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = viewports;

    VkRect2D scissors[] = {this->scissor};
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = scissors;

    VkPipelineRasterizationStateCreateInfo rasterizer{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    /* TODO: Revisit this for shadow maps! */
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    /* Backface culling */
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    /* Also visit for shadow mapping! Along with other optional parameters such as
     * depthBiasClamp, slopeFactor etc. */
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f;
    multisampling.pSampleMask = nullptr;

    /* TODO: enable multisampling and the GPU feature required for it.  */

    VkPipelineColorBlendAttachmentState color_blend_attachment{
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    color_blend_attachment.colorWriteMask = (
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);

    /* TODO: with multiple framebuffers and post processing, we will need to enable colour blending */
    color_blend_attachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo color_blending{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    color_blending.logicOpEnable = false;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;

    /* Dynamic states; these are the values that can be changed without
     * rebuilding the rendering pipeline. */
    VkPipelineDynamicStateCreateInfo dynamic_state{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamic_state.dynamicStateCount = 2;
    VkDynamicState states[] = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_LINE_WIDTH,
    };
    dynamic_state.pDynamicStates = states;
    DebugLog(LogType::Info, "Enabling [%d] dynamic states\n", dynamic_state.dynamicStateCount);
    /* Pipeline layout */
    VkPipelineLayoutCreateInfo layout_info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layout_info.setLayoutCount = 0;
    layout_info.pSetLayouts = nullptr;
    layout_info.pushConstantRangeCount = 0;
    layout_info.pPushConstantRanges = nullptr;

    if (vkCreatePipelineLayout(this->device->GetDevice(), &layout_info, nullptr, &this->layout) != VK_SUCCESS) {
        DebugLog(LogType::Error, "Error creating pipeline layout!\n");
        throw std::runtime_error("Error creating pipeline layout");
    }

    VkGraphicsPipelineCreateInfo pipeline_info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    auto stages = shader->shader_stages;
    pipeline_info.stageCount = stages.size();
    pipeline_info.pStages = stages.data();

    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_asm_info;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pDepthStencilState = nullptr;
    pipeline_info.pColorBlendState = &color_blending;
    /* TODO: reimplement dynamic states! */
    pipeline_info.pDynamicState = nullptr;

    pipeline_info.layout = layout;
    pipeline_info.renderPass = render_pass;
    pipeline_info.subpass = 0; /* Index of the subpass */
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_info.basePipelineIndex = -1;

    if (vkCreateGraphicsPipelines(this->device->GetDevice(), VK_NULL_HANDLE, 1, &pipeline_info, nullptr,
                                  &this->pipeline) != VK_SUCCESS) {
        DebugLog(LogType::Error, "Could not create graphics pipeline!\n");
        throw std::runtime_error("Could not create pipeline");
    }
    DebugLog(LogType::Info, "Created graphics pipeline!\n");
}

RendererPipeline::~RendererPipeline() {
    VkDevice render_device = this->device->GetDevice();

    vkFreeCommandBuffers(render_device, this->command_pool, this->command_buffers.size(), this->command_buffers.data());

    vkDestroyCommandPool(render_device, this->command_pool, nullptr);

    DebugLog(LogType::Info, "Destroying pipeline!\n");

    vkDestroyPipeline(render_device, this->pipeline, nullptr);
    vkDestroyPipelineLayout(render_device, this->layout, nullptr);
    vkDestroyRenderPass(render_device, this->render_pass, nullptr);
}

}; /* namespace hyperion */