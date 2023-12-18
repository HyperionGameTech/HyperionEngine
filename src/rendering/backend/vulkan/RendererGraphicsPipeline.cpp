//
// Created by emd22 on 2022-02-20.
//

#include <rendering/backend/RendererGraphicsPipeline.hpp>
#include <rendering/backend/RendererFeatures.hpp>
#include <rendering/backend/RendererRenderPass.hpp>
#include <rendering/backend/RendererFramebuffer.hpp>

#include <system/Debug.hpp>
#include <math/MathUtil.hpp>
#include <math/Transform.hpp>

#include <util/Defines.hpp>

#include <cstring>

namespace hyperion {
namespace renderer {
namespace platform {

GraphicsPipeline<Platform::VULKAN>::GraphicsPipeline()
    : Pipeline<Platform::VULKAN>(),
      viewport { },
      scissor { }
{
}

GraphicsPipeline<Platform::VULKAN>::GraphicsPipeline(const Array<DescriptorSetRef> &used_descriptor_sets)
    : Pipeline<Platform::VULKAN>(used_descriptor_sets),
      viewport { },
      scissor { }
{
}

GraphicsPipeline<Platform::VULKAN>::~GraphicsPipeline()
{
}

void GraphicsPipeline<Platform::VULKAN>::SetViewport(float x, float y, float width, float height, float min_depth, float max_depth)
{
    VkViewport *vp = &this->viewport;
    vp->x = x;
    vp->y = y;
    vp->width = width;
    vp->height = height;
    vp->minDepth = min_depth;
    vp->maxDepth = max_depth;
}

void GraphicsPipeline<Platform::VULKAN>::SetScissor(int x, int y, uint32_t width, uint32_t height)
{
    this->scissor.offset = {x, y};
    this->scissor.extent = {width, height};
}

void GraphicsPipeline<Platform::VULKAN>::UpdateDynamicStates(VkCommandBuffer cmd)
{
    vkCmdSetViewport(cmd, 0, 1, &this->viewport);
    vkCmdSetScissor(cmd, 0, 1, &this->scissor);
}

Array<VkVertexInputAttributeDescription> GraphicsPipeline<Platform::VULKAN>::BuildVertexAttributes(const VertexAttributeSet &attribute_set)
{
    static constexpr VkFormat size_to_format[] = {
        VK_FORMAT_UNDEFINED,
        VK_FORMAT_R32_SFLOAT,
        VK_FORMAT_R32G32_SFLOAT,
        VK_FORMAT_R32G32B32_SFLOAT,
        VK_FORMAT_R32G32B32A32_SFLOAT
    };

    FlatMap<UInt32, UInt32> binding_sizes { };

    const Array<VertexAttribute> attributes = attribute_set.BuildAttributes();
    this->vertex_attributes.Resize(attributes.Size());

    for (SizeType i = 0; i < attributes.Size(); i++) {
        const auto &attribute = attributes[i];

        this->vertex_attributes[i] = VkVertexInputAttributeDescription {
            .location = attribute.location,
            .binding = attribute.binding,
            .format = size_to_format[attribute.size / sizeof(float)],
            .offset = binding_sizes[attribute.binding]
        };

        binding_sizes[attribute.binding] += attribute.size;
    }

    this->vertex_binding_descriptions.Clear();
    this->vertex_binding_descriptions.Reserve(binding_sizes.Size());

    for (const auto &it : binding_sizes) {
        this->vertex_binding_descriptions.PushBack(VkVertexInputBindingDescription {
            .binding = it.first,
            .stride = it.second,
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
        });
    }

    return this->vertex_attributes;
}

void GraphicsPipeline<Platform::VULKAN>::SubmitPushConstants(CommandBuffer<Platform::VULKAN> *cmd) const
{
    vkCmdPushConstants(
        cmd->GetCommandBuffer(),
        layout,
        VK_SHADER_STAGE_ALL_GRAPHICS,
        0, sizeof(push_constants),
        &push_constants
    );
}

void GraphicsPipeline<Platform::VULKAN>::Bind(CommandBuffer<Platform::VULKAN> *cmd)
{
    UpdateDynamicStates(cmd->GetCommandBuffer());

    vkCmdBindPipeline(cmd->GetCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, this->pipeline);

    SubmitPushConstants(cmd);
}

Result GraphicsPipeline<Platform::VULKAN>::Create(Device<Platform::VULKAN> *device, ConstructionInfo &&construction_info, DescriptorPool *descriptor_pool)
{
    m_construction_info = std::move(construction_info);

    AssertThrow(m_construction_info.shader != nullptr);
    AssertThrow(!m_construction_info.fbos.Empty());

    const uint32_t width = m_construction_info.fbos[0]->GetWidth();
    const uint32_t height = m_construction_info.fbos[0]->GetHeight();

    SetScissor(0, 0, width, height);
    SetViewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));

