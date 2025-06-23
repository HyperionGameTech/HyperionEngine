/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_BACKEND_VULKAN_SEMAPHORE_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_SEMAPHORE_HPP

#include <rendering/backend/Platform.hpp>
#include <rendering/backend/RendererResult.hpp>

#include <vector>
#include <set>
#include <algorithm>

#include <vulkan/vulkan.h>

namespace hyperion {
class VulkanSemaphoreChain;

enum class VulkanSemaphoreType
{
    WAIT,
    SIGNAL
};

class VulkanSemaphore
{
    friend class VulkanSemaphoreChain;

public:
    VulkanSemaphore(VkPipelineStageFlags pipeline_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    ~VulkanSemaphore();

    VkSemaphore GetVulkanHandle() const
    {
        return m_semaphore;
    }

    VkPipelineStageFlags GetVulkanStageFlags() const
    {
        return m_pipeline_stage;
    }

    RendererResult Create();
    RendererResult Destroy();

private:
    VkSemaphore m_semaphore;
    VkPipelineStageFlags m_pipeline_stage;
};

struct VulkanSemaphoreRef
{
    VulkanSemaphore semaphore;
    mutable uint32_t count;

    VulkanSemaphoreRef(VkPipelineStageFlags pipeline_stage)
        : semaphore(pipeline_stage),
          count(0)
    {
    }

    bool operator<(const VulkanSemaphoreRef& other) const
    {
        return uintptr_t(semaphore.GetVulkanHandle()) < uintptr_t(other.semaphore.GetVulkanHandle());
    }
};

template <VulkanSemaphoreType Type>
struct VulkanSemaphoreRefHolder
{
    friend class VulkanSemaphoreChain;

    explicit VulkanSemaphoreRefHolder(std::nullptr_t)
        : m_ref(nullptr)
    {
    }

    VulkanSemaphoreRefHolder(VulkanSemaphoreRef* ref)
        : m_ref(ref)
    {
        ++ref->count;
    }

    VulkanSemaphoreRefHolder(const VulkanSemaphoreRefHolder& other)
        : m_ref(other.m_ref)
    {
        if (m_ref != nullptr)
        {
            ++m_ref->count;
        }
    }

    VulkanSemaphoreRefHolder& operator=(const VulkanSemaphoreRefHolder& other) = delete;

    VulkanSemaphoreRefHolder(VulkanSemaphoreRefHolder&& other) noexcept
        : m_ref(std::move(other.m_ref))
    {
        other.m_ref = nullptr;
    }

    VulkanSemaphoreRefHolder& operator=(VulkanSemaphoreRefHolder&& other) noexcept
    {
        if (&other == this)
        {
            return *this;
        }

        std::swap(m_ref, other.m_ref);
        other.Reset();

        return *this;
    }

    ~VulkanSemaphoreRefHolder()
    {
        Reset();
    }

    bool operator==(const VulkanSemaphoreRefHolder& other) const
    {
        return m_ref == other.m_ref;
    }

    void Reset()
    {
        if (m_ref != nullptr && !--m_ref->count)
        {
            /* ref->semaphore should have had Destroy() called on it by now,
             * or else an assertion error will be thrown on destructor call. */
            delete m_ref;
            /* ref = nullptr; ?? */
        }
    }

    VulkanSemaphore& Get()
    {
        return m_ref->semaphore;
    }

    const VulkanSemaphore& Get() const
    {
        return m_ref->semaphore;
    }

    template <VulkanSemaphoreType ToType>
    VulkanSemaphoreRefHolder<ToType> ConvertHeldType() const
    {
        return VulkanSemaphoreRefHolder<ToType>(m_ref);
    }

private:
    mutable VulkanSemaphoreRef* m_ref;
};

using VulkanWaitSemaphore = VulkanSemaphoreRefHolder<VulkanSemaphoreType::WAIT>;
using VulkanSignalSemaphore = VulkanSemaphoreRefHolder<VulkanSemaphoreType::SIGNAL>;

class VulkanSemaphoreChain
{
public:
    using VulkanSemaphoreView = std::vector<VkSemaphore>;
    using VulkanSemaphoreStageView = std::vector<VkPipelineStageFlags>;

    VulkanSemaphoreChain(
        const std::vector<VkPipelineStageFlags>& wait_stage_flags,
        const std::vector<VkPipelineStageFlags>& signal_stage_flags);
    VulkanSemaphoreChain(const VulkanSemaphoreChain& other) = delete;
    VulkanSemaphoreChain& operator=(const VulkanSemaphoreChain& other) = delete;
    VulkanSemaphoreChain(VulkanSemaphoreChain&& other) noexcept = default;
    VulkanSemaphoreChain& operator=(VulkanSemaphoreChain&& other) noexcept = default;
    ~VulkanSemaphoreChain();

    auto& GetWaitSemaphores()
    {
        return m_wait_semaphores;
    }

    const auto& GetWaitSemaphores() const
    {
        return m_wait_semaphores;
    }

    auto& GetSignalSemaphores()
    {
        return m_signal_semaphores;
    }

    const auto& GetSignalSemaphores() const
    {
        return m_signal_semaphores;
    }

    bool HasWaitSemaphore(const VulkanWaitSemaphore& wait_semaphore) const
    {
        return std::any_of(m_wait_semaphores.begin(), m_wait_semaphores.end(), [&wait_semaphore](const VulkanWaitSemaphore& item)
            {
                return wait_semaphore == item;
            });
    }

    bool HasSignalSemaphore(const VulkanSignalSemaphore& signal_semaphore) const
    {
        return std::any_of(m_signal_semaphores.begin(), m_signal_semaphores.end(), [&signal_semaphore](const VulkanSignalSemaphore& item)
            {
                return signal_semaphore == item;
            });
    }

    VulkanSemaphoreChain& WaitsFor(const VulkanSignalSemaphore& signal_semaphore);
    VulkanSemaphoreChain& SignalsTo(const VulkanWaitSemaphore& wait_semaphore);

    /*! \brief Make this wait on all signal semaphores that `signaler` has.
     * @param signaler The chain to wait on
     * @returns this
     */
    VulkanSemaphoreChain& WaitsFor(const VulkanSemaphoreChain& signaler);

    /*! \brief Make `waitee` wait on all signal semaphores that this chain has.
     * @param waitee The chain to have waiting on this chain
     * @returns this
     */
    VulkanSemaphoreChain& SignalsTo(VulkanSemaphoreChain& waitee);

    const VulkanSemaphoreView& GetSignalSemaphoresView() const
    {
        return m_signal_semaphores_view;
    }

    const VulkanSemaphoreStageView& GetSignalSemaphoreStagesView() const
    {
        return m_signal_semaphores_stage_view;
    }

    const VulkanSemaphoreView& GetWaitSemaphoresView() const
    {
        return m_wait_semaphores_view;
    }

    const VulkanSemaphoreStageView& GetWaitSemaphoreStagesView() const
    {
        return m_wait_semaphores_stage_view;
    }

    RendererResult Create();
    RendererResult Destroy();

private:
    static std::set<VulkanSemaphoreRef*> s_refs;

    void UpdateViews();

    std::vector<VulkanSignalSemaphore> m_signal_semaphores;
    std::vector<VulkanWaitSemaphore> m_wait_semaphores;

    VulkanSemaphoreView m_signal_semaphores_view;
    VulkanSemaphoreView m_wait_semaphores_view;
    VulkanSemaphoreStageView m_signal_semaphores_stage_view;
    VulkanSemaphoreStageView m_wait_semaphores_stage_view;
};

} // namespace hyperion

#endif
