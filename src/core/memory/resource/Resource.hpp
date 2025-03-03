/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_MEMORY_RESOURCE_HPP
#define HYPERION_CORE_MEMORY_RESOURCE_HPP

#include <core/threading/Threads.hpp>
#include <core/threading/AtomicVar.hpp>
#include <core/threading/Semaphore.hpp>
#include <core/threading/DataRaceDetector.hpp>

#include <core/functional/Proc.hpp>

#include <core/memory/RefCountedPtr.hpp>
#include <core/memory/UniquePtr.hpp>

#include <core/memory/MemoryPool.hpp>
#include <core/Name.hpp>

#include <Types.hpp>

namespace hyperion {

class IResourceMemoryPool;

template <class T>
class ResourceMemoryPool;

template <class T>
struct ResourceMemoryPoolInitInfo : MemoryPoolInitInfo
{
};

struct ResourceMemoryPoolHandle
{
    uint32  index = ~0u;

    HYP_FORCE_INLINE explicit operator bool() const
        { return index != ~0u; }

    HYP_FORCE_INLINE bool operator!() const
        { return index == ~0u; }
};

// Represents the objects an engine object (e.g Material) uses while it is currently being rendered, streamed, or otherwise consuming resources.
class IResource
{
public:
    virtual ~IResource() = default;

    virtual bool IsNull() const = 0;

    virtual int Claim() = 0;
    virtual int Unclaim() = 0;

    virtual void WaitForCompletion() = 0;

    virtual ResourceMemoryPoolHandle GetPoolHandle() const = 0;
    virtual void SetPoolHandle(ResourceMemoryPoolHandle pool_handle) = 0;
};

/*! \brief Basic abstract implementation of IResource, using reference counting to manage the lifetime of the underlying resources. */
class HYP_API ResourceBase : public IResource
{
protected:
    using PreInitSemaphore = Semaphore<int32, SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE, threading::detail::AtomicSemaphoreImpl<int32, SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE>>;
    using InitSemaphore = Semaphore<int32, SemaphoreDirection::WAIT_FOR_POSITIVE, threading::detail::AtomicSemaphoreImpl<int32, SemaphoreDirection::WAIT_FOR_POSITIVE>>;
    using CompletionSemaphore = Semaphore<int32, SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE, threading::detail::AtomicSemaphoreImpl<int32, SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE>>;

public:
    ResourceBase();

    ResourceBase(const ResourceBase &other)                 = delete;
    ResourceBase &operator=(const ResourceBase &other)      = delete;

    ResourceBase(ResourceBase &&other) noexcept;
    ResourceBase &operator=(ResourceBase &&other) noexcept  = delete;

    virtual ~ResourceBase() override;

    virtual bool IsNull() const override final
        { return false; }

    virtual int Claim() override final;
    virtual int Unclaim() override final;
    virtual void WaitForCompletion() override final;

    virtual ResourceMemoryPoolHandle GetPoolHandle() const override final
        { return m_pool_handle; }

    virtual void SetPoolHandle(ResourceMemoryPoolHandle pool_handle) override final
        { m_pool_handle = pool_handle; }

    /*! \brief Performs an operation on the owner thread if the resources are initialized,
     *  otherwise executes it immediately on the calling thread. Initialization on the owner thread will not begin until at least the end of the given proc,
     *  so it is safe to use this method on any thread.
     *  \param proc The operation to perform.
     *  \param force_owner_thread If true, the operation will be performed on the owner thread regardless of initialization state. */
    void Execute(Proc<void> &&proc, bool force_owner_thread = false);

#ifdef HYP_DEBUG_MODE
    HYP_FORCE_INLINE uint32 GetUseCount() const
        { return m_ref_count.Get(MemoryOrder::SEQUENTIAL); }
#endif

protected:
    int ClaimWithoutInitialize();

    HYP_FORCE_INLINE bool IsInitialized() const
        { return m_is_initialized; }

    virtual IThread *GetOwnerThread() const
        { return nullptr; }

    virtual bool CanExecuteInline() const;
    virtual void FlushScheduledTasks();
    virtual void EnqueueOp(Proc<void> &&proc);

    virtual void Initialize() = 0;
    virtual void Destroy() = 0;
    virtual void Update() = 0;

    void SetNeedsUpdate();

private:
    bool                        m_is_initialized;

    AtomicVar<int16>            m_ref_count;
    AtomicVar<int16>            m_update_counter;

    InitSemaphore               m_init_semaphore;
    PreInitSemaphore            m_pre_init_semaphore;
    CompletionSemaphore         m_completion_semaphore;

    ResourceMemoryPoolHandle    m_pool_handle;
    
    HYP_DECLARE_MT_CHECK(m_data_race_detector);
};

class IResourceMemoryPool
{
public:
    virtual ~IResourceMemoryPool() = default;
};

extern HYP_API IResourceMemoryPool *GetOrCreateResourceMemoryPool(TypeID type_id, UniquePtr<IResourceMemoryPool>(*create_fn)(void));

template <class T>
class ResourceMemoryPool final : private MemoryPool<ValueStorage<T>, ResourceMemoryPoolInitInfo<T>>, public IResourceMemoryPool
{
public:
    static_assert(std::is_base_of_v<IResource, T>, "T must be a subclass of IResource");

    using Base = MemoryPool<ValueStorage<T>, ResourceMemoryPoolInitInfo<T>>;

    static ResourceMemoryPool<T> *GetInstance()
    {
        static IResourceMemoryPool *pool = GetOrCreateResourceMemoryPool(TypeID::ForType<T>(), []() -> UniquePtr<IResourceMemoryPool>
        {
            return MakeUnique<ResourceMemoryPool<T>>();
        });
        
        return static_cast<ResourceMemoryPool<T> *>(pool);
    }

