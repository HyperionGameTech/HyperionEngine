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

static constexpr uint64 g_initialization_mask_initialized_bit = 0x1;
static constexpr uint64 g_initialization_mask_read_mask = uint64(-1) & ~g_initialization_mask_initialized_bit;

class IResourceMemoryPool;

template <class T>
class ResourceMemoryPool;

template <class T>
struct ResourceMemoryPoolInitInfo : MemoryPoolInitInfo
{
};

struct ResourceMemoryPoolHandle
{
    uint32 index = ~0u;

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return index != ~0u;
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return index == ~0u;
    }
};

// Represents the objects an engine object (e.g Material) uses while it is currently being rendered, streamed, or otherwise consuming resources.
class IResource
{
public:
    virtual ~IResource() = default;

    virtual bool IsNull() const = 0;

    virtual int IncRef(int count = 1) = 0;
    virtual int IncRefNoInitialize() = 0;
    virtual int DecRef() = 0;

    /*! \brief Waits for all tasks to be completed. */
    virtual void WaitForTaskCompletion() const = 0;

    /*! \brief Waits for ref count to be 0 and all tasks to be completed.
     *  If any ResourceHandle objects are still alive, this will block until they are destroyed.
     *  \note Ensure the current thread does not hold any ResourceHandle objects when calling this function, or it will deadlock. */
    virtual void WaitForFinalization() const = 0;

    virtual ResourceMemoryPoolHandle GetPoolHandle() const = 0;
    virtual void SetPoolHandle(ResourceMemoryPoolHandle pool_handle) = 0;
};

/*! \brief Basic abstract implementation of IResource, using reference counting to manage the lifetime of the underlying resources. */
class HYP_API ResourceBase : public IResource
{
protected:
    using RefCounter = Semaphore<int32, SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE, threading::detail::AtomicSemaphoreImpl<int32, SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE>>;
    using CompletionSemaphore = Semaphore<int32, SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE, threading::detail::ConditionVarSemaphoreImpl<int32, SemaphoreDirection::WAIT_FOR_ZERO_OR_NEGATIVE>>;

public:
    ResourceBase();

    ResourceBase(const ResourceBase& other) = delete;
    ResourceBase& operator=(const ResourceBase& other) = delete;

    ResourceBase(ResourceBase&& other) noexcept;
    ResourceBase& operator=(ResourceBase&& other) noexcept = delete;

    virtual ~ResourceBase() override;

    HYP_FORCE_INLINE int32 NumRefs() const
    {
        return m_ref_counter.GetValue();
    }

    virtual bool IsNull() const override final
    {
        return false;
    }

    virtual int IncRef(int count = 1) override final;
    virtual int DecRef() override final;

    /*! \brief Wait for all tasks to be completed. */
    virtual void WaitForTaskCompletion() const override final;

    /*! \brief Wait for ref count to be 0 and all tasks to be completed. Will also wait for any tasks that are currently executing to complete.
     *  If any ResourceHandle objects are still alive, this will block until they are destroyed.
     *  \note Ensure the current thread does not hold any ResourceHandle objects when calling this function, or it will deadlock. */
    virtual void WaitForFinalization() const override final;

    virtual ResourceMemoryPoolHandle GetPoolHandle() const override final
    {
        return m_pool_handle;
    }

    virtual void SetPoolHandle(ResourceMemoryPoolHandle pool_handle) override final
    {
        m_pool_handle = pool_handle;
    }

