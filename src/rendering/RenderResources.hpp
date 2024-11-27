/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDER_RESOURCES_HPP
#define HYPERION_RENDER_RESOURCES_HPP

#include <core/threading/Threads.hpp>
#include <core/threading/AtomicVar.hpp>
#include <core/threading/Semaphore.hpp>
#include <core/threading/DataRaceDetector.hpp>

#include <core/functional/Proc.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <Types.hpp>

namespace hyperion {

// Represents the objects an engine object (e.g Material) uses while it is currently being rendered.
// The resources are reference counted internally, so as long as the object is being used for rendering somewhere,
// the resources will remain in memory.

class RenderResourcesBase : public EnableRefCountedPtrFromThis<RenderResourcesBase>
{
    using PreInitSemaphore = Semaphore<int32, SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE, threading::detail::AtomicSemaphoreImpl<int32, SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE>>;
    using InitSemaphore = Semaphore<int32, SemaphoreDirection::WAIT_FOR_POSITIVE, threading::detail::AtomicSemaphoreImpl<int32, SemaphoreDirection::WAIT_FOR_POSITIVE>>;

public:
    RenderResourcesBase();

    RenderResourcesBase(const RenderResourcesBase &other)                   = delete;
    RenderResourcesBase &operator=(const RenderResourcesBase &other)        = delete;

    RenderResourcesBase(RenderResourcesBase &&other) noexcept;
    RenderResourcesBase &operator=(RenderResourcesBase &&other) noexcept    = delete;

    virtual ~RenderResourcesBase();

    void Claim();
    void Unclaim();

    HYP_FORCE_INLINE uint32 GetBufferIndex() const
    {
#ifdef HYP_DEBUG_MODE
        // Allow render thread or task thread (under render thread) to call this method.
        Threads::AssertOnThread(ThreadName::THREAD_RENDER | ThreadName::THREAD_TASK);
#endif

        return m_buffer_index;
    }

protected:
    HYP_FORCE_INLINE bool IsInitialized() const
        { return m_init_semaphore.IsInSignalState(); }

    virtual void Initialize() = 0;
    virtual void Destroy() = 0;
    virtual void Update() = 0;

    virtual uint32 AcquireBufferIndex() const = 0;
    virtual void ReleaseBufferIndex(uint32 buffer_index) const = 0;

    /*! \brief Performs an operation on the render thread if the resources are initialized,
     *  otherwise executes it immediately on the calling thread. Initialization on the render thread will not begin until at least the end of the given proc,
     *  so it is safe to use this method on any thread.
     *  \param proc The operation to perform.
     *  \param force_render_thread If true, the operation will be performed on the render thread regardless of initialization state. */
    void Execute(Proc<void> &&proc, bool force_render_thread = false);

    void SetNeedsUpdate();

    bool                m_is_initialized;
    uint32              m_buffer_index;

private:
    AtomicVar<int16>    m_ref_count;
    AtomicVar<int16>    m_update_counter;
    PreInitSemaphore    m_pre_init_semaphore;
    InitSemaphore       m_init_semaphore;

#ifdef HYP_ENABLE_MT_CHECK
    DataRaceDetector    m_data_race_detector;
#endif
};

class RenderResourcesHandle
{
public:
    RenderResourcesHandle()
        : render_resources(nullptr)
    {
    }

    RenderResourcesHandle(RenderResourcesBase &render_resources)
        : render_resources(&render_resources)
    {
        render_resources.Claim();
    }

    RenderResourcesHandle(const RenderResourcesHandle &other)
        : render_resources(other.render_resources)
    {
        if (render_resources != nullptr) {
            render_resources->Claim();
        }
    }

    RenderResourcesHandle &operator=(const RenderResourcesHandle &other)
    {
        if (this == &other || render_resources == other.render_resources) {
            return *this;
        }

        if (render_resources != nullptr) {
            render_resources->Unclaim();
        }

        render_resources = other.render_resources;

        if (render_resources != nullptr) {
            render_resources->Claim();
        }

        return *this;
    }

    RenderResourcesHandle(RenderResourcesHandle &&other) noexcept
        : render_resources(other.render_resources)
    {
        other.render_resources = nullptr;
    }

    RenderResourcesHandle &operator=(RenderResourcesHandle &&other) noexcept
    {
        if (this == &other) {
            return *this;
        }

        if (render_resources != nullptr) {
            render_resources->Unclaim();
        }

        render_resources = other.render_resources;
        other.render_resources = nullptr;

        return *this;
    }

    ~RenderResourcesHandle()
    {
        if (render_resources != nullptr) {
            render_resources->Unclaim();
        }
    }

    void Reset()
    {
        if (render_resources != nullptr) {
            render_resources->Unclaim();
        }

        render_resources = nullptr;
    }

    HYP_FORCE_INLINE explicit operator bool() const
        { return render_resources != nullptr; }

    HYP_FORCE_INLINE bool operator!() const
        { return !render_resources; }

    HYP_FORCE_INLINE bool operator==(const RenderResourcesHandle &other) const
        { return render_resources == other.render_resources; }

    HYP_FORCE_INLINE bool operator!=(const RenderResourcesHandle &other) const
        { return render_resources != other.render_resources; }

    HYP_FORCE_INLINE RenderResourcesBase *operator->() const
        { return render_resources; }

    HYP_FORCE_INLINE RenderResourcesBase &operator*() const
        { return *render_resources; }

private:
    RenderResourcesBase *render_resources;
};

template <class T>
class TRenderResourcesHandle
{
public:
    static_assert(std::is_base_of_v<RenderResourcesBase, T>, "T must be a subclass of RenderResourcesBase");

    TRenderResourcesHandle()    = default;

    TRenderResourcesHandle(T &render_resources)
        : handle(render_resources)
    {
    }

    TRenderResourcesHandle(const TRenderResourcesHandle &other)
        : handle(other.handle)
    {
    }

    TRenderResourcesHandle &operator=(const TRenderResourcesHandle &other)
    {
        if (this == &other) {
            return *this;
        }

        handle = other.handle;

        return *this;
    }

    TRenderResourcesHandle(TRenderResourcesHandle &&other) noexcept
        : handle(std::move(other.handle))
    {
    }

    TRenderResourcesHandle &operator=(TRenderResourcesHandle &&other) noexcept
    {
        if (this == &other) {
            return *this;
        }

        handle = std::move(other.handle);

        return *this;
    }

    ~TRenderResourcesHandle()   = default;

    HYP_FORCE_INLINE void Reset()
        { handle.Reset();  }

    HYP_FORCE_INLINE explicit operator bool() const
        { return bool(handle); }

    HYP_FORCE_INLINE bool operator!() const
        { return !handle; }

    HYP_FORCE_INLINE bool operator==(const TRenderResourcesHandle &other) const
        { return handle == other.handle; }

    HYP_FORCE_INLINE bool operator!=(const TRenderResourcesHandle &other) const
        { return handle != other.handle; }

    HYP_FORCE_INLINE T *operator->() const
        { return static_cast<T *>(handle.operator->()); }

    HYP_FORCE_INLINE T &operator*() const
        { return static_cast<T &>(handle.operator*()); }

private:
    RenderResourcesHandle   handle;
};

} // namespace hyperion

#endif