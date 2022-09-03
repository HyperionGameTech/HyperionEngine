#ifndef HYPERION_RENDERER_HELPERS_H
#define HYPERION_RENDERER_HELPERS_H

#include "RendererResult.hpp"
#include "RendererDevice.hpp"
#include "RendererFence.hpp"
#include "RendererStructs.hpp"
#include "RendererCommandBuffer.hpp"

#include <Threads.hpp>

#include <vulkan/vulkan.h>

#include <vector>

namespace hyperion {
namespace renderer {
namespace helpers {

uint32_t MipmapSize(uint32_t src_size, int lod);

VkIndexType ToVkIndexType(DatumType);

class SingleTimeCommands {
public:
    SingleTimeCommands() : command_buffer{}, pool{}, family_indices{} {}

    inline void Push(const std::function<Result(CommandBuffer *)> &fn)
    {
        m_functions.push_back(fn);
    }

    inline Result Execute(Device *device)
    {
        HYPERION_BUBBLE_ERRORS(Begin(device));

        auto result = Result::OK;

        for (auto &fn : m_functions) {
            HYPERION_PASS_ERRORS(fn(command_buffer.get()), result);

            if (!result) {
                break;
            }
        }

        m_functions.clear();

        HYPERION_PASS_ERRORS(End(device), result);

        return result;
    }

    std::unique_ptr<CommandBuffer> command_buffer;
    VkCommandPool pool;
    QueueFamilyIndices family_indices;

private:
    std::vector<std::function<Result(CommandBuffer *)>> m_functions;
    std::unique_ptr<Fence> m_fence;

    inline Result Begin(Device *device)
    {
        v2::Threads::AssertOnThread(v2::THREAD_RENDER);

        command_buffer = std::make_unique<CommandBuffer>(CommandBuffer::Type::COMMAND_BUFFER_PRIMARY);
        m_fence = std::make_unique<Fence>();

        HYPERION_BUBBLE_ERRORS(command_buffer->Create(device, pool));

        return command_buffer->Begin(device);
    }

    inline Result End(Device *device)
    {
        v2::Threads::AssertOnThread(v2::THREAD_RENDER);

        auto result = Result::OK;

        HYPERION_PASS_ERRORS(command_buffer->End(device), result);

        HYPERION_PASS_ERRORS(m_fence->Create(device), result);

        // Submit to the queue
        auto queue_graphics = device->GetQueue(family_indices.graphics_family.value(), 0);

        HYPERION_PASS_ERRORS(command_buffer->SubmitPrimary(queue_graphics, m_fence.get(), nullptr), result);
        
        HYPERION_PASS_ERRORS(m_fence->WaitForGpu(device), result);
        HYPERION_PASS_ERRORS(m_fence->Destroy(device), result);

        HYPERION_PASS_ERRORS(command_buffer->Destroy(device, pool), result);

        return result;
    }
};

} // namespace helpers
} // namespace renderer
} // namespace hyperion

#endif