    m_dynamic_states = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    static int x = 0;
    DebugLog(LogType::Debug, "Create Pipeline [%d]\n", x++);

    return Rebuild(device, descriptor_pool);
}

Result GraphicsPipeline<Platform::VULKAN>::Rebuild(Device<Platform::VULKAN> *device, DescriptorPool *descriptor_pool)
{
    BuildVertexAttributes(m_construction_info.vertex_attributes);

    VkPipelineVertexInputStateCreateInfo vertex_input_info { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    vertex_input_info.vertexBindingDescriptionCount = UInt32(vertex_binding_descriptions.Size());
    vertex_input_info.pVertexBindingDescriptions = vertex_binding_descriptions.Data();
    vertex_input_info.vertexAttributeDescriptionCount = UInt32(vertex_attributes.Size());
    vertex_input_info.pVertexAttributeDescriptions = vertex_attributes.Data();

    VkPipelineInputAssemblyStateCreateInfo input_asm_info { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
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

    VkPipelineViewportStateCreateInfo viewport_state { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    VkViewport viewports[] = { viewport };
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = viewports;

    VkRect2D scissors[] = { scissor };
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = scissors;

    VkPipelineRasterizationStateCreateInfo rasterizer { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

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
        rasterizer.lineWidth = 1.0f; //2.5f; // have to set VK_DYNAMIC_STATE_LINE_WIDTH and wideLines feature to use any non-1.0 value
        break;
    case FillMode::FILL: // fallthrough
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
    multisampling.alphaToOneEnable = VK_FALSE; // Optional

    Array<VkPipelineColorBlendAttachmentState> color_blend_attachments;
    color_blend_attachments.Reserve(m_construction_info.render_pass->GetAttachmentUsages().Size());

    for (const auto *attachment_usage : m_construction_info.render_pass->GetAttachmentUsages()) {
        if (attachment_usage->IsDepthAttachment()) {
            continue;
        }

        const bool blend_enabled = m_construction_info.blend_mode != BlendMode::NONE
            && attachment_usage->AllowBlending();

        // static const VkBlendFactor src_blend_factors_rgb[] = { VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA };
        // static const VkBlendFactor dst_blend_factors_rgb[] = { VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_FACTOR_ONE  };
        // static const VkBlendFactor src_blend_factors_alpha[] = { VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA  };
        static const VkBlendFactor src_blend_factors_rgb[] = { VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_SRC_ALPHA };
        static const VkBlendFactor src_blend_factors_alpha[] = { VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE };
        static const VkBlendFactor dst_blend_factors_rgb[] = { VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_FACTOR_ONE };
        static const VkBlendFactor dst_blend_factors_alpha[] = { VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_FACTOR_ONE };
        static const VkBlendOp color_blend_ops[] = { VK_BLEND_OP_ADD, VK_BLEND_OP_ADD, VK_BLEND_OP_ADD };
        static const VkBlendOp alpha_blend_ops[] = { VK_BLEND_OP_ADD, VK_BLEND_OP_ADD, VK_BLEND_OP_ADD };

        color_blend_attachments.PushBack(VkPipelineColorBlendAttachmentState {
            .blendEnable = blend_enabled,
            .srcColorBlendFactor = src_blend_factors_rgb[UInt(m_construction_info.blend_mode)],
            .dstColorBlendFactor = dst_blend_factors_rgb[UInt(m_construction_info.blend_mode)],
            .colorBlendOp = color_blend_ops[UInt(m_construction_info.blend_mode)],
            .srcAlphaBlendFactor = src_blend_factors_alpha[UInt(m_construction_info.blend_mode)],
            .dstAlphaBlendFactor = dst_blend_factors_alpha[UInt(m_construction_info.blend_mode)],
            .alphaBlendOp = alpha_blend_ops[UInt(m_construction_info.blend_mode)],
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT
                | VK_COLOR_COMPONENT_G_BIT
                | VK_COLOR_COMPONENT_B_BIT
                | VK_COLOR_COMPONENT_A_BIT
        });
    }

    VkPipelineColorBlendStateCreateInfo color_blending { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.attachmentCount = UInt32(color_blend_attachments.Size());
    color_blending.pAttachments = color_blend_attachments.Data();
    color_blending.blendConstants[0] = 0.0f;
    color_blending.blendConstants[1] = 0.0f;
    color_blending.blendConstants[2] = 0.0f;
    color_blending.blendConstants[3] = 0.0f;

    /* Dynamic states; these are the values that can be changed without
     * rebuilding the rendering pipeline. */
    VkPipelineDynamicStateCreateInfo dynamic_state { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };

    dynamic_state.dynamicStateCount = UInt32(m_dynamic_states.Size());
    dynamic_state.pDynamicStates = m_dynamic_states.Data();
    DebugLog(
        LogType::Debug,
        "Enabling [%d] dynamic states\n",
        dynamic_state.dynamicStateCount
    );

    /* Pipeline layout */
    VkPipelineLayoutCreateInfo layout_info { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };

    const auto used_layouts = GetDescriptorSetLayouts(device, descriptor_pool);
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

    layout_info.setLayoutCount = UInt32(used_layouts.size());
    layout_info.pSetLayouts = used_layouts.data();

    /* Push constants */
    const VkPushConstantRange push_constant_ranges[] = {
        {
            .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
            .offset = 0,
            .size = UInt32(device->GetFeatures().PaddedSize<PushConstantData>())
        }
    };

    layout_info.pushConstantRangeCount = UInt32(std::size(push_constant_ranges));
    layout_info.pPushConstantRanges = push_constant_ranges;

    DebugLog(
        LogType::Debug,
        "Creating graphics pipeline layout\n"
    );

    HYPERION_VK_CHECK_MSG(
        vkCreatePipelineLayout(device->GetDevice(), &layout_info, nullptr, &this->layout),
        "Failed to create graphics pipeline layout"
    );

    /* Depth / stencil */
    VkPipelineDepthStencilStateCreateInfo depth_stencil { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    depth_stencil.depthTestEnable = m_construction_info.depth_test;
    depth_stencil.depthWriteEnable = m_construction_info.depth_write;
    depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depth_stencil.depthBoundsTestEnable = VK_FALSE;
    depth_stencil.minDepthBounds = 0.0f; // Optional
    depth_stencil.maxDepthBounds = 1.0f; // Optional
    depth_stencil.stencilTestEnable = VK_FALSE;
    depth_stencil.front = {}; // Optional
    depth_stencil.back = {}; // Optional

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

    VkGraphicsPipelineCreateInfo pipeline_info { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };

    auto &stages = m_construction_info.shader->GetShaderStages();
    AssertThrowMsg(stages.Any(), "No shader stages found");

    pipeline_info.stageCount          = UInt32(stages.Size());
    pipeline_info.pStages             = stages.Data();
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

    float specialization_info_data = 0.0f;

    VkSpecializationMapEntry specialization_map_entry { 0, 0, sizeof(float) };

    VkSpecializationInfo specialization_info {
        .mapEntryCount = 1,
        .pMapEntries   = &specialization_map_entry,
        .dataSize      = sizeof(float),
        .pData         = &specialization_info_data
    };

    // if (m_construction_info.render_pass->IsMultiview()) {
    //     AssertThrowMsg(m_construction_info.multiview_index != ~0u, "Multiview index not set but renderpass is multiview");

    //     specialization_info_data = static_cast<float>(m_construction_info.multiview_index);

    //     // create specialization info for each shader stage
    //     for (auto &stage : stages) {
    //         stage.pSpecializationInfo = &specialization_info;
    //     }
    // }

    HYPERION_VK_CHECK_MSG(
        vkCreateGraphicsPipelines(device->GetDevice(), VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &this->pipeline),
        "Failed to create graphics pipeline"
    );

    HYPERION_RETURN_OK;
}

Result GraphicsPipeline<Platform::VULKAN>::Destroy(Device<Platform::VULKAN> *device)
{
    VkDevice render_device = device->GetDevice();

    m_construction_info.fbos.Clear();

    DebugLog(LogType::Info, "Destroying pipeline!\n");

    vkDestroyPipeline(render_device, this->pipeline, nullptr);
    this->pipeline = nullptr;

    vkDestroyPipelineLayout(render_device, this->layout, nullptr);
    this->layout = nullptr;

    HYPERION_RETURN_OK;
}

} // namespace platform
} // namespace renderer
} // namespace hyperion
