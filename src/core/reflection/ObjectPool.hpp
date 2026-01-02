/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>
#include <core/Constants.hpp>
#include <core/Types.hpp>

#include <core/reflection/ObjId.hpp>
#include <core/reflection/ObjectFwd.hpp>

#include <core/containers/SparsePagedArray.hpp>

#include <core/utilities/IdGenerator.hpp>

#include <core/threading/Mutex.hpp>
#include <core/threading/AtomicVar.hpp>
#include <core/threading/AtomicFlag.hpp>

#include <core/debug/Debug.hpp>

#include <core/memory/Memory.hpp>
#include <core/memory/pool/Pool.hpp>

#include <type_traits>

// #define HYP_OBJECT_POOL_DEBUG
namespace Hyperion {

template <class T>
class ObjectContainer;

class ObjectContainerBase;

struct ObjectHeader;
class Class;

HYP_API extern void ReleaseObject(ObjectHeader* header);

#ifdef HYP_DOTNET
HYP_API extern void Object_IncScriptObjectRef(class ObjectBase* ptr);
HYP_API extern void Object_DecScriptObjectRef(class ObjectBase* ptr);
#endif

class ObjectContainerBase
{
    friend class ObjectPool;

public:
    virtual ~ObjectContainerBase() = default;

    HYP_FORCE_INLINE const TypeId& GetObjectTypeId() const
    {
        return m_typeId;
    }

    HYP_FORCE_INLINE const Class* GetClass() const
    {
        return m_class;
    }

    virtual ObjectHeader* GetObjectHeader(uint32 index, TLockGuard<AtomicFlag>& outGuard) = 0;

    virtual void Release(ObjectHeader* header) = 0;

protected:
    enum PoolFlags : uint8
    {
        PF_NONE = 0x0,
        PF_WRITER = 0x1,
        PF_ALLOCATE = 0x2,
        PF_FREE = 0x4
    };

    ObjectContainerBase(TypeId typeId, const Class* cls);

    /*! \brief Checks that the current thread is the pool's owning thread, or locks the global pool lock if this is the global pool.
     *  \param outGuard If this is the global pool, the lock state will be stored here so it can be released later.
     */
    HYP_API Pool* GetPool() const;
    HYP_API static void LockPoolOrThreadAssert(Pool* pool, TLockGuard<AtomicFlag>& outGuard, int flags);

    TypeId m_typeId;
    const Class* m_class;
    IdGenerator m_idGenerator;
};

/*! \brief Metadata for a generic object in the object pool. */
struct ObjectHeader
{
    const Class* cls;
    uint32 index;
    volatile int32 refCountStrong;
    volatile int32 refCountWeak;

    union
    {
#ifdef HYP_DEBUG_MODE
        bool wasSafeDeleted;
#endif
        uint32 flags;
    };

    ObjectHeader()
        : cls(nullptr),
          index(~0u),
          refCountStrong(0),
          refCountWeak(0),
          flags(0)
    {
    }

    ObjectHeader(const ObjectHeader&) = delete;
    ObjectHeader& operator=(const ObjectHeader&) = delete;
    ObjectHeader(ObjectHeader&&) noexcept = delete;
    ObjectHeader& operator=(ObjectHeader&&) noexcept = delete;
    ~ObjectHeader() = default;

    HYP_FORCE_INLINE bool IsNull() const
    {
        return index == ~0u;
    }

    HYP_FORCE_INLINE uint32 GetRefCountStrong() const
    {
        return AtomicAdd(const_cast<volatile int32*>(&refCountStrong), 0);
    }

    HYP_FORCE_INLINE uint32 GetRefCountWeak() const
    {
        return AtomicAdd(const_cast<volatile int32*>(&refCountWeak), 0);
    }

    bool TryIncRefStrong()
    {
        int32 count = AtomicAdd(&refCountStrong, 0);

        while (count != 0)
        {
            if (AtomicCompareExchange(&refCountStrong, count, count + 1))
            {
#ifdef HYP_DOTNET
                // if count was added successfully (and now, greater than 1), we can acquire the lock for the managed object
                Object_IncScriptObjectRef(GetObjectPointer(this));
#endif

                return true;
            }
        }

        // if count was 0, the object is no longer alive, return false
        return false;
    }

    uint32 IncRefStrong()
    {
        const int32 count = AtomicIncrement(&refCountStrong);

#ifdef HYP_DOTNET
        if (count > 1)
        {
            Object_IncScriptObjectRef(GetObjectPointer(this));
        }
#endif

        return count;
    }

    uint32 IncRefWeak()
    {
        return (uint32)AtomicIncrement(&refCountWeak);
    }

    uint32 DecRefStrong()
    {
        int32 count;

        if ((count = AtomicDecrement(&refCountStrong)) == 0)
        {
            // Increment weak reference count by 1 so any WeakHandleFromThis() calls in the destructor do not immediately cause the item to be removed from the pool
            AtomicIncrement(&refCountWeak);

            // call virtual destructor of ObjectBase
            DestructThisObject(this);

            if (AtomicDecrement(&refCountWeak) == 0)
            {
                // Free the slot for this
                ReleaseObject(this);
            }

            return 0;
        }

        HYP_CORE_ASSERT(count > 0, "RefCount bug! strong count went negative");

#ifdef HYP_DOTNET
        if (count > 1)
        {
            Object_DecScriptObjectRef(GetObjectPointer(this));
        }
#endif

        return (uint32)count;
    }

