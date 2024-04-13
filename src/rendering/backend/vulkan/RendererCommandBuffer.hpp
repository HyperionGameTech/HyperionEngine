#ifndef HYPERION_RENDERER_BACKEND_VULKAN_COMMAND_BUFFER_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_COMMAND_BUFFER_HPP

#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererRenderPass.hpp>
#include <rendering/backend/RendererSemaphore.hpp>
#include <rendering/backend/RendererFence.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/Platform.hpp>

#include <core/lib/FixedArray.hpp>

#include <vulkan/vulkan.h>

#include <Types.hpp>

namespace hyperion {
namespace renderer {
namespace platform {

template <PlatformType PLATFORM>
class Frame;

template <PlatformType PLATFORM>
class Pipeline;

template <PlatformType PLATFORM>
class GraphicsPipeline;

template <PlatformType PLATFORM>
class ComputePipeline;

template <PlatformType PLATFORM>
class RaytracingPipeline;

template <>
class CommandBuffer<Platform::VULKAN>
{
public:
    HYP_API CommandBuffer(CommandBufferType type);
    CommandBuffer(const CommandBuffer &other)               = delete;
    CommandBuffer &operator=(const CommandBuffer &other)    = delete;
    HYP_API ~CommandBuffer();

    CommandBufferType GetType() const
        { return m_type; }

    VkCommandBuffer GetCommandBuffer() const
        { return m_command_buffer; }

    HYP_API Result Create(Device<Platform::VULKAN> *device, VkCommandPool command_pool);
    HYP_API Result Destroy(Device<Platform::VULKAN> *device);
    HYP_API Result Begin(Device<Platform::VULKAN> *device, const RenderPass<Platform::VULKAN> *render_pass = nullptr);
    HYP_API Result End(Device<Platform::VULKAN> *device);
    HYP_API Result Reset(Device<Platform::VULKAN> *device);
    HYP_API Result SubmitPrimary(
        VkQueue queue,
        Fence<Platform::VULKAN> *fence,
        SemaphoreChain *semaphore_chain
    );

    HYP_API Result SubmitSecondary(CommandBuffer *primary);

    HYP_API void BindVertexBuffer(const GPUBuffer<Platform::VULKAN> *buffer);
    HYP_API void BindIndexBuffer(const GPUBuffer<Platform::VULKAN> *buffer, DatumType datum_type = DatumType::UNSIGNED_INT);

    HYP_API void DrawIndexed(
        uint32 num_indices,
        uint32 num_instances  = 1,
        uint32 instance_index = 0
    ) const;

    HYP_API void DrawIndexedIndirect(
        const GPUBuffer<Platform::VULKAN> *buffer,
        uint32 buffer_offset
    ) const;

    HYP_API void DebugMarkerBegin(const char *marker_name) const;
    HYP_API void DebugMarkerEnd() const;

    template <class LambdaFunction>
    Result Record(Device<Platform::VULKAN> *device, const RenderPass<Platform::VULKAN> *render_pass, const LambdaFunction &fn)
    {
        HYPERION_BUBBLE_ERRORS(Begin(device, render_pass));

        Result result = fn(this);

        HYPERION_PASS_ERRORS(End(device), result);

        return result;
    }

private:
    CommandBufferType   m_type;
    VkCommandBuffer     m_command_buffer;
    VkCommandPool       m_command_pool;
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#endif