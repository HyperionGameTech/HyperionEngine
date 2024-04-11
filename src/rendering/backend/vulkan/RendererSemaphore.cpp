#include <rendering/backend/RendererSemaphore.hpp>
#include <rendering/backend/RendererDevice.hpp>

#include <functional>

namespace hyperion {
namespace renderer {
Semaphore::Semaphore(VkPipelineStageFlags pipeline_stage)
    : m_semaphore(nullptr),
      m_pipeline_stage(pipeline_stage)
{
}

Semaphore::~Semaphore()
{
    AssertThrowMsg(m_semaphore == nullptr, "semaphore should have been destroyed");
}

Result Semaphore::Create(Device *device)
{
    VkSemaphoreCreateInfo semaphore_info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

    HYPERION_VK_CHECK_MSG(
        vkCreateSemaphore(device->GetDevice(), &semaphore_info, nullptr, &m_semaphore),
        "Failed to create semaphore"
    );

    HYPERION_RETURN_OK;
}

Result Semaphore::Destroy(Device *device)
{
    vkDestroySemaphore(device->GetDevice(), m_semaphore, nullptr);
    m_semaphore = nullptr;

    HYPERION_RETURN_OK;
}

std::set<SemaphoreRef *> SemaphoreChain::refs{};

void SemaphoreChain::Link(SemaphoreChain &signaler, SemaphoreChain &waitee)
{
    for (auto &signal_semaphore : signaler.GetSignalSemaphores()) {
        waitee.WaitsFor(signal_semaphore);
    }
}

SemaphoreChain::SemaphoreChain(
    const std::vector<VkPipelineStageFlags> &wait_stage_flags,
    const std::vector<VkPipelineStageFlags> &signal_stage_flags
)
{
    m_wait_semaphores.reserve(wait_stage_flags.size());
    m_signal_semaphores.reserve(signal_stage_flags.size());

    for (const VkPipelineStageFlags wait_stage_flag : wait_stage_flags) {
        auto *ref = new SemaphoreRef(wait_stage_flag);

        refs.insert(ref);

        m_wait_semaphores.emplace_back(ref);
    }

    for (const VkPipelineStageFlags signal_stage_flag : signal_stage_flags) {
        auto *ref = new SemaphoreRef(signal_stage_flag);

        refs.insert(ref);

        m_signal_semaphores.emplace_back(ref);
    }
    
    UpdateViews();
}

SemaphoreChain::~SemaphoreChain()
{
    AssertThrowMsg(
        std::all_of(m_signal_semaphores.begin(), m_signal_semaphores.end(), [](const SignalSemaphore &semaphore) {
            return semaphore.ref == nullptr;
        }),
        "All semaphores must have ref counts decremented via Destroy() before destructor call"
    );

    AssertThrowMsg(
        std::all_of(m_wait_semaphores.begin(), m_wait_semaphores.end(), [](const WaitSemaphore &semaphore) {
            return semaphore.ref == nullptr;
        }),
        "All semaphores must have ref counts decremented via Destroy() before destructor call"
    );
}

Result SemaphoreChain::Create(Device *device)
{
    for (size_t i = 0; i < m_signal_semaphores.size(); i++) {
        auto &ref = m_signal_semaphores[i];
        
        HYPERION_BUBBLE_ERRORS(ref.Get().Create(device));

        m_signal_semaphores_view[i] = ref.Get().GetSemaphore();
    }

    for (size_t i = 0; i < m_wait_semaphores.size(); i++) {
        auto &ref = m_wait_semaphores[i];

        HYPERION_BUBBLE_ERRORS(ref.Get().Create(device));

        m_wait_semaphores_view[i] = ref.Get().GetSemaphore();
    }

    HYPERION_RETURN_OK;
}

Result SemaphoreChain::Destroy(Device *device)
{
    Result result;

    const auto dec_ref = [this, &result, device](auto &semaphore) {
        auto *ref = semaphore.ref;

        if (ref == nullptr) {
            return;
        }

        if (!--ref->count) {
            HYPERION_PASS_ERRORS(ref->semaphore.Destroy(device), result);

            auto it = refs.find(ref);
            AssertThrow(it != refs.end());

            delete *it;
            refs.erase(it);
        }

        semaphore.ref = nullptr;
    };

    for (auto &semaphore : m_signal_semaphores) {
        dec_ref(semaphore);
    }

    for (auto &semaphore : m_wait_semaphores) {
        dec_ref(semaphore);
    }
    
    return result;
}

SemaphoreChain &SemaphoreChain::WaitsFor(const SignalSemaphore &signal_semaphore)
{
    auto wait_semaphore = signal_semaphore.ConvertHeldType<SemaphoreType::WAIT>();

    if (HasWaitSemaphore(wait_semaphore)) {
        return *this;
    }

    m_wait_semaphores.push_back(wait_semaphore);
    m_wait_semaphores_view.push_back(wait_semaphore.Get().GetSemaphore());
    m_wait_semaphores_stage_view.push_back(wait_semaphore.Get().GetStageFlags());

    return *this;
}

SemaphoreChain &SemaphoreChain::WaitsFor(const SemaphoreChain &signaler)
{
    for (auto &signal_semaphore : signaler.GetSignalSemaphores()) {
        WaitsFor(signal_semaphore);
    }

    return *this;
}

SemaphoreChain &SemaphoreChain::SignalsTo(const WaitSemaphore &wait_semaphore)
{
    auto signal_semaphore = wait_semaphore.ConvertHeldType<SemaphoreType::SIGNAL>();
    
    if (HasSignalSemaphore(signal_semaphore)) {
        return *this;
    }

    m_signal_semaphores.push_back(signal_semaphore);
    m_signal_semaphores_view.push_back(signal_semaphore.Get().GetSemaphore());
    m_signal_semaphores_stage_view.push_back(signal_semaphore.Get().GetStageFlags());

    return *this;
}

SemaphoreChain &SemaphoreChain::SignalsTo(SemaphoreChain &waitee)
{
    for (auto &signal_semaphore : GetSignalSemaphores()) {
        waitee.WaitsFor(signal_semaphore);
    }

    return waitee;
}

void SemaphoreChain::UpdateViews()
{
    m_signal_semaphores_view.resize(m_signal_semaphores.size());
    m_signal_semaphores_stage_view.resize(m_signal_semaphores.size());
    m_wait_semaphores_view.resize(m_wait_semaphores.size());
    m_wait_semaphores_stage_view.resize(m_wait_semaphores.size());

    for (size_t i = 0; i < m_signal_semaphores.size(); i++) {
        const auto &semaphore = m_signal_semaphores[i];

        m_signal_semaphores_view[i] = semaphore.Get().GetSemaphore();
        m_signal_semaphores_stage_view[i] = semaphore.Get().GetStageFlags();
    }

    for (size_t i = 0; i < m_wait_semaphores.size(); i++) {
        const auto &semaphore = m_wait_semaphores[i];

        m_wait_semaphores_view[i] = semaphore.Get().GetSemaphore();
        m_wait_semaphores_stage_view[i] = semaphore.Get().GetStageFlags();
    }
}

} // namespace renderer
} // namespace hyperion