    /*! \brief Performs an transactional operation on this Resource. Executes on the owner thread if the resources are initialized,
     *  otherwise executes it inline on the calling thread. Initialization on the owner thread will not begin until at least the end of the given proc,
     *  so it is safe to use this method on any thread.
     *  \param proc The operation to perform.
     *  \param force_owner_thread If true, the operation will be performed on the owner thread regardless of initialization state. */
    template <class Function>
    void Execute(Function&& function, bool force_owner_thread = false)
    {
        m_completion_semaphore.Produce(1);

        if (!force_owner_thread)
        {
            if (m_ref_counter.IsInSignalState())
            {
                // Try to add read access - if it can't be acquired, we are in the process of initializing
                // and thus we will remove our read access and enqueue the operation
                if (!(m_initialization_mask.Increment(2, MemoryOrder::ACQUIRE) & g_initialization_mask_initialized_bit))
                {
                    {
                        // Check again, as it might have been initialized in between the initial check and the increment
                        HYP_MT_CHECK_RW(m_data_race_detector);

                        // Execute inline if not initialized yet instead of pushing to owner thread
                        function();
                    }

                    // Remove our read access
                    m_initialization_mask.Decrement(2, MemoryOrder::RELAXED);

                    m_completion_semaphore.Release(1);

                    return;
                }

                // Remove our read access
                m_initialization_mask.Decrement(2, MemoryOrder::RELAXED);
            }
        }

        if (!CanExecuteInline())
        {
            EnqueueOp([this, f = std::forward<Function>(function)]() mutable
                {
                    {
                        HYP_MT_CHECK_RW(m_data_race_detector);

                        f();
                    }

                    m_completion_semaphore.Release(1);
                });

            return;
        }

        {
            HYP_MT_CHECK_RW(m_data_race_detector);

            function();
        }

        m_completion_semaphore.Release(1);
    }

protected:
    // Needed to increment ref count for resources that are initialized in LOADED state.
    // We can't call Initialize() because it is a virtual function and the object might not be fully constructed yet.
    virtual int IncRefNoInitialize() override final;

    bool IsInitialized() const;

    virtual IThread* GetOwnerThread() const
    {
        return nullptr;
    }

    virtual bool CanExecuteInline() const;
    virtual void FlushScheduledTasks() const;
    virtual void EnqueueOp(Proc<void()>&& proc);

    virtual void Initialize() = 0;
    virtual void Destroy() = 0;
    virtual void Update() = 0;

    void SetNeedsUpdate();

protected:
    AtomicVar<int16> m_update_counter;

    RefCounter m_ref_counter;
    CompletionSemaphore m_completion_semaphore;

    ResourceMemoryPoolHandle m_pool_handle;

    HYP_DECLARE_MT_CHECK(m_data_race_detector);

private:
    AtomicVar<uint64> m_initialization_mask;
};

class IResourceMemoryPool
{
public:
    virtual ~IResourceMemoryPool() = default;
};

extern HYP_API IResourceMemoryPool* GetOrCreateResourceMemoryPool(TypeID type_id, UniquePtr<IResourceMemoryPool> (*create_fn)(void));

template <class T>
class ResourceMemoryPool final : private MemoryPool<ValueStorage<T>, ResourceMemoryPoolInitInfo<T>>, public IResourceMemoryPool
{
public:
    static_assert(std::is_base_of_v<IResource, T>, "T must be a subclass of IResource");

    using Base = MemoryPool<ValueStorage<T>, ResourceMemoryPoolInitInfo<T>>;

    static ResourceMemoryPool<T>* GetInstance()
    {
        static IResourceMemoryPool* pool = GetOrCreateResourceMemoryPool(TypeID::ForType<T>(), []() -> UniquePtr<IResourceMemoryPool>
            {
                return MakeUnique<ResourceMemoryPool<T>>();
            });

        return static_cast<ResourceMemoryPool<T>*>(pool);
    }

    ResourceMemoryPool()
        : Base()
    {
    }

    virtual ~ResourceMemoryPool() override = default;

    template <class... Args>
    T* Allocate(Args&&... args)
    {
        ValueStorage<T>* element;
        const uint32 index = Base::AcquireIndex(&element);

        T* ptr = element->Construct(std::forward<Args>(args)...);
        ptr->SetPoolHandle(ResourceMemoryPoolHandle { index });

        return ptr;
    }

    void Free(T* ptr)
    {
        AssertThrow(ptr != nullptr);

        ptr->WaitForFinalization();

        const ResourceMemoryPoolHandle pool_handle = ptr->GetPoolHandle();
        AssertThrowMsg(pool_handle, "Resource has no pool handle set - the resource was likely not allocated using the pool");

        // Invoke the destructor
        ptr->~T();

        Base::ReleaseIndex(pool_handle.index);
    }
};

template <class T, class... Args>
HYP_FORCE_INLINE static T* AllocateResource(Args&&... args)
{
    return ResourceMemoryPool<T>::GetInstance()->Allocate(std::forward<Args>(args)...);
}

template <class T>
HYP_FORCE_INLINE static void FreeResource(T* ptr)
{
    if (!ptr)
    {
        return;
    }

    ResourceMemoryPool<T>::GetInstance()->Free(ptr);
}

HYP_API IResource& GetNullResource();