    uint32 DecRefWeak()
    {
        int32 count;

        if ((count = AtomicDecrement(&refCountWeak)) == 0)
        {
            if (AtomicAdd(&refCountStrong, 0) == 0)
            {
                // Free the slot for this
                ReleaseObject(this);
            }

            return 0;
        }

        HYP_CORE_ASSERT(count > 0, "RefCount bug! weak count went negative");

        return (uint32)count;
    }

    //! Get the pointer to the actual object that this header is for. Header must be non-null
    static HYP_API ObjectBase* GetObjectPointer(ObjectHeader* header);
    static HYP_API void DestructThisObject(ObjectHeader* header);
};

template <class T>
class ObjectContainer final : public ObjectContainerBase
{
public:
    ObjectContainer(const Class* cls)
        : ObjectContainerBase(TypeId::ForType<T>(), cls)
    {
    }

    ObjectContainer(const ObjectContainer& other) = delete;
    ObjectContainer& operator=(const ObjectContainer& other) = delete;

    ObjectContainer(ObjectContainer&& other) noexcept = delete;
    ObjectContainer& operator=(ObjectContainer&& other) noexcept = delete;

    virtual ~ObjectContainer() override
    {
        HYP_CORE_ASSERT(m_headers.Empty(), "Destroying ObjectContainer with live objects!");
    }

    HYP_NODISCARD ObjectHeader* Allocate(SizeType size)
    {
        static constexpr uint32 MaxObjectAlignment = 16;

        static_assert(alignof(T) <= MaxObjectAlignment, "Invalid alignment for object type T, must be <= MaxObjectAlignment");

        AssertDebug(size != 0, "Object size and alignment must be set before allocating objects");
        AssertDebug(size >= sizeof(T));

        // allocation would be the header size + object size, aligned to the object alignment
        const SizeType totalSize = ByteUtil::AlignAs(ByteUtil::AlignAs(sizeof(ObjectHeader), MaxObjectAlignment) + size, MaxObjectAlignment);

        Pool* pool = GetPool();

        TLockGuard<AtomicFlag> guard;
        LockPoolOrThreadAssert(pool, guard, PF_WRITER | PF_ALLOCATE);

        void* mem = pool->Allocate(totalSize, MaxObjectAlignment);

        // header needs to have padding in front of it so we can get the header from the object pointer
        constexpr uint32 HeaderOffset = ByteUtil::AlignAs(sizeof(ObjectHeader), MaxObjectAlignment) - sizeof(ObjectHeader);

        ObjectHeader* header = reinterpret_cast<ObjectHeader*>(reinterpret_cast<UIntPtr>(mem) + HeaderOffset);
        header->index = m_idGenerator.Next() - 1;
        header->cls = m_class;
        header->refCountStrong = 1;
        header->refCountWeak = 0;
        header->flags = 0;

        m_headers.Emplace(header->index, header);

        return header;
    }

    virtual ObjectHeader* GetObjectHeader(uint32 index, TLockGuard<AtomicFlag>& outGuard) override
    {
        if (index == ~0u)
        {
            return nullptr;
        }

        Pool* pool = GetPool();
        LockPoolOrThreadAssert(pool, outGuard, PF_NONE);

        if (!m_headers.HasIndex(index))
        {
            return nullptr;
        }

        return m_headers[index];
    }

    virtual void Release(ObjectHeader* header) override
    {
        HYP_CORE_ASSERT(header != nullptr);

        Pool* pool = GetPool();

        TLockGuard<AtomicFlag> guard;
        LockPoolOrThreadAssert(pool, guard, PF_WRITER | PF_FREE);

        const uint32 index = header->index;
        HYP_CORE_ASSERT(index != ~0u, "Invalid index");

        m_idGenerator.ReleaseId(index + 1);

        constexpr uint32 HeaderOffset = ByteUtil::AlignAs(sizeof(ObjectHeader), 16) - sizeof(ObjectHeader);

        void* mem = reinterpret_cast<void*>(reinterpret_cast<UIntPtr>(header) - HeaderOffset);
        pool->Free(mem);

        m_headers.EraseAt(index);
    }

private:
    SparsePagedArray<ObjectHeader*, 1024> m_headers;
};

class ObjectPool
{
public:
    class ContainerMap
    {
        // Maps TypeId to object container
        // Use a linked list so that references are never invalidated.
        LinkedList<Pair<TypeId, ObjectContainerBase*>> m_map;
        Mutex m_mutex;

    public:
        ContainerMap() = default;
        ContainerMap(const ContainerMap&) = delete;
        ContainerMap& operator=(const ContainerMap&) = delete;
        ContainerMap(ContainerMap&&) noexcept = delete;
        ContainerMap& operator=(ContainerMap&&) noexcept = delete;
        HYP_API ~ContainerMap();

        HYP_API ObjectContainerBase& Get(TypeId typeId);
        HYP_API ObjectContainerBase* TryGet(TypeId typeId);

        HYP_API ObjectContainerBase& GetOrCreate(
            TypeId typeId,
            const Class* cls,
            ObjectContainerBase* (*createFn)(const Class* cls));

        template <class T>
        ObjectContainer<T>& GetOrCreate(const Class* cls)
        {
            // static variable to ensure that the object container is only created once and we don't have to lock everytime this is called
            static ObjectContainer<T>& s_container = static_cast<ObjectContainer<T>&>(GetOrCreate(
                TypeId::ForType<T>(), cls, +[](const Class* cls) -> ObjectContainerBase*
                {
                    return new ObjectContainer<T>(cls);
                }));

            return s_container;
        }
    };

    HYP_API static ContainerMap& GetObjectContainerMap();
};

} // namespace Hyperion
