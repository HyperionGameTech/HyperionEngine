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

    virtual Name GetTypeName() const = 0;

    virtual bool IsNull() const = 0;

    virtual int Claim() = 0;
    virtual int Unclaim() = 0;

    virtual void WaitForCompletion() = 0;

    virtual ResourceMemoryPoolHandle GetPoolHandle() const = 0;
    virtual void SetPoolHandle(ResourceMemoryPoolHandle pool_handle) = 0;
};

class IResourceMemoryPool
{
public:
    virtual ~IResourceMemoryPool() = default;
};

extern HYP_API IResourceMemoryPool *GetOrCreateResourceMemoryPool(TypeID type_id, UniquePtr<IResourceMemoryPool>(*create_fn)(void));

template <class T>
class ResourceMemoryPool final : private MemoryPool<ValueStorage<T>>, public IResourceMemoryPool
{
public:
    static_assert(std::is_base_of_v<IResource, T>, "T must be a subclass of IResource");

    using Base = MemoryPool<ValueStorage<T>>;

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
        AssertThrow(pool_handle);

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

private:
    IResource   *resource;
};

template <class T>
class TResourceHandle
{
public:
    TResourceHandle()   = default;

    TResourceHandle(T &resource)
        : handle(resource)
    {
        static_assert(std::is_base_of_v<IResource, T>, "T must be a subclass of IResource");
    }

    TResourceHandle(const TResourceHandle &other)
        : handle(other.handle)
    {
    }

    TResourceHandle &operator=(const TResourceHandle &other)
    {
        if (this == &other) {
            return *this;
        }

        handle = other.handle;

        return *this;
    }

    TResourceHandle(TResourceHandle &&other) noexcept
        : handle(std::move(other.handle))
    {
    }

    TResourceHandle &operator=(TResourceHandle &&other) noexcept
    {
        if (this == &other) {
            return *this;
        }

        handle = std::move(other.handle);

        return *this;
    }

    ~TResourceHandle()  = default;

    HYP_FORCE_INLINE void Reset()
    {
        handle.Reset();
    }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        // Check if the handle is not null and not the null resource.
        return bool(handle);
    }

    HYP_FORCE_INLINE operator ResourceHandle &() &
        { return handle; }

    HYP_FORCE_INLINE operator const ResourceHandle &() const &
        { return handle; }

    HYP_FORCE_INLINE operator ResourceHandle &&() &&
        { return std::move(handle); }

    HYP_FORCE_INLINE bool operator!() const
        { return !handle; }

    HYP_FORCE_INLINE bool operator==(const TResourceHandle &other) const
        { return handle == other.handle; }

    HYP_FORCE_INLINE bool operator!=(const TResourceHandle &other) const
        { return handle != other.handle; }

    HYP_FORCE_INLINE T *Get() const
    {
        IResource &ptr = *handle;

        if (ptr.IsNull()) {
            return nullptr;
        }

        // can safely cast to T since we know it's not NullResource
        return static_cast<T *>(&ptr);
    }

    HYP_FORCE_INLINE T *operator->() const
    {
        return Get();
    }

    HYP_FORCE_INLINE T &operator*() const
    {
        T *ptr = Get();

        if (!ptr) {
            HYP_FAIL("Dereferenced null resource handle");
        }

        // can safely cast to T since we know it's not null
        return *ptr;
    }

private:
    ResourceHandle  handle;
};

} // namespace hyperion

#endif