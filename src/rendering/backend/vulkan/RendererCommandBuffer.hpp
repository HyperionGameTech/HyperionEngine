/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_BACKEND_VULKAN_COMMAND_BUFFER_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_COMMAND_BUFFER_HPP

#include <rendering/backend/RendererCommandBuffer.hpp>

#include <rendering/backend/vulkan/RendererBuffer.hpp>
#include <rendering/backend/vulkan/RendererFence.hpp>

#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/Platform.hpp>

#include <core/containers/FixedArray.hpp>

#include <vulkan/vulkan.h>

#include <Types.hpp>

namespace hyperion {
namespace renderer {

class VulkanSemaphoreChain;
class VulkanRenderPass;

class VulkanCommandBuffer final : public CommandBufferBase
{
public:
    HYP_API explicit VulkanCommandBuffer(CommandBufferType type);
    HYP_API virtual ~VulkanCommandBuffer() override;

    HYP_FORCE_INLINE VkCommandBuffer GetVulkanHandle() const
    {
        return m_handle;
    }

    HYP_FORCE_INLINE VkCommandPool GetVulkanCommandPool() const
    {
        return m_command_pool;
    }

    HYP_FORCE_INLINE CommandBufferType GetType() const
    {
        return m_type;
    }

    HYP_API virtual bool IsCreated() const override;

    HYP_API virtual RendererResult Create() override;
    HYP_API RendererResult Create(VkCommandPool command_pool);
    HYP_API virtual RendererResult Destroy() override;

    HYP_API RendererResult Begin(const VulkanRenderPass* render_pass = nullptr);
    HYP_API RendererResult End();
    HYP_API RendererResult Reset();
    HYP_API RendererResult SubmitPrimary(
        VulkanDeviceQueue* queue,
        VulkanFence* fence,
        VulkanSemaphoreChain* semaphore_chain);

    HYP_API RendererResult SubmitSecondary(VulkanCommandBuffer* primary);

    HYP_API virtual void BindVertexBuffer(const GPUBufferBase* buffer) override;
    HYP_API virtual void BindIndexBuffer(const GPUBufferBase* buffer, DatumType datum_type = DatumType::UNSIGNED_INT) override;

    HYP_API virtual void DrawIndexed(
        uint32 num_indices,
        uint32 num_instances = 1,
        uint32 instance_index = 0) const override;

    HYP_API virtual void DrawIndexedIndirect(
        const GPUBufferBase* buffer,
        uint32 buffer_offset) const override;

    HYP_API void DebugMarkerBegin(const char* marker_name) const;
    HYP_API void DebugMarkerEnd() const;

    template <class LambdaFunction>
    RendererResult Record(const VulkanRenderPass* render_pass, LambdaFunction&& fn)
    {
        HYPERION_BUBBLE_ERRORS(Begin(render_pass));

        RendererResult result = fn(this);

        HYPERION_PASS_ERRORS(End(), result);

        return result;
    }

private:
    CommandBufferType m_type;
    VkCommandBuffer m_handle;
    VkCommandPool m_command_pool;
};

} // namespace renderer
} // namespace hyperion

#endif