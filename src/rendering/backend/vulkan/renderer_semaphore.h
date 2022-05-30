#ifndef HYPERION_RENDERER_SEMAPHORE_H
#define HYPERION_RENDERER_SEMAPHORE_H

#include <csignal>

#include "renderer_result.h"
#include <util/non_owning_ptr.h>

#include <vector>
#include <set>

#include <vulkan/vulkan.h>

namespace hyperion {
namespace renderer {

class Device;
class SemaphoreChain;

class Semaphore {
    friend class SemaphoreChain;
    friend class CommandBuffer;
public:
    Semaphore(VkPipelineStageFlags pipeline_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    Semaphore(const Semaphore &other) = delete;
    Semaphore &operator=(const Semaphore &other) = delete;
    ~Semaphore();

    inline VkSemaphore &GetSemaphore() { return m_semaphore; }
    inline VkSemaphore GetSemaphore() const { return m_semaphore; }
    inline VkPipelineStageFlags GetStageFlags() const { return m_pipeline_stage; }

    Result Create(Device *device);
    Result Destroy(Device *device);

private:
    VkSemaphore m_semaphore;
    VkPipelineStageFlags m_pipeline_stage;
};

struct SemaphoreRef {
    Semaphore semaphore;
    mutable uint32_t count;

    SemaphoreRef(VkPipelineStageFlags pipeline_stage)
        : semaphore(pipeline_stage),
          count(0)
    {
    }

    inline bool operator<(const SemaphoreRef &other) const
        { return uintptr_t(semaphore.GetSemaphore()) < uintptr_t(other.semaphore.GetSemaphore()); }
};

enum class SemaphoreType {
    WAIT,
    SIGNAL
};

template <SemaphoreType Type>
struct SemaphoreRefHolder {
    friend class SemaphoreChain;

    explicit SemaphoreRefHolder(std::nullptr_t) : ref(nullptr) {}
    SemaphoreRefHolder(SemaphoreRef *ref) : ref(ref) { ++ref->count; }

    SemaphoreRefHolder(const SemaphoreRefHolder &other)
        : ref(other.ref)
    {
        if (ref != nullptr) {
            ++ref->count;
        }
    }

    SemaphoreRefHolder &operator=(const SemaphoreRefHolder &other) = delete;

    SemaphoreRefHolder(SemaphoreRefHolder &&other) noexcept
        : ref(std::move(other.ref))
    {
        other.ref = nullptr;
    }


    SemaphoreRefHolder &operator=(SemaphoreRefHolder &&other) noexcept
    {
        if (&other == this) {
            return *this;
        }

        std::swap(ref, other.ref);
        other.Reset();

        return *this;
    }

    ~SemaphoreRefHolder()
        { Reset(); }

    bool operator==(const SemaphoreRefHolder &other) const
        { return ref == other.ref; }

    inline void Reset()
    {
        if (ref != nullptr && !--ref->count) {
            /* ref->semaphore should have had Destroy() called on it by now,
             * or else an assertion error will be thrown on destructor call. */
            delete ref;
            /* ref = nullptr; ?? */
        }
    }

    inline Semaphore &Get() { return ref->semaphore; }
    inline const Semaphore &Get() const { return ref->semaphore; }
    inline VkSemaphore &GetSemaphore() { return ref->semaphore.GetSemaphore(); }
    inline const VkSemaphore &GetSemaphore() const { return ref->semaphore.GetSemaphore(); }
    inline VkPipelineStageFlags GetStageFlags() const { return ref->semaphore.GetStageFlags(); }

    template <SemaphoreType ToType>
    SemaphoreRefHolder<ToType> ConvertHeldType() const
        { return SemaphoreRefHolder<ToType>(ref); }

private:
    mutable SemaphoreRef *ref;

};

using WaitSemaphore = SemaphoreRefHolder<SemaphoreType::WAIT>;
using SignalSemaphore = SemaphoreRefHolder<SemaphoreType::SIGNAL>;

class SemaphoreChain {
    friend class CommandBuffer;
    friend class Instance;
public:
    using SemaphoreView = std::vector<VkSemaphore>;
    using SemaphoreStageView = std::vector<VkPipelineStageFlags>;

    static void Link(SemaphoreChain &signaler, SemaphoreChain &waitee);
    
    SemaphoreChain(const std::vector<VkPipelineStageFlags> &wait_stage_flags,
        const std::vector<VkPipelineStageFlags> &signal_stage_flags);
    SemaphoreChain(const SemaphoreChain &other) = delete;
    SemaphoreChain &operator=(const SemaphoreChain &other) = delete;
    SemaphoreChain(SemaphoreChain &&other) noexcept = default;
    SemaphoreChain &operator=(SemaphoreChain &&other) noexcept = default;
    ~SemaphoreChain();

    inline auto &GetWaitSemaphores() { return m_wait_semaphores; }
    inline const auto &GetWaitSemaphores() const { return m_wait_semaphores; }
    inline auto &GetSignalSemaphores() { return m_signal_semaphores; }
    inline const auto &GetSignalSemaphores() const { return m_signal_semaphores; }

    inline bool HasWaitSemaphore(const WaitSemaphore &wait_semaphore) const
    {
        return std::any_of(m_wait_semaphores.begin(), m_wait_semaphores.end(), [&wait_semaphore](const WaitSemaphore &item) {
            return wait_semaphore == item;
        });
    }

    inline bool HasSignalSemaphore(const SignalSemaphore &signal_semaphore) const
    {
        return std::any_of(m_signal_semaphores.begin(), m_signal_semaphores.end(), [&signal_semaphore](const SignalSemaphore &item) {
            return signal_semaphore == item;
        });
    }

    SemaphoreChain &WaitsFor(const SignalSemaphore &signal_semaphore);
    SemaphoreChain &SignalsTo(const WaitSemaphore &wait_semaphore);
    
    /*! \brief Make this wait on all signal semaphores that `signaler` has.
     * @param signaler The chain to wait on
     * @returns this
     */
    SemaphoreChain &WaitsFor(const SemaphoreChain &signaler);

    /*! \brief Make `waitee` wait on all signal semaphores that this chain has.
     * @param waitee The chain to have waiting on this chain
     * @returns this
     */
    SemaphoreChain &SignalsTo(SemaphoreChain &waitee);

    inline const SemaphoreView &GetSignalSemaphoresView() const
        { return m_signal_semaphores_view; }

    inline const SemaphoreStageView &GetSignalSemaphoreStagesView() const
        { return m_signal_semaphores_stage_view; }

    inline const SemaphoreView &GetWaitSemaphoresView() const
        { return m_wait_semaphores_view; }

    inline const SemaphoreStageView &GetWaitSemaphoreStagesView() const
        { return m_wait_semaphores_stage_view; }

    Result Create(Device *device);
    Result Destroy(Device *device);

private:
    static std::set<SemaphoreRef *> refs;

    void UpdateViews();
    
    std::vector<SignalSemaphore> m_signal_semaphores;
    std::vector<WaitSemaphore>   m_wait_semaphores;

    /* Views, for on the fly linear memory access: */
    SemaphoreView m_signal_semaphores_view;
    SemaphoreView m_wait_semaphores_view;
    SemaphoreStageView m_signal_semaphores_stage_view;
    SemaphoreStageView m_wait_semaphores_stage_view;
};

} // namespace renderer
} // namespace hyperion

#endif
