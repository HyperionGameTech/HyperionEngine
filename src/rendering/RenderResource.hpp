/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDER_RESOURCE_HPP
#define HYPERION_RENDER_RESOURCE_HPP

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

// Represents the objects an engine object (e.g Material) uses while it is currently being used by the renderer in some way.
// The resources are reference counted internally, so as long as the object is being used for rendering somewhere,
// the resources will remain in memory.

/// \TODO: Refactor this so that changes are immediately applied to the data on the thread that sets it, but it marks it for needing update
/// on the render thread. updates will be applied during sync points between the game and render thread, rather than enqueing a task to update the data.
/// This will reduce the amount of atomics needed to synchronize the data between the game and render thread, as well as reduce the amount of tasks that need to be enqueued.

class RenderResourceBase : public ResourceBase
{
public:
    RenderResourceBase();

    RenderResourceBase(const RenderResourceBase& other) = delete;
    RenderResourceBase& operator=(const RenderResourceBase& other) = delete;

    RenderResourceBase(RenderResourceBase&& other) noexcept;
    RenderResourceBase& operator=(RenderResourceBase&& other) noexcept = delete;

    virtual ~RenderResourceBase() override;

    /*! \note Only call from render thread or from task on a task thread that is initiated by the render thread. */
    HYP_FORCE_INLINE uint32 GetBufferIndex() const
    {
        return m_buffer_index;
    }

    /*! \note Only call from render thread or from task on a task thread that is initiated by the render thread. */
    HYP_FORCE_INLINE void* GetBufferAddress() const
    {
        return m_buffer_address;
    }

protected:
    virtual IThread* GetOwnerThread() const override final;
    virtual bool CanExecuteInline() const override final;
    virtual void FlushScheduledTasks() const override final;
    virtual void EnqueueOp(Proc<void()>&& proc) override final;

    virtual void Initialize() override final;
    virtual void Destroy() override final;
    virtual void Update() override final;

    virtual void Initialize_Internal() = 0;
    virtual void Destroy_Internal() = 0;
    virtual void Update_Internal() = 0;

    virtual GPUBufferHolderBase* GetGPUBufferHolder() const
    {
        return nullptr;
    }

    uint32 m_buffer_index;
    void* m_buffer_address;

private:
    void AcquireBufferIndex();
    void ReleaseBufferIndex();
};

} // namespace hyperion

#endif