class ResourceHandle
{
public:
    ResourceHandle()
        : m_resource(&GetNullResource())
    {
    }

    /*! \brief Construct a ResourceHandle using the given resource. The resource will have its ref count incremented if it is not null.
     *  If \ref{should_initialize} is true (default), the resource will be initialized.
     *  Otherwise, IncRefNoInitialize() will be called (this should only be used when required, like in the constructor of base classes that have Initialize() as a virtual method). */
    ResourceHandle(IResource& resource, bool should_initialize = true)
        : m_resource(&resource)
    {
        if (!resource.IsNull())
        {
            if (should_initialize)
            {
                resource.IncRef();
            }
            else
            {
                resource.IncRefNoInitialize();
            }
        }
    }

    ResourceHandle(const ResourceHandle& other)
        : m_resource(other.m_resource)
    {
        if (!m_resource->IsNull())
        {
            m_resource->IncRef();
        }
    }

    ResourceHandle& operator=(const ResourceHandle& other)
    {
        if (this == &other || m_resource == other.m_resource)
        {
            return *this;
        }

        if (!m_resource->IsNull())
        {
            m_resource->DecRef();
        }

        m_resource = other.m_resource;

        if (!m_resource->IsNull())
        {
            m_resource->IncRef();
        }

        return *this;
    }

    ResourceHandle(ResourceHandle&& other) noexcept
        : m_resource(other.m_resource)
    {
        other.m_resource = &GetNullResource();
    }

    ResourceHandle& operator=(ResourceHandle&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        if (!m_resource->IsNull())
        {
            m_resource->DecRef();
        }

        m_resource = other.m_resource;
        other.m_resource = &GetNullResource();

        return *this;
    }

    ~ResourceHandle()
    {
        if (!m_resource->IsNull())
        {
            m_resource->DecRef();
        }
    }

    void Reset()
    {
        if (!m_resource->IsNull())
        {
            m_resource->DecRef();

            m_resource = &GetNullResource();
        }
    }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return !m_resource->IsNull();
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return m_resource->IsNull();
    }

    HYP_FORCE_INLINE bool operator==(const ResourceHandle& other) const
    {
        return m_resource == other.m_resource;
    }

    HYP_FORCE_INLINE bool operator!=(const ResourceHandle& other) const
    {
        return m_resource != other.m_resource;
    }

    HYP_FORCE_INLINE IResource* operator->() const
    {
        return m_resource;
    }

    HYP_FORCE_INLINE IResource& operator*() const
    {
        AssertDebug(!m_resource->IsNull());

        return *m_resource;
    }

protected:
    IResource* m_resource;
};

template <class ResourceType>
class TResourceHandle : public ResourceHandle
{
public:
    TResourceHandle() = default;

    template <class T, typename = std::enable_if_t<!std::is_base_of_v<ResourceHandle, NormalizedType<T>>>>
    TResourceHandle(T& resource)
        : ResourceHandle(static_cast<IResource&>(resource))
    {
    }

    TResourceHandle(const TResourceHandle& other)
        : ResourceHandle(static_cast<const ResourceHandle&>(other))
    {
    }

    TResourceHandle& operator=(const TResourceHandle& other)
    {
        if (this == &other)
        {
            return *this;
        }

        ResourceHandle::operator=(static_cast<const ResourceHandle&>(other));

        return *this;
    }

    TResourceHandle(TResourceHandle&& other) noexcept
        : ResourceHandle(static_cast<ResourceHandle&&>(std::move(other)))
    {
    }

    TResourceHandle& operator=(TResourceHandle&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        ResourceHandle::operator=(static_cast<ResourceHandle&&>(std::move(other)));

        return *this;
    }

    ~TResourceHandle() = default;

    HYP_FORCE_INLINE ResourceType* Get() const
    {
        static_assert(std::is_base_of_v<IResource, ResourceType>);

        if (m_resource->IsNull())
        {
            return nullptr;
        }

        // can safely cast to ResourceType since we know it's not NullResource
        return static_cast<ResourceType*>(m_resource);
    }

    HYP_FORCE_INLINE ResourceType* operator->() const
    {
        return Get();
    }

    HYP_FORCE_INLINE ResourceType& operator*() const
    {
        ResourceType* ptr = Get();

        if (!ptr)
        {
            HYP_FAIL("Dereferenced null resource handle");
        }

        return *ptr;
    }
};

} // namespace hyperion

#endif