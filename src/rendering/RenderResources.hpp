/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDER_RESOURCES_HPP
#define HYPERION_RENDER_RESOURCES_HPP

#include <core/threading/Threads.hpp>
#include <core/threading/AtomicVar.hpp>
#include <core/threading/Semaphore.hpp>
#include <core/threading/DataRaceDetector.hpp>

#include <core/functional/Proc.hpp>

#include <core/memory/RefCountedPtr.hpp>
#include <core/memory/UniquePtr.hpp>

#include <core/memory/resource/Resource.hpp>

#include <Types.hpp>

namespace hyperion {

class GPUBufferHolderBase;

// Represents the objects an engine object (e.g Material) uses while it is currently being rendered.
// The resources are reference counted internally, so as long as the object is being used for rendering somewhere,
// the resources will remain in memory.
class RenderResourcesBase : public IResource
{
    using PreInitSemaphore = Semaphore<int32, SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE, threading::detail::AtomicSemaphoreImpl<int32, SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE>>;
    using InitSemaphore = Semaphore<int32, SemaphoreDirection::WAIT_FOR_POSITIVE, threading::detail::AtomicSemaphoreImpl<int32, SemaphoreDirection::WAIT_FOR_POSITIVE>>;
    using CompletionSemaphore = Semaphore<int32, SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE, threading::detail::AtomicSemaphoreImpl<int32, SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE>>;

public:
    RenderResourcesBase();

    RenderResourcesBase(const RenderResourcesBase &other)                   = delete;
    RenderResourcesBase &operator=(const RenderResourcesBase &other)        = delete;

    RenderResourcesBase(RenderResourcesBase &&other) noexcept;
    RenderResourcesBase &operator=(RenderResourcesBase &&other) noexcept    = delete;

    virtual ~RenderResourcesBase() override;

    virtual bool IsNull() const override
        { return false; }

    virtual int Claim() override final;
    virtual int Unclaim() override final;

    /*! \brief Waits (blocking) until all operations on this RenderResources are complete and the RenderResources is no longer being used. */
    virtual void WaitForCompletion() override final;

    /*! \brief Performs an operation on the render thread if the resources are initialized,
     *  otherwise executes it immediately on the calling thread. Initialization on the render thread will not begin until at least the end of the given proc,
     *  so it is safe to use this method on any thread.
     *  \param proc The operation to perform.
     *  \param force_render_thread If true, the operation will be performed on the render thread regardless of initialization state. */
    void Execute(Proc<void> &&proc, bool force_render_thread = false);

    virtual ResourceMemoryPoolHandle GetPoolHandle() const override final
    {
        return m_pool_handle;
    }

    virtual void SetPoolHandle(ResourceMemoryPoolHandle pool_handle) override final
    {
        m_pool_handle = pool_handle;
    }

    /*! \note Only call from render thread or from task on a task thread that is initiated by the render thread. */
    HYP_FORCE_INLINE uint32 GetBufferIndex() const
        { return m_buffer_index; }

    /*! \note Only call from render thread or from task on a task thread that is initiated by the render thread. */
    HYP_FORCE_INLINE void *GetBufferAddress() const
        { return m_buffer_address; }

#ifdef HYP_DEBUG_MODE
    HYP_FORCE_INLINE uint32 GetUseCount() const
        { return m_ref_count.Get(MemoryOrder::SEQUENTIAL); }
#endif

protected:
    HYP_FORCE_INLINE bool IsInitialized() const
        { return m_init_semaphore.IsInSignalState(); }

    virtual void Initialize() = 0;
    virtual void Destroy() = 0;
    virtual void Update() = 0;

    virtual GPUBufferHolderBase *GetGPUBufferHolder() const { return nullptr; }

    virtual Name GetTypeName() const override = 0;

    void SetNeedsUpdate();

    bool                            m_is_initialized;
    uint32                          m_buffer_index;
    void                            *m_buffer_address;

private:
    void AcquireBufferIndex();
    void ReleaseBufferIndex();

    ResourceMemoryPoolHandle        m_pool_handle;

    AtomicVar<int16>                m_ref_count;
    AtomicVar<int16>                m_update_counter;

    PreInitSemaphore                m_pre_init_semaphore;
    InitSemaphore                   m_init_semaphore;
    CompletionSemaphore             m_completion_semaphore;

#ifdef HYP_ENABLE_MT_CHECK
    DataRaceDetector                m_data_race_detector;
#endif
};

} // namespace hyperion

#endif