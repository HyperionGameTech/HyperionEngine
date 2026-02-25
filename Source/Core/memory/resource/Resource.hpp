/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/threading/AtomicVar.hpp>
#include <Core/threading/Semaphore.hpp>
#include <Core/threading/DataRaceDetector.hpp>
#include <Core/threading/ConditionVariable.hpp>
#include <Core/threading/SharedMutex.hpp>

#include <Core/functional/Proc.hpp>

#include <Core/memory/UniquePtr.hpp>

#include <Core/memory/allocator/SlabAllocator.hpp>
#include <Core/memory/pool/Pool.hpp>
#include <Core/Name.hpp>

#include <Core/Types.hpp>

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

class ResourceBase
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

    void GetNumUsers(int64& outReaders, int64& outWriters) const;

protected:
    virtual void Initialize() = 0;
    virtual void Destroy() = 0;

private:
    volatile int64 m_state;

    mutable Mutex m_initMutex;
    ConditionVariable m_initCV;
    bool m_isInitialized;
};

} // namespace Hyperion
