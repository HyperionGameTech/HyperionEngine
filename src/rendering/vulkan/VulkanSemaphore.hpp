/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once
#include <rendering/RenderResult.hpp>

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

class VulkanSemaphore final
{
    friend class VulkanSemaphoreChain;

public:
    VulkanSemaphore(VkPipelineStageFlags pipelineStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    ~VulkanSemaphore();

    VkSemaphore GetVulkanHandle() const
    {
        return m_semaphore;
    }

    VkPipelineStageFlags GetVulkanStageFlags() const
    {
        return m_pipelineStage;
    }

    RendererResult Create();

private:
    VkSemaphore m_semaphore;
    VkPipelineStageFlags m_pipelineStage;
};

struct VulkanSemaphoreRef
{
    VulkanSemaphore semaphore;
    mutable uint32_t count;

    VulkanSemaphoreRef(VkPipelineStageFlags pipelineStage)
        : semaphore(pipelineStage),
          count(0)
    {
    }

    bool operator<(const VulkanSemaphoreRef& other) const
    {
        return UIntPtr(semaphore.GetVulkanHandle()) < UIntPtr(other.semaphore.GetVulkanHandle());
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
#ifdef HYP_DEBUG_MODE
            DebugLog(LogType::Debug, "Deleting VulkanSemaphoreRef %p (ref count reached zero)\n", (void*)m_ref->semaphore.GetVulkanHandle());
#endif
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
        const std::vector<VkPipelineStageFlags>& waitStageFlags,
        const std::vector<VkPipelineStageFlags>& signalStageFlags);
    VulkanSemaphoreChain(const VulkanSemaphoreChain& other) = delete;
    VulkanSemaphoreChain& operator=(const VulkanSemaphoreChain& other) = delete;
    VulkanSemaphoreChain(VulkanSemaphoreChain&& other) noexcept = default;
    VulkanSemaphoreChain& operator=(VulkanSemaphoreChain&& other) noexcept = default;
    ~VulkanSemaphoreChain();

    auto& GetWaitSemaphores()
    {
        return m_waitSemaphores;
    }

    const auto& GetWaitSemaphores() const
    {
        return m_waitSemaphores;
    }

    auto& GetSignalSemaphores()
    {
        return m_signalSemaphores;
    }

    const auto& GetSignalSemaphores() const
    {
        return m_signalSemaphores;
    }

    bool HasWaitSemaphore(const VulkanWaitSemaphore& waitSemaphore) const
    {
        return std::any_of(m_waitSemaphores.begin(), m_waitSemaphores.end(), [&waitSemaphore](const VulkanWaitSemaphore& item)
            {
                return waitSemaphore == item;
            });
    }

    bool HasSignalSemaphore(const VulkanSignalSemaphore& signalSemaphore) const
    {
        return std::any_of(m_signalSemaphores.begin(), m_signalSemaphores.end(), [&signalSemaphore](const VulkanSignalSemaphore& item)
            {
                return signalSemaphore == item;
            });
    }

    VulkanSemaphoreChain& WaitsFor(const VulkanSignalSemaphore& signalSemaphore);
    VulkanSemaphoreChain& SignalsTo(const VulkanWaitSemaphore& waitSemaphore);

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
        return m_signalSemaphoresView;
    }

    const VulkanSemaphoreStageView& GetSignalSemaphoreStagesView() const
    {
        return m_signalSemaphoresStageView;
    }

    const VulkanSemaphoreView& GetWaitSemaphoresView() const
    {
        return m_waitSemaphoresView;
    }

    const VulkanSemaphoreStageView& GetWaitSemaphoreStagesView() const
    {
        return m_waitSemaphoresStageView;
    }

    RendererResult Create();

private:
    static std::set<VulkanSemaphoreRef*> s_refs;

    void UpdateViews();

    std::vector<VulkanSignalSemaphore> m_signalSemaphores;
    std::vector<VulkanWaitSemaphore> m_waitSemaphores;

    VulkanSemaphoreView m_signalSemaphoresView;
    VulkanSemaphoreView m_waitSemaphoresView;
    VulkanSemaphoreStageView m_signalSemaphoresStageView;
    VulkanSemaphoreStageView m_waitSemaphoresStageView;
};

} // namespace hyperion
