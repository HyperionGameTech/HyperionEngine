/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/RendererGraphicsPipeline.hpp>
#include <rendering/backend/RendererFeatures.hpp>
#include <rendering/backend/RendererRenderPass.hpp>
#include <rendering/backend/RendererFramebuffer.hpp>

#include <core/debug/Debug.hpp>
#include <math/MathUtil.hpp>
#include <math/Transform.hpp>

#include <core/Defines.hpp>

#include <cstring>

namespace hyperion {
namespace renderer {
namespace platform {

#pragma region Helpers

static VkBlendFactor ToVkBlendFactor(BlendModeFactor blend_mode)
{
    switch (blend_mode) {
    case BlendModeFactor::ONE:
        return VK_BLEND_FACTOR_ONE;
    case BlendModeFactor::ZERO:
        return VK_BLEND_FACTOR_ZERO;
    case BlendModeFactor::SRC_COLOR:
        return VK_BLEND_FACTOR_SRC_COLOR;
    case BlendModeFactor::SRC_ALPHA:
        return VK_BLEND_FACTOR_SRC_ALPHA;
    case BlendModeFactor::DST_COLOR:
        return VK_BLEND_FACTOR_DST_COLOR;
    case BlendModeFactor::DST_ALPHA:
        return VK_BLEND_FACTOR_DST_ALPHA;
    case BlendModeFactor::ONE_MINUS_SRC_COLOR:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    case BlendModeFactor::ONE_MINUS_SRC_ALPHA:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case BlendModeFactor::ONE_MINUS_DST_COLOR:
        return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    case BlendModeFactor::ONE_MINUS_DST_ALPHA:
        return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    default:
        return VK_BLEND_FACTOR_ONE;
    }
}

static VkStencilOp ToVkStencilOp(StencilOp stencil_op)
{
    switch (stencil_op) {
    case StencilOp::KEEP:
        return VK_STENCIL_OP_KEEP;
    case StencilOp::ZERO:
        return VK_STENCIL_OP_ZERO;
    case StencilOp::REPLACE:
        return VK_STENCIL_OP_REPLACE;
    case StencilOp::INCREMENT:
        return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
    case StencilOp::DECREMENT:
        return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
    default:
        return VK_STENCIL_OP_KEEP;
    }
}

static VkCompareOp ToVkCompareOp(StencilCompareOp compare_op)
{
    switch (compare_op) {
    case StencilCompareOp::ALWAYS:
        return VK_COMPARE_OP_ALWAYS;
    case StencilCompareOp::NEVER:
        return VK_COMPARE_OP_NEVER;
    case StencilCompareOp::EQUAL:
        return VK_COMPARE_OP_EQUAL;
    case StencilCompareOp::NOT_EQUAL:
        return VK_COMPARE_OP_NOT_EQUAL;
    default:
        return VK_COMPARE_OP_ALWAYS;
    }
}

#pragma endregion Helpers

#pragma region GraphicsPipelinePlatformImpl

void GraphicsPipelinePlatformImpl<Platform::VULKAN>::UpdateViewport(
    CommandBuffer<Platform::VULKAN> *command_buffer,
    const Viewport &viewport
)
{
    //if (viewport == this->viewport) {
    //    return;
    //}

    VkViewport vk_viewport { };
    vk_viewport.x = float(viewport.position.x);
    vk_viewport.y = float(viewport.position.y);
    vk_viewport.width = float(viewport.extent.x);
    vk_viewport.height = float(viewport.extent.y);
    vk_viewport.minDepth = 0.0f;
    vk_viewport.maxDepth = 1.0f;
    vkCmdSetViewport(command_buffer->GetPlatformImpl().command_buffer, 0, 1, &vk_viewport);

    VkRect2D vk_scissor { };
    vk_scissor.offset = { viewport.position.x, viewport.position.y };
    vk_scissor.extent = { uint32(viewport.extent.x), uint32(viewport.extent.y) };
    vkCmdSetScissor(command_buffer->GetPlatformImpl().command_buffer, 0, 1, &vk_scissor);

    this->viewport = viewport;
}

void GraphicsPipelinePlatformImpl<Platform::VULKAN>::BuildVertexAttributes(
    const VertexAttributeSet &attribute_set,
    Array<VkVertexInputAttributeDescription> &out_vk_vertex_attributes,
    Array<VkVertexInputBindingDescription> &out_vk_vertex_binding_descriptions
)
{
    static constexpr VkFormat size_to_format[] = {
        VK_FORMAT_UNDEFINED,
        VK_FORMAT_R32_SFLOAT,
        VK_FORMAT_R32G32_SFLOAT,
        VK_FORMAT_R32G32B32_SFLOAT,
        VK_FORMAT_R32G32B32A32_SFLOAT
    };

    FlatMap<uint32, uint32> binding_sizes { };

    const Array<VertexAttribute::Type> attribute_types = attribute_set.BuildAttributes();
    out_vk_vertex_attributes.Resize(attribute_types.Size());

    for (SizeType i = 0; i < attribute_types.Size(); i++) {
        const VertexAttribute &attribute = VertexAttribute::mapping[attribute_types[i]];

        out_vk_vertex_attributes[i] = VkVertexInputAttributeDescription {
            .location   = attribute.location,
            .binding    = attribute.binding,
            .format     = size_to_format[attribute.size / sizeof(float)],
            .offset     = binding_sizes[attribute.binding]
        };

        binding_sizes[attribute.binding] += attribute.size;
    }

    out_vk_vertex_binding_descriptions.Clear();
    out_vk_vertex_binding_descriptions.Reserve(binding_sizes.Size());

    for (const auto &it : binding_sizes) {
        out_vk_vertex_binding_descriptions.PushBack(VkVertexInputBindingDescription {
            .binding    = it.first,
            .stride     = it.second,
            .inputRate  = VK_VERTEX_INPUT_RATE_VERTEX
        });
    }
}

#pragma endregion GraphicsPipelinePlatformImpl

#pragma region GraphicsPipeline

template <>
GraphicsPipeline<Platform::VULKAN>::GraphicsPipeline()
    : Pipeline<Platform::VULKAN>(),
      m_platform_impl { this }
{
}

template <>
GraphicsPipeline<Platform::VULKAN>::GraphicsPipeline(const ShaderRef<Platform::VULKAN> &shader, const DescriptorTableRef<Platform::VULKAN> &descriptor_table)
    : Pipeline<Platform::VULKAN>(shader, descriptor_table),
      m_platform_impl { this }
{
}

template <>
GraphicsPipeline<Platform::VULKAN>::~GraphicsPipeline()
{
}

template <>
void GraphicsPipeline<Platform::VULKAN>::Bind(CommandBuffer<Platform::VULKAN> *cmd)
{
    if (m_framebuffers.Any()) {
        Viewport viewport;
        viewport.position = Vec2i::Zero();
        viewport.extent = Vec2i(m_framebuffers[0]->GetExtent());
        m_platform_impl.UpdateViewport(cmd, viewport);
    }

    vkCmdBindPipeline(
        cmd->GetPlatformImpl().command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        Pipeline<Platform::VULKAN>::m_platform_impl.handle
    );

    if (m_push_constants) {
        vkCmdPushConstants(
            cmd->GetPlatformImpl().command_buffer,
            Pipeline<Platform::VULKAN>::m_platform_impl.layout,
            VK_SHADER_STAGE_ALL_GRAPHICS,
            0,
            m_push_constants.Size(),
            m_push_constants.Data()
        );
    }
}

template <>
RendererResult GraphicsPipeline<Platform::VULKAN>::Rebuild(Device<Platform::VULKAN> *device)
{
    Array<VkVertexInputAttributeDescription> vk_vertex_attributes;
    Array<VkVertexInputBindingDescription> vk_vertex_binding_descriptions;
    
    m_platform_impl.BuildVertexAttributes(m_vertex_attributes, vk_vertex_attributes, vk_vertex_binding_descriptions);

    VkPipelineVertexInputStateCreateInfo vertex_input_info { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    vertex_input_info.vertexBindingDescriptionCount = uint32(vk_vertex_binding_descriptions.Size());
    vertex_input_info.pVertexBindingDescriptions = vk_vertex_binding_descriptions.Data();
    vertex_input_info.vertexAttributeDescriptionCount = uint32(vk_vertex_attributes.Size());
    vertex_input_info.pVertexAttributeDescriptions = vk_vertex_attributes.Data();

    VkPipelineInputAssemblyStateCreateInfo input_asm_info { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    input_asm_info.primitiveRestartEnable = VK_FALSE;

    switch (m_topology) {
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

    m_platform_impl.viewport = {
        Vec2i::Zero(),
        Vec2i(m_framebuffers[0]->GetExtent())
    };

    VkViewport vk_viewport { };
    vk_viewport.x = float(m_platform_impl.viewport.position.x);
    vk_viewport.y = float(m_platform_impl.viewport.position.y);
    vk_viewport.width = float(m_platform_impl.viewport.extent.x);
    vk_viewport.height = float(m_platform_impl.viewport.extent.y);
    vk_viewport.minDepth = 0.0f;
    vk_viewport.maxDepth = 1.0f;

    VkRect2D vk_scissor { };
    vk_scissor.offset = { m_platform_impl.viewport.position.x, m_platform_impl.viewport.position.y };
    vk_scissor.extent = { uint32(m_platform_impl.viewport.extent.x), uint32(m_platform_impl.viewport.extent.y) };

    VkPipelineViewportStateCreateInfo viewport_state { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &vk_viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &vk_scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    switch (m_face_cull_mode) {
    case FaceCullMode::BACK:
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        break;
    case FaceCullMode::FRONT:
        rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
        break;
    case FaceCullMode::NONE:
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        break;
    default:
        return HYP_MAKE_ERROR(RendererError, "Invalid value for face cull mode!");
    }

    switch (m_fill_mode) {
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
    color_blend_attachments.Reserve(m_render_pass->GetAttachments().Size());

    for (const AttachmentRef<Platform::VULKAN> &attachment : m_render_pass->GetAttachments()) {
        if (attachment->IsDepthAttachment()) {
            continue;
        }

        const bool blend_enabled = attachment->AllowBlending() && m_blend_function != BlendFunction::None();

        static const VkBlendOp color_blend_ops[] = { VK_BLEND_OP_ADD, VK_BLEND_OP_ADD, VK_BLEND_OP_ADD };
        static const VkBlendOp alpha_blend_ops[] = { VK_BLEND_OP_ADD, VK_BLEND_OP_ADD, VK_BLEND_OP_ADD };

        color_blend_attachments.PushBack(VkPipelineColorBlendAttachmentState {
            .blendEnable            = blend_enabled,
            .srcColorBlendFactor    = ToVkBlendFactor(m_blend_function.GetSrcColor()),
            .dstColorBlendFactor    = ToVkBlendFactor(m_blend_function.GetDstColor()),
            .colorBlendOp           = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor    = ToVkBlendFactor(m_blend_function.GetSrcAlpha()),
            .dstAlphaBlendFactor    = ToVkBlendFactor(m_blend_function.GetDstAlpha()),
            .alphaBlendOp           = VK_BLEND_OP_ADD,
            .colorWriteMask         = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
        });
    }

    VkPipelineColorBlendStateCreateInfo color_blending { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.attachmentCount = uint32(color_blend_attachments.Size());
    color_blending.pAttachments = color_blend_attachments.Data();
    color_blending.blendConstants[0] = 0.0f;
    color_blending.blendConstants[1] = 0.0f;
    color_blending.blendConstants[2] = 0.0f;
    color_blending.blendConstants[3] = 0.0f;

    // Allow updating viewport and scissor at runtime
    const FixedArray<VkDynamicState, 2> dynamic_states {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamic_state { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dynamic_state.dynamicStateCount = uint32(dynamic_states.Size());
    dynamic_state.pDynamicStates = dynamic_states.Data();

    VkPipelineLayoutCreateInfo layout_info { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };

    const uint32 max_set_layouts = device->GetFeatures().GetPhysicalDeviceProperties().limits.maxBoundDescriptorSets;
    
    if (!m_descriptor_table.IsValid()) {
        return HYP_MAKE_ERROR(RendererError, "No descriptor table set for pipeline");
    }
    
    Array<VkDescriptorSetLayout> used_layouts = Pipeline<Platform::VULKAN>::m_platform_impl.GetDescriptorSetLayouts();

    for (VkDescriptorSetLayout vk_descriptor_set_layout : used_layouts) {
        if (vk_descriptor_set_layout == VK_NULL_HANDLE) {
            return HYP_MAKE_ERROR(RendererError, "Null descriptor set layout in pipeline");
        }
    }
    
    if (used_layouts.Size() > max_set_layouts) {
        DebugLog(
            LogType::Debug,
            "Device max bound descriptor sets exceeded (%llu > %u)\n",
            used_layouts.Size(),
            max_set_layouts
        );

        return HYP_MAKE_ERROR(RendererError, "Device max bound descriptor sets exceeded");
    }

    layout_info.setLayoutCount = uint32(used_layouts.Size());
    layout_info.pSetLayouts = used_layouts.Data();

    /* Push constants */
    const VkPushConstantRange push_constant_ranges[] = {
        {
            .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
            .offset     = 0,
            .size       = uint32(device->GetFeatures().PaddedSize<PushConstantData>())
        }
    };

    layout_info.pushConstantRangeCount = uint32(std::size(push_constant_ranges));
    layout_info.pPushConstantRanges = push_constant_ranges;

    HYPERION_VK_CHECK_MSG(
        vkCreatePipelineLayout(device->GetDevice(), &layout_info, nullptr, &Pipeline<Platform::VULKAN>::m_platform_impl.layout),
        "Failed to create graphics pipeline layout"
    );

    /* Depth / stencil */
    VkPipelineDepthStencilStateCreateInfo depth_stencil { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    depth_stencil.depthTestEnable = m_depth_test;
    depth_stencil.depthWriteEnable = m_depth_write;
    depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depth_stencil.depthBoundsTestEnable = VK_FALSE;
    depth_stencil.minDepthBounds = 0.0f; // Optional
    depth_stencil.maxDepthBounds = 1.0f; // Optional
    depth_stencil.stencilTestEnable = VK_FALSE;
    depth_stencil.front = {};
    depth_stencil.back = {};

    if (m_stencil_function.IsSet()) {
        depth_stencil.stencilTestEnable = VK_TRUE;

        depth_stencil.back = {
            .failOp      = ToVkStencilOp(m_stencil_function.fail_op),
            .passOp      = ToVkStencilOp(m_stencil_function.pass_op),
            .depthFailOp = ToVkStencilOp(m_stencil_function.depth_fail_op),
            .compareOp   = ToVkCompareOp(m_stencil_function.compare_op),
            .compareMask = m_stencil_function.mask,
            .writeMask   = m_stencil_function.mask,
            .reference   = m_stencil_function.value
        };

        depth_stencil.front = depth_stencil.back;
    }

    VkGraphicsPipelineCreateInfo pipeline_info { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };

    const Array<VkPipelineShaderStageCreateInfo> &stages = m_shader->GetPlatformImpl().vk_shader_stages;
    AssertThrowMsg(stages.Any(), "No shader stages found");

    pipeline_info.stageCount = uint32(stages.Size());
    pipeline_info.pStages = stages.Data();
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_asm_info;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pDepthStencilState= &depth_stencil;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDynamicState = &dynamic_state;
    pipeline_info.layout = Pipeline<Platform::VULKAN>::m_platform_impl.layout;
    pipeline_info.renderPass = m_render_pass->GetHandle();
    pipeline_info.subpass = 0; /* Index of the subpass */
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_info.basePipelineIndex = -1;

    float specialization_info_data = 0.0f;

    VkSpecializationMapEntry specialization_map_entry { 0, 0, sizeof(float) };

    VkSpecializationInfo specialization_info {
        .mapEntryCount = 1,
        .pMapEntries   = &specialization_map_entry,
        .dataSize      = sizeof(float),
        .pData         = &specialization_info_data
    };

    HYPERION_VK_CHECK_MSG(
        vkCreateGraphicsPipelines(device->GetDevice(), VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &Pipeline<Platform::VULKAN>::m_platform_impl.handle),
        "Failed to create graphics pipeline"
    );

    HYPERION_RETURN_OK;
}

template <>
RendererResult GraphicsPipeline<Platform::VULKAN>::Create(Device<Platform::VULKAN> *device)
{
    if (!m_shader.IsValid()) {
        return HYP_MAKE_ERROR(RendererError, "Cannot create a graphics pipeline with no shader");
    }

    if (m_framebuffers.Empty()) {
        return HYP_MAKE_ERROR(RendererError, "Cannot create a graphics pipeline with no framebuffers");
    }

    RendererResult rebuild_result = Rebuild(device);

    if (!rebuild_result) {
        return rebuild_result;
    }

    HYPERION_RETURN_OK;
}

template <>
RendererResult GraphicsPipeline<Platform::VULKAN>::Destroy(Device<Platform::VULKAN> *device)
{
    SafeRelease(std::move(m_framebuffers));

    return Pipeline<Platform::VULKAN>::Destroy(device);
}

template <>
void GraphicsPipeline<Platform::VULKAN>::SetRenderPass(const RenderPassRef<Platform::VULKAN> &render_pass)
{
    SafeRelease(std::move(m_render_pass));

    m_render_pass = render_pass;
}

template <>
void GraphicsPipeline<Platform::VULKAN>::SetFramebuffers(const Array<FramebufferRef<Platform::VULKAN>> &framebuffers)
{
    SafeRelease(std::move(m_framebuffers));

    m_framebuffers = framebuffers;
}

#pragma endregion GraphicsPipeline

} // namespace platform
} // namespace renderer
} // namespace hyperion
