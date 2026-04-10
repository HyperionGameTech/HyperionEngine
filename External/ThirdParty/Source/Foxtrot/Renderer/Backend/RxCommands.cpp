#include "RxCommands.hpp"

#include "RxDevice.hpp"

#include <Core/FxPanic.hpp>
#include <Renderer/RxGlobals.hpp>
#include <Renderer/RxRenderBackend.hpp>


FX_SET_MODULE_NAME("RxCommandBuffer")

void RxCommandPool::Create(RxGpuDevice* device, uint32 queue_family)
{
    QueueFamilyIndex = queue_family;

    const VkCommandPoolCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queue_family,
    };

    mpDevice = device;

    const VkResult status = vkCreateCommandPool(mpDevice->Device, &create_info, nullptr, &CommandPool);

    if (status != VK_SUCCESS) {
        FxPanicVulkan("FxCommandPool", "Error creating command pool", status);
    }
}

void RxCommandBuffer::Create(RxCommandPool* pool)
{
    mpCommandPool = pool;

    const VkCommandBufferAllocateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = pool->CommandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    mpDevice = gRenderer->GetDevice();

    const VkResult status = vkAllocateCommandBuffers(mpDevice->Device, &buffer_info, &CommandBuffer);
    if (status != VK_SUCCESS) {
        FxModulePanicVulkan("Could not allocate command buffer", status);
    }

    FxLogDebug("Creating Command buffer 0x{:p} from queue family {:d}", reinterpret_cast<void*>(CommandBuffer),
               pool->QueueFamilyIndex);

    mbInitialized = true;
}

inline void RxCommandBuffer::CheckInitialized() const
{
    if (!IsInitialized()) {
        FxModulePanic("Command buffer has not been initialized!", 0);
    }
}

void RxCommandBuffer::Record(VkCommandBufferUsageFlags usage_flags)
{
    CheckInitialized();

    const VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = 0,
        .pInheritanceInfo = nullptr,
    };

    const VkResult status = vkBeginCommandBuffer(CommandBuffer, &begin_info);
    if (status != VK_SUCCESS) {
        FxModulePanicVulkan("Failed to begin recording command buffer", status);
    }
}

void RxCommandBuffer::Reset()
{
    CheckInitialized();

    vkResetCommandBuffer(CommandBuffer, 0);
}

void RxCommandBuffer::End()
{
    CheckInitialized();

    const VkResult status = vkEndCommandBuffer(CommandBuffer);
    if (status != VK_SUCCESS) {
        FxModulePanicVulkan("Failed to create command buffer!", status);
    }
}

void RxCommandBuffer::Destroy()
{
    vkFreeCommandBuffers(mpDevice->Device, mpCommandPool->CommandPool, 1, &CommandBuffer);
}