    ResourceMemoryPool()
        : Base()
    {
    }

    virtual ~ResourceMemoryPool() override = default;

    template <class... Args>
    T *Allocate(Args &&... args)
    {
        ValueStorage<T> *element;
        const uint32 index = Base::AcquireIndex(&element);

        T *ptr = element->Construct(std::forward<Args>(args)...);
        ptr->SetPoolHandle(ResourceMemoryPoolHandle { index });

        return ptr;
    }

    void Free(T *ptr)
    {
        AssertThrow(ptr != nullptr);

        // Wait for it to finish any tasks before destroying
        ptr->WaitForCompletion();

        const ResourceMemoryPoolHandle pool_handle = ptr->GetPoolHandle();
        AssertThrowMsg(pool_handle, "Resource has no pool handle set - the resource was likely not allocated using the pool");

        // Invoke the destructor
        ValueStorage<T> &element = Base::GetElement(pool_handle.index);
        element.Destruct();

        Base::ReleaseIndex(pool_handle.index);
    }
};

template <class T, class... Args>
HYP_FORCE_INLINE static T *AllocateResource(Args &&... args)
{
    return ResourceMemoryPool<T>::GetInstance()->Allocate(std::forward<Args>(args)...);
}

template <class T>
HYP_FORCE_INLINE static void FreeResource(T *ptr)
{
    if (!ptr) {
        return;
    }

    ResourceMemoryPool<T>::GetInstance()->Free(ptr);
}

HYP_API IResource &GetNullResource();

class ResourceHandle
{
public:
    ResourceHandle()
        : resource(&GetNullResource())
    {
    }

    ResourceHandle(IResource &resource)
        : resource(&resource)
    {
        resource.Claim();
    }

    ResourceHandle(const ResourceHandle &other)
        : resource(other.resource)
    {
        if (!resource->IsNull()) {
            resource->Claim();
        }
    }

    ResourceHandle &operator=(const ResourceHandle &other)
    {
        if (this == &other || resource == other.resource) {
            return *this;
        }

        if (!resource->IsNull()) {
            resource->Unclaim();
        }

        resource = other.resource;

        if (!resource->IsNull()) {
            resource->Claim();
        }

        return *this;
    }

    ResourceHandle(ResourceHandle &&other) noexcept
        : resource(other.resource)
    {
        other.resource = &GetNullResource();
    }

    ResourceHandle &operator=(ResourceHandle &&other) noexcept
    {
        if (this == &other) {
            return *this;
        }

        if (!resource->IsNull()) {
            resource->Unclaim();
        }

        resource = other.resource;
        other.resource = &GetNullResource();

        return *this;
    }

    ~ResourceHandle()
    {
        if (!resource->IsNull()) {
            resource->Unclaim();
        }
    }

    void Reset()
    {
        if (!resource->IsNull()) {
            resource->Unclaim();

            resource = &GetNullResource();
        }
    }

    HYP_FORCE_INLINE explicit operator bool() const
        { return !resource->IsNull(); }

    HYP_FORCE_INLINE bool operator!() const
        { return resource->IsNull(); }

    HYP_FORCE_INLINE bool operator==(const ResourceHandle &other) const
        { return resource == other.resource; }

    HYP_FORCE_INLINE bool operator!=(const ResourceHandle &other) const
        { return resource != other.resource; }

    HYP_FORCE_INLINE IResource *operator->() const
        { return resource; }

    HYP_FORCE_INLINE IResource &operator*() const
        { return *resource; }

protected:
    IResource   *resource;
};

template <class ResourceType>
class TResourceHandle : public ResourceHandle
{
public:
    TResourceHandle() = default;

    template <class T, typename = std::enable_if_t< (std::is_same_v<NormalizedType<T>, ResourceType> || std::is_base_of_v<ResourceType, NormalizedType<T>>) && (std::is_base_of_v<IResource, NormalizedType<T>>) > >
    TResourceHandle(T &resource)
        : ResourceHandle(static_cast<IResource &>(resource))
    {
    }

    TResourceHandle(const TResourceHandle &other)
        : ResourceHandle(static_cast<const ResourceHandle &>(other))
    {
    }

    TResourceHandle &operator=(const TResourceHandle &other)
    {
        if (this == &other) {
            return *this;
        }

        ResourceHandle::operator=(static_cast<const ResourceHandle &>(other));

        return *this;
    }

    TResourceHandle(TResourceHandle &&other) noexcept
        : ResourceHandle(static_cast<ResourceHandle &&>(std::move(other)))
    {
    }

    TResourceHandle &operator=(TResourceHandle &&other) noexcept
    {
        if (this == &other) {
            return *this;
        }

        ResourceHandle::operator=(static_cast<ResourceHandle &&>(std::move(other)));

        return *this;
    }

    ~TResourceHandle()  = default;

    HYP_FORCE_INLINE ResourceType *Get() const
    {
        if (resource->IsNull()) {
            return nullptr;
        }

        // can safely cast to ResourceType since we know it's not NullResource
        return static_cast<ResourceType *>(resource);
    }

    HYP_FORCE_INLINE ResourceType *operator->() const
    {
        return Get();
    }

    HYP_FORCE_INLINE ResourceType &operator*() const
    {
        ResourceType *ptr = Get();

        if (!ptr) {
            HYP_FAIL("Dereferenced null resource handle");
        }

        return *ptr;
    }
};

} // namespace hyperion

#endif