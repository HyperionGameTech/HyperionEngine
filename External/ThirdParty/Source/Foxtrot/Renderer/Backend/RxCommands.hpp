#pragma once

#include "RxDevice.hpp"

#include <vulkan/vulkan.h>

#include <Core/FxTypes.hpp>

class RxCommandPool
{
public:
    void Create(RxGpuDevice* device, uint32 queue_family);

    void Reset() { vkResetCommandPool(mpDevice->Device, CommandPool, 0); }

    FX_FORCE_INLINE VkCommandPool Get() { return CommandPool; };

    void Destroy()
    {
        if (!CommandPool) {
            return;
        }

        vkDestroyCommandPool(mpDevice->Device, CommandPool, nullptr);
        CommandPool = nullptr;
    }

    ~RxCommandPool() { Destroy(); }

public:
    VkCommandPool CommandPool = nullptr;

    uint32 QueueFamilyIndex = 0;

private:
    RxGpuDevice* mpDevice = nullptr;
};

class RxCommandBuffer
{
public:
    void Create(RxCommandPool* pool);
    void Destroy();

    void Record(VkCommandBufferUsageFlags usage_flags = 0);

    void Reset();
    void End();

    FX_FORCE_INLINE VkCommandBuffer Get() const { return CommandBuffer; }


    operator VkCommandBuffer() const { return CommandBuffer; }

    bool IsInitialized() const { return mbInitialized; }

private:
    void CheckInitialized() const;

public:
    VkCommandBuffer CommandBuffer = nullptr;

private:
    bool mbInitialized = false;
    RxCommandPool* mpCommandPool = nullptr;
    RxGpuDevice* mpDevice = nullptr;
};
