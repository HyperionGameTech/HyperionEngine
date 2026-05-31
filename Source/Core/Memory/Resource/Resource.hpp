/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Threading/AtomicVar.hpp>
#include <Core/Threading/AtomicFlag.hpp>
#include <Core/Threading/Semaphore.hpp>
#include <Core/Threading/DataRaceDetector.hpp>
#include <Core/Threading/ConditionVariable.hpp>
#include <Core/Threading/SharedMutex.hpp>

#include <Core/Functional/Proc.hpp>

#include <Core/Memory/UniquePtr.hpp>

#include <Core/Memory/Allocator/SlabAllocator.hpp>
#include <Core/Memory/Pool/Pool.hpp>
#include <Core/Name/Name.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

class ResourceBase;

class IResourceMemoryPool;

template <class T>
class ResourceMemoryPool;

struct CORE_API ResourceGuard
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

class CORE_API ResourceBase
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
    AtomicFlag m_isInitialized;
};

} // namespace Hyperion
