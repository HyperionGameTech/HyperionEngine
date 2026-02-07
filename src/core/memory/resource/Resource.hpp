/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/threading/AtomicVar.hpp>
#include <core/threading/Semaphore.hpp>
#include <core/threading/DataRaceDetector.hpp>
#include <core/threading/ConditionVariable.hpp>
#include <core/threading/SharedMutex.hpp>

#include <core/functional/Proc.hpp>

#include <core/memory/UniquePtr.hpp>

#include <core/memory/allocator/SlabAllocator.hpp>
#include <core/memory/pool/Pool.hpp>
#include <core/Name.hpp>

#include <core/Types.hpp>

namespace Hyperion {

HYP_API extern Pool* g_resourcePool;

class ResourceBase;

class IResourceMemoryPool;

template <class T>
class ResourceMemoryPool;

struct ResourceGuard
{
    enum
    {
        Read = 0x1,
        Write = 0x2
    };
    
    ResourceBase* resource;
    int mask;

    ResourceGuard()
        : resource(nullptr),
          mask(0)
    {
    }

    explicit ResourceGuard(ResourceBase& resource, int mask = Read);

    ResourceGuard(const ResourceGuard& other);
    ResourceGuard& operator=(const ResourceGuard& other);
    
    ResourceGuard(ResourceGuard&& other) noexcept;
    ResourceGuard& operator=(ResourceGuard&& other) noexcept;

    ~ResourceGuard();

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return resource != nullptr && mask != 0;
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return !bool(*this);
    }

    void Release();
};

template <class T>
struct TResourceGuard : ResourceGuard
{
    TResourceGuard()
        : ResourceGuard()
    {
    }

    explicit TResourceGuard(T& resource, int mask = Read)
        : ResourceGuard(resource, mask)
    {
    }

    T& operator->() const
    {
        return *static_cast<T*>(resource);
    }

    T* operator*() const
    {
        return static_cast<T*>(resource);
    }
};

class HYP_API ResourceBase
{
protected:
    ResourceBase();
    virtual ~ResourceBase();

public:
    friend struct ResourceGuard;

    ResourceBase(const ResourceBase& other) = delete;
    ResourceBase& operator=(const ResourceBase& other) = delete;
    ResourceBase(ResourceBase&& other) noexcept = delete;
    ResourceBase& operator=(ResourceBase&& other) noexcept = delete;

    ResourceGuard GetWriteScope();
    ResourceGuard GetReadScope();
    
    void AddWriter(bool doInitialize = true);
    void ReleaseWriter(bool doDeinitialize = true);

    void AddReader();
    void ReleaseReader();

protected:
    virtual void Initialize() = 0;
    virtual void Destroy() = 0;

private:
    volatile int64 m_state;

    mutable Mutex m_initMutex;
    ConditionVariable m_initCV;
    bool m_isInitialized;
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

        // Invoke the destructor.
        // Waits for reads to complete. BEWARE, will deadlock if reading on same thread as freeing.
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

} // namespace Hyperion
