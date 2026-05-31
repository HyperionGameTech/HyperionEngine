/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/RenderTypes.hpp>

#include <Vulkan/vulkan.h>

namespace Hyperion {

extern Pool* g_vulkanPool;

HYP_ENUM()
enum class VulkanSemaphoreType : uint8
{
    BINARY = 0,
    TIMELINE = 1
};

HYP_CLASS(NoScriptBindings)
class VulkanSemaphore final : public ObjectBase
{
    HYP_OBJECT_BODY(VulkanSemaphore);

public:
    static Pool* GetAllocator() { return g_vulkanPool; }

    VulkanSemaphore();

    explicit VulkanSemaphore(VulkanSemaphoreType type)
        : m_handle(VK_NULL_HANDLE),
          m_type(type)
    {
    }

    VulkanSemaphore(const VulkanSemaphore&) = delete;
    VulkanSemaphore& operator=(const VulkanSemaphore&) = delete;

    VulkanSemaphore(VulkanSemaphore&& other) noexcept
        : m_handle(other.m_handle),
          m_type(other.m_type)
    {
        other.m_handle = VK_NULL_HANDLE;
    }

    VulkanSemaphore& operator=(VulkanSemaphore&& other) noexcept;

    ~VulkanSemaphore() override;

    HYP_FORCE_INLINE VkSemaphore GetVulkanHandle() const
    {
        return m_handle;
    }

    HYP_FORCE_INLINE bool IsCreated() const
    {
        return m_handle != VK_NULL_HANDLE;
    }

    HYP_FORCE_INLINE VulkanSemaphoreType GetType() const
    {
        return m_type;
    }

    HYP_FORCE_INLINE bool IsTimeline() const
    {
        return m_type == VulkanSemaphoreType::TIMELINE;
    }

    RendererResult Create();

    void Signal(uint64 value);
    void WaitForValue(uint64 value, uint64 timeoutNs = UINT64_MAX);
    uint64 GetCounterValue() const;

#if HYP_DEBUG_MODE
    void SetDebugName(Name name);

    Name debugName;
#endif

private:
    VkSemaphore m_handle;
    VulkanSemaphoreType m_type;
};

} // namespace Hyperion
