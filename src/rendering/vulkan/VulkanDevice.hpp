/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/vulkan/VulkanDeviceQueue.hpp>
#include <rendering/vulkan/VulkanStructs.hpp>

#include <core/memory/UniquePtr.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/HashMap.hpp>
#include <core/containers/String.hpp>

#include <core/utilities/Span.hpp>
#include <core/utilities/Optional.hpp>

#include <rendering/RenderObject.hpp>
#include <rendering/RenderDevice.hpp>
#include <rendering/RenderResult.hpp>
#include <rendering/Shared.hpp>

#include <system/vma/VmaUsage.hpp>

namespace hyperion {

class VulkanInstance;
class VulkanFeatures;

using ExtensionMap = HashMap<String, bool>;

struct QueueFamilyIndices
{
    Optional<uint32> graphicsFamily;
    Optional<uint32> transferFamily;
    Optional<uint32> presentFamily;
    Optional<uint32> computeFamily;

    bool IsComplete() const
    {
        return graphicsFamily.HasValue()
            && transferFamily.HasValue()
            && presentFamily.HasValue()
            && computeFamily.HasValue();
    }
};

HYP_CLASS(NoScriptBindings)
class VulkanDevice final : public DeviceBase
{
    HYP_OBJECT_BODY(VulkanDevice);

    static QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);

public:
    VulkanDevice(VkPhysicalDevice physical, VkSurfaceKHR surface);
    VulkanDevice(const VulkanDevice&) = delete;
    VulkanDevice& operator=(const VulkanDevice&) = delete;
    VulkanDevice(VulkanDevice&&) noexcept = delete;
    VulkanDevice& operator=(VulkanDevice&&) noexcept = delete;
    virtual ~VulkanDevice() override;

    void Destroy();

    void SetRenderSurface(const VkSurfaceKHR& surface);
    void SetRequiredExtensions(const ExtensionMap& extensions);

    VkDevice GetDevice();
    VkSurfaceKHR GetRenderSurface();
    VkPhysicalDevice GetPhysicalDevice();

    void DebugLogAllocatorStats() const;

    RendererResult SetupAllocator(VulkanInstance* instance);
    RendererResult DestroyAllocator();

    VmaAllocator GetAllocator() const
    {
        return m_allocator;
    }

    const QueueFamilyIndices& GetQueueFamilyIndices() const
    {
        return m_queueFamilyIndices;
    }

    const VulkanFeatures& GetFeatures() const
    {
        return *m_features;
    }

    VulkanDeviceQueue& GetGraphicsQueue()
    {
        return m_queueGraphics;
    }

    const VulkanDeviceQueue& GetGraphicsQueue() const
    {
        return m_queueGraphics;
    }

    VulkanDeviceQueue& GetTransferQueue()
    {
        return m_queueTransfer;
    }

    const VulkanDeviceQueue& GetTransferQueue() const
    {
        return m_queueTransfer;
    }

    VulkanDeviceQueue& GetPresentQueue()
    {
        return m_queuePresent;
    }

    const VulkanDeviceQueue& GetPresentQueue() const
    {
        return m_queuePresent;
    }

    VulkanDeviceQueue& GetComputeQueue()
    {
        return m_queueCompute;
    }

    const VulkanDeviceQueue& GetComputeQueue() const
    {
        return m_queueCompute;
    }

    VkQueue GetQueue(uint32 queueFamilyIndex, uint32 queueIndex = 0);

    RendererResult Create(uint32 requiredQueueFamilies);
    RendererResult CheckDeviceSuitable(const ExtensionMap& unsupportedExtensions);

    /*! \brief Wait for the device to be idle */
    RendererResult Wait() const;

    /*! \brief Check if the set required extensions extensions are supported. Any unsupported extensions are returned. */
    ExtensionMap GetUnsupportedExtensions();

    Array<VkExtensionProperties> GetSupportedExtensions();

private:
    VkDevice m_device;
    VkPhysicalDevice m_physical;
    VkSurfaceKHR m_surface;
    VmaAllocator m_allocator;

    UniquePtr<VulkanFeatures> m_features;
    QueueFamilyIndices m_queueFamilyIndices;

    VulkanDeviceQueue m_queueGraphics;
    VulkanDeviceQueue m_queueTransfer;
    VulkanDeviceQueue m_queuePresent;
    VulkanDeviceQueue m_queueCompute;

    ExtensionMap m_requiredExtensions;
};

} // namespace hyperion
