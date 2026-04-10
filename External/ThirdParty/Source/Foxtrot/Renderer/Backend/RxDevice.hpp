#pragma once

#include "Core/FxSizedArray.hpp"

#include <vulkan/vulkan.h>

#include <Core/FxTypes.hpp>
#include <cstdint>
#include <vector>

class RxQueueFamilies
{
public:
    static const uint32 scNullQueue = UINT32_MAX;

    void FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);

    /// Checks if the main graphics and presentation queues have been queried
    bool IsComplete()
    {
        return (mGraphicsIndex != scNullQueue && mPresentIndex != scNullQueue && mTransferIndex != scNullQueue);
    }

    /// Retrieves the available queue indices
    std::vector<uint32> GetQueueIndexList() const
    {
        return std::vector<uint32>({
            mGraphicsIndex,
            mPresentIndex,

            mTransferIndex,
        });
    }

    uint32 GetGraphicsFamily() const { return mGraphicsIndex; }
    uint32 GetPresentFamily() const { return mPresentIndex; }
    uint32 GetTransferFamily() const { return mTransferIndex; }

private:
    void FindGraphicsFamily(VkPhysicalDevice device, VkSurfaceKHR surface);
    void FindTransferFamily(VkPhysicalDevice device, VkSurfaceKHR surface);

    bool FamilyHasPresentSupport(VkPhysicalDevice device, VkSurfaceKHR surface, uint32 family_index);

public:
    FxSizedArray<VkQueueFamilyProperties> RawFamilies;

private:
    uint32 mGraphicsIndex = scNullQueue;
    uint32 mPresentIndex = scNullQueue;
    uint32 mTransferIndex = scNullQueue;
};

class RxGpuDevice
{
public:
    RxGpuDevice() = default;
    RxGpuDevice(VkInstance instance, VkSurfaceKHR surface) { Create(instance, surface); }

    void Create(VkInstance instance, VkSurfaceKHR surface);
    void Destroy();

    void PickPhysicalDevice();
    void CreateLogicalDevice();

    void WaitForIdle();

    VkSurfaceFormatKHR GetSurfaceFormat();

    operator VkDevice() const { return Device; }
    operator VkPhysicalDevice() const { return Physical; }

private:
    bool IsPhysicalDeviceSuitable(VkPhysicalDevice& device);
    void QueryQueues();

public:
    VkPhysicalDevice Physical = nullptr;
    VkDevice Device = nullptr;

    RxQueueFamilies mQueueFamilies;

    VkQueue GraphicsQueue = nullptr;
    VkQueue TransferQueue = nullptr;
    VkQueue PresentQueue = nullptr;

private:
    VkInstance mInstance;
    VkSurfaceKHR mSurface;
};
