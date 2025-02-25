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
class RenderResourcesBase : public ResourceBase
{
public:
    RenderResourcesBase();

    RenderResourcesBase(const RenderResourcesBase &other)                   = delete;
    RenderResourcesBase &operator=(const RenderResourcesBase &other)        = delete;

    RenderResourcesBase(RenderResourcesBase &&other) noexcept;
    RenderResourcesBase &operator=(RenderResourcesBase &&other) noexcept    = delete;

    virtual ~RenderResourcesBase() override;

    /*! \note Only call from render thread or from task on a task thread that is initiated by the render thread. */
    HYP_FORCE_INLINE uint32 GetBufferIndex() const
        { return m_buffer_index; }

    /*! \note Only call from render thread or from task on a task thread that is initiated by the render thread. */
    HYP_FORCE_INLINE void *GetBufferAddress() const
        { return m_buffer_address; }

protected:
    virtual IThread *GetOwnerThread() const override final;
    virtual bool CanExecuteInline() const override final;
    virtual void FlushScheduledTasks() override final;
    virtual void EnqueueOp(Proc<void> &&proc) override final;

    virtual void Initialize() override final;
    virtual void Destroy() override final;
    virtual void Update() override final;

    virtual void Initialize_Internal() = 0;
    virtual void Destroy_Internal() = 0;
    virtual void Update_Internal() = 0;

    virtual GPUBufferHolderBase *GetGPUBufferHolder() const { return nullptr; }

    virtual Name GetTypeName() const override = 0;

    uint32  m_buffer_index;
    void    *m_buffer_address;

private:
    void AcquireBufferIndex();
    void ReleaseBufferIndex();
};

} // namespace hyperion

#endif