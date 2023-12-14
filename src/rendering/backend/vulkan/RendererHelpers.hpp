#ifndef HYPERION_RENDERER_BACKEND_VULKAN_HELPERS_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_HELPERS_HPP

#include <core/lib/Proc.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <rendering/backend/RendererResult.hpp>
#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererFence.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>

#include <vulkan/vulkan.h>

#include <Types.hpp>

#include <vector>

namespace hyperion {
namespace renderer {
namespace helpers {

VkIndexType ToVkIndexType(DatumType);
VkFormat ToVkFormat(InternalFormat);
VkImageType ToVkType(ImageType);
VkFilter ToVkFilter(FilterMode);
VkSamplerAddressMode ToVkSamplerAddressMode(WrapMode);
VkImageAspectFlags ToVkImageAspect(InternalFormat internal_format);
VkImageViewType ToVkImageViewType(ImageType type, Bool is_array);

class SingleTimeCommands
{
public:
    SingleTimeCommands() : command_buffer{}, pool{}, family_indices{} {}

    void Push(const std::function<Result(const CommandBufferRef_VULKAN &)> &fn)
    {
        m_functions.push_back(fn);
    }

    Result Execute(Device *device)
    {
        HYPERION_BUBBLE_ERRORS(Begin(device));

        auto result = Result::OK;

        for (auto &fn : m_functions) {
            HYPERION_PASS_ERRORS(fn(command_buffer), result);

            if (!result) {
                break;
            }
        }

        m_functions.clear();

        HYPERION_PASS_ERRORS(End(device), result);

        return result;
    }

    CommandBufferRef_VULKAN command_buffer;
    VkCommandPool           pool;
    QueueFamilyIndices      family_indices;

private:
    std::vector<std::function<Result(const CommandBufferRef_VULKAN &)>> m_functions;
    std::unique_ptr<Fence> m_fence;

    Result Begin(Device *device)
    {
        command_buffer = MakeRenderObject<CommandBuffer, Platform::VULKAN>(CommandBufferType::COMMAND_BUFFER_PRIMARY);
        m_fence = std::make_unique<Fence>();

        HYPERION_BUBBLE_ERRORS(command_buffer->Create(device, pool));

        return command_buffer->Begin(device);
    }

    Result End(Device *device)
    {
        auto result = Result::OK;

        HYPERION_PASS_ERRORS(command_buffer->End(device), result);

        HYPERION_PASS_ERRORS(m_fence->Create(device), result);

        // Submit to the queue
        auto queue_graphics = device->GetQueue(family_indices.graphics_family.Get(), 0);

        HYPERION_PASS_ERRORS(command_buffer->SubmitPrimary(queue_graphics, m_fence.get(), nullptr), result);
        
        HYPERION_PASS_ERRORS(m_fence->WaitForGPU(device), result);
        HYPERION_PASS_ERRORS(m_fence->Destroy(device), result);

        HYPERION_PASS_ERRORS(command_buffer->Destroy(device), result);

        return result;
    }
};

} // namespace helpers
} // namespace renderer
} // namespace hyperion

#endif