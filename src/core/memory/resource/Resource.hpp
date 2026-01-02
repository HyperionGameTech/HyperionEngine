/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/threading/AtomicVar.hpp>
#include <core/threading/Semaphore.hpp>
#include <core/threading/DataRaceDetector.hpp>
#include <core/threading/ConditionVariable.hpp>

#include <core/functional/Proc.hpp>

#include <core/memory/UniquePtr.hpp>

#include <core/memory/allocator/SlabAllocator.hpp>
#include <core/memory/pool/Pool.hpp>
#include <core/Name.hpp>

#include <core/Types.hpp>

namespace Hyperion {

HYP_API extern Pool* g_resourcePool;

class IResourceMemoryPool;

template <class T>
class ResourceMemoryPool;

class IResource
{
public:
    virtual ~IResource() = default;

    virtual bool IsNull() const = 0;

    virtual int IncRef() = 0;
    virtual int DecRef() = 0;

    /*! \brief Waits for ref count to be 0 and all tasks to be completed.
     *  If any ResourceGuard objects are still alive, this will block until they are destroyed.
     *  \note Ensure the current thread does not hold any ResourceGuard objects when calling this function, or it will deadlock. */
    virtual void WaitForFinalization() const = 0;
};

class HYP_API ResourceBase : public IResource
{
protected:
    ResourceBase();
    ~ResourceBase();

public:
    ResourceBase(const ResourceBase& other) = delete;
    ResourceBase& operator=(const ResourceBase& other) = delete;
    ResourceBase(ResourceBase&& other) noexcept = delete;
    ResourceBase& operator=(ResourceBase&& other) noexcept = delete;

    virtual bool IsNull() const override final
    {
        return false;
    }

    virtual int IncRef() override final;
    virtual int DecRef() override final;

    /*! \brief Wait for the resource to no longer be in loaded state */
    virtual void WaitForFinalization() const override final;

    bool IsInitialized() const;

protected:
    virtual void Initialize() = 0;
    virtual void Destroy() = 0;

protected:
    int m_refCount;

private:
    ConditionVariable m_isAliveCV;
    mutable Mutex m_mutex;
    mutable volatile int32 m_isAlive;
};

class IResourceMemoryPool
{
public:
    virtual ~IResourceMemoryPool() = default;

    virtual void Free(void* ptr) = 0;
};

extern HYP_API IResourceMemoryPool* GetOrCreateResourceMemoryPool(TypeId typeId, UniquePtr<IResourceMemoryPool> (*createFn)(void));

template <class T>
class ResourceMemoryPool final : public IResourceMemoryPool
{
    using AllocatorType = TSlabAllocator<AllocatorInstance<Pool, &g_resourcePool>>;

public:
    static_assert(std::is_base_of_v<IResource, T>, "T must be a subclass of IResource");

    static ResourceMemoryPool<T>* GetInstance()
    {
        static IResourceMemoryPool* s_pool = GetOrCreateResourceMemoryPool(TypeId::ForType<T>(), []() -> UniquePtr<IResourceMemoryPool>
            {
                return MakeUnique<ResourceMemoryPool<T>>();
            });

        return static_cast<ResourceMemoryPool<T>*>(s_pool);
    }

    ResourceMemoryPool()
        : m_allocator(sizeof(T), alignof(T), /* blocksPerSlab */ 256, /* flags */ AF_THREAD_SAFE)
    {
    }

    virtual ~ResourceMemoryPool() override = default;

    template <class... Args>
    T* Allocate(Args&&... args)
    {
        T* ptr = static_cast<T*>(m_allocator.Allocate());
        HYP_CORE_ASSERT(ptr != nullptr, "Failed to allocate resource from pool");

        new (ptr) T(std::forward<Args>(args)...);

        return ptr;
    }

    virtual void Free(void* ptr) override
    {
        Free_Internal(reinterpret_cast<T*>(ptr));
    }

private:
    void Free_Internal(T* ptr)
    {
        HYP_CORE_ASSERT(ptr != nullptr);

        ptr->WaitForFinalization();

        // Invoke the destructor
        ptr->~T();

        m_allocator.Free(ptr);
    }

    AllocatorType m_allocator;
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

class ResourceGuard
{
public:
    ResourceGuard()
        : m_resource(&GetNullResource())
    {
    }

    /*! \brief Construct a ResourceGuard using the given resource. The resource will have its ref count incremented if it is not null. */
    ResourceGuard(IResource& resource)
        : m_resource(&resource)
    {
        if (!resource.IsNull())
        {
            resource.IncRef();
        }
    }

    ResourceGuard(const ResourceGuard& other)
        : m_resource(other.m_resource)
    {
        if (!m_resource->IsNull())
        {
            m_resource->IncRef();
        }
    }

    ResourceGuard& operator=(const ResourceGuard& other)
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

    ResourceGuard(ResourceGuard&& other) noexcept
        : m_resource(other.m_resource)
    {
        other.m_resource = &GetNullResource();
    }

    ResourceGuard& operator=(ResourceGuard&& other) noexcept
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
        other.m_resource = &GetNullResource();

        return *this;
    }

    ~ResourceGuard()
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

    HYP_FORCE_INLINE bool operator==(const ResourceGuard& other) const
    {
        return m_resource == other.m_resource;
    }

    HYP_FORCE_INLINE bool operator!=(const ResourceGuard& other) const
    {
        return m_resource != other.m_resource;
    }

    HYP_FORCE_INLINE IResource* operator->() const
    {
        return m_resource;
    }

    HYP_FORCE_INLINE IResource& operator*() const
    {
        HYP_CORE_ASSERT(!m_resource->IsNull());

        return *m_resource;
    }

protected:
    IResource* m_resource;
};

template <class ResourceType>
class TResourceGuard : public ResourceGuard
{
public:
    TResourceGuard() = default;

    template <class T, typename = std::enable_if_t<!std::is_base_of_v<ResourceGuard, NormalizedType<T>>>>
    TResourceGuard(T& resource)
        : ResourceGuard(static_cast<IResource&>(resource))
    {
    }

    TResourceGuard(const TResourceGuard& other)
        : ResourceGuard(static_cast<const ResourceGuard&>(other))
    {
    }

    TResourceGuard& operator=(const TResourceGuard& other)
    {
        if (this == &other)
        {
            return *this;
        }

        ResourceGuard::operator=(static_cast<const ResourceGuard&>(other));

        return *this;
    }

    TResourceGuard(TResourceGuard&& other) noexcept
        : ResourceGuard(static_cast<ResourceGuard&&>(std::move(other)))
    {
    }

    TResourceGuard& operator=(TResourceGuard&& other) noexcept
    {
        if (this == &other)
        {
            return *this;
        }

        ResourceGuard::operator=(static_cast<ResourceGuard&&>(std::move(other)));

        return *this;
    }

    ~TResourceGuard() = default;

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
            HYP_FAIL("Dereferencing null resource handle!");
        }

        return *ptr;
    }
};

} // namespace Hyperion
