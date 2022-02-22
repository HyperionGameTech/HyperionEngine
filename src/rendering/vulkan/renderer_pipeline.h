//
// Created by emd22 on 2022-02-20.
//

#ifndef HYPERION_RENDERER_PIPELINE_H
#define HYPERION_RENDERER_PIPELINE_H

#include <vulkan/vulkan.h>

#include "renderer_device.h"
#include "renderer_swapchain.h"
#include "renderer_shader.h"

namespace hyperion {

class RendererPipeline {
public:
    RendererPipeline(RendererDevice *_device, RendererSwapchain *_swapchain);

    void SetPrimitive(VkPrimitiveTopology _primitive);

    void SetDynamicStates(const std::vector<VkDynamicState> &_states);

    VkPrimitiveTopology GetPrimitive();

    std::vector<VkDynamicState> GetDynamicStates();

    VkRenderPass *GetRenderPass();

    void CreateCommandPool();

    void CreateCommandBuffers();

    ~RendererPipeline();

    void SetViewport(float x, float y, float width, float height, float min_depth = 0.0f, float max_depth = 1.0f);

    void SetScissor(int x, int y, uint32_t width, uint32_t height);

    void Rebuild(RendererShader *shader);

    void CreateRenderPass(VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_1_BIT);

    void DoRenderPass();

    /* Per frame data */
    VkCommandPool command_pool;
    std::vector<VkCommandBuffer> command_buffers;

private:
    std::vector<VkDynamicState> dynamic_states;


    VkViewport viewport;
    VkRect2D scissor;
    VkPrimitiveTopology primitive;

    VkPipeline pipeline;
    VkPipelineLayout layout;
    VkRenderPass render_pass;

    RendererSwapchain *swapchain;
    RendererDevice *device;
};

}; /* namespace hyperion */

#endif //HYPERION_RENDERER_PIPELINE_H
