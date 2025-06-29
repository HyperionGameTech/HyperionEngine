/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/vulkan/RendererSemaphore.hpp>
#include <rendering/backend/vulkan/VulkanRenderBackend.hpp>

#include <rendering/backend/RendererDevice.hpp>

namespace hyperion {

extern IRenderBackend* g_render_backend;

static inline VulkanRenderBackend* GetRenderBackend()
{
    return static_cast<VulkanRenderBackend*>(g_render_backend);
}

VulkanSemaphore::VulkanSemaphore(VkPipelineStageFlags pipeline_stage)
    : m_semaphore(nullptr),
      m_pipeline_stage(pipeline_stage)
{
}

VulkanSemaphore::~VulkanSemaphore()
{
    AssertThrowMsg(m_semaphore == nullptr, "semaphore should have been destroyed");
}

RendererResult VulkanSemaphore::Create()
{
    VkSemaphoreCreateInfo semaphore_info { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

    HYPERION_VK_CHECK_MSG(
        vkCreateSemaphore(GetRenderBackend()->GetDevice()->GetDevice(), &semaphore_info, nullptr, &m_semaphore),
        "Failed to create semaphore");

    HYPERION_RETURN_OK;
}

RendererResult VulkanSemaphore::Destroy()
{
    vkDestroySemaphore(GetRenderBackend()->GetDevice()->GetDevice(), m_semaphore, nullptr);
    m_semaphore = nullptr;

    HYPERION_RETURN_OK;
}

std::set<VulkanSemaphoreRef*> VulkanSemaphoreChain::s_refs {};

VulkanSemaphoreChain::VulkanSemaphoreChain(
    const std::vector<VkPipelineStageFlags>& wait_stage_flags,
    const std::vector<VkPipelineStageFlags>& signal_stage_flags)
{
    m_wait_semaphores.reserve(wait_stage_flags.size());
    m_signal_semaphores.reserve(signal_stage_flags.size());

    for (const VkPipelineStageFlags wait_stage_flag : wait_stage_flags)
    {
        auto* ref = new VulkanSemaphoreRef(wait_stage_flag);

        s_refs.insert(ref);

        m_wait_semaphores.emplace_back(ref);
    }

    for (const VkPipelineStageFlags signal_stage_flag : signal_stage_flags)
    {
        auto* ref = new VulkanSemaphoreRef(signal_stage_flag);

        s_refs.insert(ref);

        m_signal_semaphores.emplace_back(ref);
    }

    UpdateViews();
}

VulkanSemaphoreChain::~VulkanSemaphoreChain()
{
    AssertThrowMsg(
        std::all_of(m_signal_semaphores.begin(), m_signal_semaphores.end(), [](const VulkanSignalSemaphore& semaphore)
            {
                return semaphore.m_ref == nullptr;
            }),
        "All semaphores must have ref counts decremented via Destroy() before destructor call");

    AssertThrowMsg(
        std::all_of(m_wait_semaphores.begin(), m_wait_semaphores.end(), [](const VulkanWaitSemaphore& semaphore)
            {
                return semaphore.m_ref == nullptr;
            }),
        "All semaphores must have ref counts decremented via Destroy() before destructor call");
}

RendererResult VulkanSemaphoreChain::Create()
{
    for (size_t i = 0; i < m_signal_semaphores.size(); i++)
    {
        auto& ref = m_signal_semaphores[i];

        HYPERION_BUBBLE_ERRORS(ref.Get().Create());

        m_signal_semaphores_view[i] = ref.Get().GetVulkanHandle();
    }

    for (size_t i = 0; i < m_wait_semaphores.size(); i++)
    {
        auto& ref = m_wait_semaphores[i];

        HYPERION_BUBBLE_ERRORS(ref.Get().Create());

        m_wait_semaphores_view[i] = ref.Get().GetVulkanHandle();
    }

    HYPERION_RETURN_OK;
}

RendererResult VulkanSemaphoreChain::Destroy()
{
    RendererResult result;

    const auto dec_ref = [this, &result](auto& semaphore)
    {
        auto* ref = semaphore.m_ref;

        if (ref == nullptr)
        {
            return;
        }

        if (!--ref->count)
        {
            HYPERION_PASS_ERRORS(ref->semaphore.Destroy(), result);

            auto it = s_refs.find(ref);
            AssertThrow(it != s_refs.end());

            delete *it;
            s_refs.erase(it);
        }

        semaphore.m_ref = nullptr;
    };

    for (auto& semaphore : m_signal_semaphores)
    {
        dec_ref(semaphore);
    }

    for (auto& semaphore : m_wait_semaphores)
    {
        dec_ref(semaphore);
    }

    return result;
}

VulkanSemaphoreChain& VulkanSemaphoreChain::WaitsFor(const VulkanSignalSemaphore& signal_semaphore)
{
    auto wait_semaphore = signal_semaphore.ConvertHeldType<VulkanSemaphoreType::WAIT>();

    if (HasWaitSemaphore(wait_semaphore))
    {
        return *this;
    }

    m_wait_semaphores.push_back(wait_semaphore);
    m_wait_semaphores_view.push_back(wait_semaphore.Get().GetVulkanHandle());
    m_wait_semaphores_stage_view.push_back(wait_semaphore.Get().GetVulkanStageFlags());

    return *this;
}

VulkanSemaphoreChain& VulkanSemaphoreChain::WaitsFor(const VulkanSemaphoreChain& signaler)
{
    for (auto& signal_semaphore : signaler.GetSignalSemaphores())
    {
        WaitsFor(signal_semaphore);
    }

    return *this;
}

VulkanSemaphoreChain& VulkanSemaphoreChain::SignalsTo(const VulkanWaitSemaphore& wait_semaphore)
{
    auto signal_semaphore = wait_semaphore.ConvertHeldType<VulkanSemaphoreType::SIGNAL>();

    if (HasSignalSemaphore(signal_semaphore))
    {
        return *this;
    }

    m_signal_semaphores.push_back(signal_semaphore);
    m_signal_semaphores_view.push_back(signal_semaphore.Get().GetVulkanHandle());
    m_signal_semaphores_stage_view.push_back(signal_semaphore.Get().GetVulkanStageFlags());

    return *this;
}

VulkanSemaphoreChain& VulkanSemaphoreChain::SignalsTo(VulkanSemaphoreChain& waitee)
{
    for (auto& signal_semaphore : GetSignalSemaphores())
    {
        waitee.WaitsFor(signal_semaphore);
    }

    return waitee;
}

void VulkanSemaphoreChain::UpdateViews()
{
    m_signal_semaphores_view.resize(m_signal_semaphores.size());
    m_signal_semaphores_stage_view.resize(m_signal_semaphores.size());
    m_wait_semaphores_view.resize(m_wait_semaphores.size());
    m_wait_semaphores_stage_view.resize(m_wait_semaphores.size());

    for (size_t i = 0; i < m_signal_semaphores.size(); i++)
    {
        const auto& semaphore = m_signal_semaphores[i];

        m_signal_semaphores_view[i] = semaphore.Get().GetVulkanHandle();
        m_signal_semaphores_stage_view[i] = semaphore.Get().GetVulkanStageFlags();
    }

    for (size_t i = 0; i < m_wait_semaphores.size(); i++)
    {
        const auto& semaphore = m_wait_semaphores[i];

        m_wait_semaphores_view[i] = semaphore.Get().GetVulkanHandle();
        m_wait_semaphores_stage_view[i] = semaphore.Get().GetVulkanStageFlags();
    }
}

} // namespace hyperion