//
// Created by emd22 on 2022-02-20.
//

#include "renderer_graphics_pipeline.h"
#include "renderer_features.h"
#include "renderer_render_pass.h"
#include "renderer_fbo.h"

#include <system/debug.h>
#include <math/math_util.h>
#include <math/transform.h>

#include <util/defines.h>

#include <cstring>

namespace hyperion {
namespace renderer {
GraphicsPipeline::GraphicsPipeline()
    : Pipeline(),
      viewport{},
      scissor{}
{
}

GraphicsPipeline::~GraphicsPipeline()
{
}

void GraphicsPipeline::SetViewport(float x, float y, float width, float height, float min_depth, float max_depth)
{
    VkViewport *vp = &this->viewport;
    vp->x = x;
    vp->y = y;
    vp->width = width;
    vp->height = height;
    vp->minDepth = min_depth;
    vp->maxDepth = max_depth;
}

void GraphicsPipeline::SetScissor(int x, int y, uint32_t width, uint32_t height)
{
    this->scissor.offset = {x, y};
    this->scissor.extent = {width, height};
}

void GraphicsPipeline::UpdateDynamicStates(VkCommandBuffer cmd)
{
    vkCmdSetViewport(cmd, 0, 1, &this->viewport);
    vkCmdSetScissor(cmd, 0, 1, &this->scissor);
}

std::vector<VkVertexInputAttributeDescription> GraphicsPipeline::BuildVertexAttributes(const VertexAttributeSet &attribute_set)
{
    std::unordered_map<uint32_t, uint32_t> binding_sizes{};

    const auto attributes = attribute_set.BuildAttributes();
    this->vertex_attributes = std::vector<VkVertexInputAttributeDescription>(attributes.size());

    for (size_t i = 0; i < attributes.size(); i++) {
        const auto &attribute = attributes[i];

        this->vertex_attributes[i] = VkVertexInputAttributeDescription{
            .location = attribute.location,
            .binding  = attribute.binding,
            .format   = attribute.GetFormat(),
            .offset   = binding_sizes[attribute.binding]
        };

        binding_sizes[attribute.binding] += attribute.size;
    }

    this->vertex_binding_descriptions.clear();
    this->vertex_binding_descriptions.reserve(binding_sizes.size());

    for (const auto &it : binding_sizes) {
        this->vertex_binding_descriptions.push_back(VkVertexInputBindingDescription{
            .binding = it.first,
            .stride = it.second,
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
        });
    }

    return this->vertex_attributes;
}

void GraphicsPipeline::SubmitPushConstants(CommandBuffer *cmd) const
{
    vkCmdPushConstants(
        cmd->GetCommandBuffer(),
        layout,
        VK_SHADER_STAGE_ALL_GRAPHICS,
        0, sizeof(push_constants),
        &push_constants
    );
}

void GraphicsPipeline::Bind(CommandBuffer *cmd)
{
    UpdateDynamicStates(cmd->GetCommandBuffer());

    vkCmdBindPipeline(cmd->GetCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, this->pipeline);

    SubmitPushConstants(cmd);
}

void GraphicsPipeline::SetVertexInputMode(
    std::vector<VkVertexInputBindingDescription> &binding_descs,
    std::vector<VkVertexInputAttributeDescription> &attribs)
{
    this->vertex_binding_descriptions = binding_descs;
    this->vertex_attributes = attribs;
}

Result GraphicsPipeline::Create(Device *device, ConstructionInfo &&construction_info, DescriptorPool *descriptor_pool)
{
    m_construction_info = std::move(construction_info);

    AssertThrow(m_construction_info.shader != nullptr);
    AssertThrow(!m_construction_info.fbos.empty());

    const uint32_t width = m_construction_info.fbos[0]->GetWidth();
    const uint32_t height = m_construction_info.fbos[0]->GetHeight();
    
    SetViewport(0.0f, float(height), float(width), -float(height), 0.0f, 1.0f);
    SetScissor(0, 0, width, height);
    
    SetDynamicStates({
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    });

    static int x = 0;
    DebugLog(LogType::Debug, "Create Pipeline [%d]\n", x++);

    return Rebuild(device, descriptor_pool);
}

Result GraphicsPipeline::Rebuild(Device *device, DescriptorPool *descriptor_pool)
{
    DebugLog(LogType::Debug, "Initializing graphics pipeline with shader:\n");

    if (m_construction_info.shader != nullptr) {
        for (const auto &shader_module : m_construction_info.shader->GetShaderModules()) {
            DebugLog(
                LogType::Debug,
                "\tType: %d\tName: %s\tPath: %s\n",
                int(shader_module.type),
                shader_module.spirv.metadata.name.empty()
                    ? "<not set>"
                    : shader_module.spirv.metadata.name.c_str(),
                shader_module.spirv.metadata.path.empty()
                    ? "<not set>"
                    : shader_module.spirv.metadata.path.c_str()
            );
        }
    } else {
        DebugLog(LogType::Debug, "\tNULL\n");
    }

    BuildVertexAttributes(m_construction_info.vertex_attributes);

    VkPipelineVertexInputStateCreateInfo vertex_input_info{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertex_input_info.vertexBindingDescriptionCount   = static_cast<uint32_t>(vertex_binding_descriptions.size());
    vertex_input_info.pVertexBindingDescriptions      = vertex_binding_descriptions.data();
    vertex_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertex_attributes.size());
    vertex_input_info.pVertexAttributeDescriptions    = vertex_attributes.data();

    VkPipelineInputAssemblyStateCreateInfo input_asm_info{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    input_asm_info.primitiveRestartEnable = VK_FALSE;

    switch (m_construction_info.topology) {
    case Topology::TRIANGLES:
        input_asm_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        break;
#ifndef HYP_APPLE
    case Topology::TRIANGLE_FAN:
        input_asm_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN; // not supported on metal
        break;
#endif
    case Topology::TRIANGLE_STRIP:
        input_asm_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        break;
    case Topology::LINES:
        input_asm_info.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        break;
    case Topology::POINTS:
        input_asm_info.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        break;
    default:
        input_asm_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        break;
    }

    VkPipelineViewportStateCreateInfo viewport_state{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    VkViewport viewports[]       = {viewport};
    viewport_state.viewportCount = 1;
    viewport_state.pViewports    = viewports;

    VkRect2D scissors[]         = {scissor};
    viewport_state.scissorCount = 1;
    viewport_state.pScissors    = scissors;

    VkPipelineRasterizationStateCreateInfo rasterizer{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterizer.depthClampEnable        = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    switch (m_construction_info.cull_mode) {
    case FaceCullMode::BACK:
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        break;
    case FaceCullMode::FRONT:
        rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
        break;
    case FaceCullMode::NONE:
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        break;
    }

    switch (m_construction_info.fill_mode) {
    case FillMode::LINE:
        rasterizer.polygonMode = VK_POLYGON_MODE_LINE;
        rasterizer.lineWidth = 2.5f;
        break;
    case FillMode::FILL:
    default:
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        break;
    }

    /* Also visit for shadow mapping! Along with other optional parameters such as
     * depthBiasClamp, slopeFactor etc. */
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    multisampling.sampleShadingEnable   = VK_FALSE;
    multisampling.rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading      = 1.0f;
    multisampling.pSampleMask           = nullptr;
    multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
    multisampling.alphaToOneEnable      = VK_FALSE; // Optional

    /* TODO: enable multisampling and the GPU feature required for it.  */
    std::vector<VkPipelineColorBlendAttachmentState> color_blend_attachments;
    color_blend_attachments.reserve(m_construction_info.render_pass->GetAttachmentRefs().size());

    for (const auto *attachment_ref : m_construction_info.render_pass->GetAttachmentRefs()) {
        if (attachment_ref->IsDepthAttachment()) {
            continue;
        }

        color_blend_attachments.push_back(VkPipelineColorBlendAttachmentState{
            .blendEnable         = m_construction_info.blend_enabled,
            .srcColorBlendFactor = m_construction_info.blend_enabled ? VK_BLEND_FACTOR_SRC_ALPHA : VK_BLEND_FACTOR_ONE,
            .dstColorBlendFactor = m_construction_info.blend_enabled ? VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA : VK_BLEND_FACTOR_ZERO,
            .colorBlendOp        = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE, 
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
            .alphaBlendOp        = VK_BLEND_OP_ADD,
            .colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                   VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
        });
    }

    VkPipelineColorBlendStateCreateInfo color_blending{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    color_blending.logicOpEnable     = VK_FALSE;
    color_blending.attachmentCount   = static_cast<uint32_t>(color_blend_attachments.size());
    color_blending.pAttachments      = color_blend_attachments.data();
    color_blending.blendConstants[0] = 0.0f;
    color_blending.blendConstants[1] = 0.0f;
    color_blending.blendConstants[2] = 0.0f;
    color_blending.blendConstants[3] = 0.0f;

    /* Dynamic states; these are the values that can be changed without
     * rebuilding the rendering pipeline. */
    VkPipelineDynamicStateCreateInfo dynamic_state{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    const auto &states = GetDynamicStates();

    dynamic_state.dynamicStateCount = static_cast<uint32_t>(states.size());
    dynamic_state.pDynamicStates    = states.data();
    DebugLog(
        LogType::Debug,
        "Enabling [%d] dynamic states\n",
        dynamic_state.dynamicStateCount
    );

    /* Pipeline layout */
    VkPipelineLayoutCreateInfo layout_info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};

    auto used_layouts = GetDescriptorSetLayouts(device, descriptor_pool);
    const auto max_set_layouts = device->GetFeatures().GetPhysicalDeviceProperties().limits.maxBoundDescriptorSets;

    DebugLog(
        LogType::Debug,
        "Using %llu descriptor set layouts in pipeline\n",
        used_layouts.size()
    );
    
    if (used_layouts.size() > max_set_layouts) {
        DebugLog(
            LogType::Debug,
            "Device max bound descriptor sets exceeded (%llu > %u)\n",
            used_layouts.size(),
            max_set_layouts
        );

        return Result{Result::RENDERER_ERR, "Device max bound descriptor sets exceeded"};
    }

    layout_info.setLayoutCount = static_cast<uint32_t>(used_layouts.size());
    layout_info.pSetLayouts    = used_layouts.data();

    /* Push constants */
    const VkPushConstantRange push_constant_ranges[] = {
        {
            .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
            .offset     = 0,
            .size       = static_cast<uint32_t>(device->GetFeatures().PaddedSize<PushConstantData>())
        }
    };

    layout_info.pushConstantRangeCount = static_cast<uint32_t>(std::size(push_constant_ranges));
    layout_info.pPushConstantRanges    = push_constant_ranges;

    DebugLog(
        LogType::Debug,
        "Creating graphics pipeline layout\n"
    );

    HYPERION_VK_CHECK_MSG(
        vkCreatePipelineLayout(device->GetDevice(), &layout_info, nullptr, &this->layout),
        "Failed to create graphics pipeline layout"
    );

    /* Depth / stencil */
    VkPipelineDepthStencilStateCreateInfo depth_stencil{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depth_stencil.depthTestEnable       = m_construction_info.depth_test;
    depth_stencil.depthWriteEnable      = m_construction_info.depth_write;
    depth_stencil.depthCompareOp        = VK_COMPARE_OP_LESS;
    depth_stencil.depthBoundsTestEnable = VK_FALSE;
    depth_stencil.minDepthBounds        = 0.0f; // Optional
    depth_stencil.maxDepthBounds        = 1.0f; // Optional
    depth_stencil.stencilTestEnable     = VK_FALSE;
    depth_stencil.front                 = {}; // Optional
    depth_stencil.back                  = {}; // Optional

    if (m_construction_info.stencil_state.mode != StencilMode::NONE) {
        depth_stencil.stencilTestEnable = VK_TRUE;

        switch (m_construction_info.stencil_state.mode) {
        case StencilMode::FILL:
            depth_stencil.back = {
                .failOp      = VK_STENCIL_OP_REPLACE,
                .passOp      = VK_STENCIL_OP_REPLACE,
                .depthFailOp = VK_STENCIL_OP_REPLACE,
                .compareOp   = VK_COMPARE_OP_ALWAYS
            };

            break;
        case StencilMode::OUTLINE:
            depth_stencil.back = {
                .failOp      = VK_STENCIL_OP_KEEP,
                .passOp      = VK_STENCIL_OP_KEEP,
                .depthFailOp = VK_STENCIL_OP_KEEP,
                .compareOp   = VK_COMPARE_OP_NOT_EQUAL
            };

            break;
        }

        depth_stencil.back.reference = m_construction_info.stencil_state.id;
        depth_stencil.front          = depth_stencil.back;
    }

    VkGraphicsPipelineCreateInfo pipeline_info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};

    const auto &stages = m_construction_info.shader->GetShaderStages();

    pipeline_info.stageCount          = static_cast<uint32_t>(stages.size());
    pipeline_info.pStages             = stages.data();
    pipeline_info.pVertexInputState   = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_asm_info;
    pipeline_info.pViewportState      = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState   = &multisampling;
    pipeline_info.pDepthStencilState  = &depth_stencil;
    pipeline_info.pColorBlendState    = &color_blending;
    pipeline_info.pDynamicState       = &dynamic_state;
    pipeline_info.layout              = layout;
    pipeline_info.renderPass          = m_construction_info.render_pass->GetHandle();
    pipeline_info.subpass             = 0; /* Index of the subpass */
    pipeline_info.basePipelineHandle  = VK_NULL_HANDLE;
    pipeline_info.basePipelineIndex   = -1;

    HYPERION_VK_CHECK_MSG(
        vkCreateGraphicsPipelines(device->GetDevice(), VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &this->pipeline),
        "Failed to create graphics pipeline"
    );

    HYPERION_RETURN_OK;
}

Result GraphicsPipeline::Destroy(Device *device)
{
    VkDevice render_device = device->GetDevice();

    m_construction_info.fbos.clear();

    DebugLog(LogType::Info, "Destroying pipeline!\n");

    vkDestroyPipeline(render_device, this->pipeline, nullptr);
    this->pipeline = nullptr;

    vkDestroyPipelineLayout(render_device, this->layout, nullptr);
    this->layout = nullptr;

    HYPERION_RETURN_OK;
}

} // namespace renderer
} // namespace hyperion
