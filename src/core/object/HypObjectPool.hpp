/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/object/ObjId.hpp>
#include <core/Util.hpp>
#include <core/Defines.hpp>

#include <core/object/HypObjectFwd.hpp>

#include <core/utilities/IdGenerator.hpp>

#include <core/threading/Mutex.hpp>
#include <core/threading/DataRaceDetector.hpp>
#include <core/threading/AtomicVar.hpp>
#include <core/threading/Spinlock.hpp>

#include <core/debug/Debug.hpp>

#include <core/memory/Memory.hpp>
#include <core/memory/pool/Pool.hpp>

#include <core/Constants.hpp>
#include <core/Types.hpp>

#include <type_traits>

// #define HYP_OBJECT_POOL_DEBUG
namespace hyperion {

template <class T>
class HypObjectContainer;

class HypObjectContainerBase;

struct HypObjectHeader;
class HypClass;

HYP_API extern void ReleaseHypObject(HypObjectHeader* header);

class HypObjectContainerBase
{
    friend class HypObjectPool;

public:
    virtual ~HypObjectContainerBase() = default;

    HYP_FORCE_INLINE const TypeId& GetObjectTypeId() const
    {
        return m_typeId;
    }

    HYP_FORCE_INLINE const HypClass* GetHypClass() const
    {
        return m_hypClass;
    }

    virtual HypObjectBase* GetObjectPointer(HypObjectHeader*) = 0;
    virtual HypObjectHeader* GetObjectHeader(uint32 index) = 0;

    virtual void Release(HypObjectHeader* header) = 0;

protected:
    enum PoolFlags : uint8
    {
        PF_NONE = 0x0,
        PF_WRITER = 0x1,
        PF_ALLOCATE = 0x2,
        PF_FREE = 0x4
    };

    struct GlobalPoolLockGuard
    {
        Spinlock* lock = nullptr;
        int flags = PF_NONE;

        HYP_FORCE_INLINE ~GlobalPoolLockGuard()
        {
            if (lock)
            {
                if (flags & PF_WRITER)
                {
                    // make sure lock is released on destruction
                    lock->UnlockWriter();
                }
                else
                {
                    lock->UnlockReader();
                }
            }
        }
    };

    HypObjectContainerBase(TypeId typeId, const HypClass* hypClass);

    /*! \brief Checks that the current thread is the pool's owning thread, or locks the global pool lock if this is the global pool.
     *  \param outGuard If this is the global pool, the lock state will be stored here so it can be released later.
     */
    HYP_API void LockPoolOrThreadAssert(GlobalPoolLockGuard& outGuard, int flags) const;

    TypeId m_typeId;
    const HypClass* m_hypClass;
    IdGenerator m_idGenerator;
    Pool* m_pool;
};

/*! \brief Metadata for a generic object in the object pool. */
struct HypObjectHeader
{
    const HypClass* hypClass;
    uint32 index;
    volatile int32 refCountStrong;
    volatile int32 refCountWeak;

    HypObjectHeader()
        : hypClass(nullptr),
          index(~0u),
          refCountStrong(0),
          refCountWeak(0)
    {
    }

    HypObjectHeader(const HypObjectHeader&) = delete;
    HypObjectHeader& operator=(const HypObjectHeader&) = delete;
    HypObjectHeader(HypObjectHeader&&) noexcept = delete;
    HypObjectHeader& operator=(HypObjectHeader&&) noexcept = delete;
    ~HypObjectHeader() = default;

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
        return AtomicAdd(const_cast<volatile int32*>(&refCountStrong), 0);
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
                HypObject_IncScriptObjectRef(GetObjectPointer(this));
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
            HypObject_IncScriptObjectRef(GetObjectPointer(this));
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

            // call virtual destructor of HypObjectBase
            DestructThisObject(this);

            if (AtomicDecrement(&refCountWeak) == 0)
            {
                // Free the slot for this
                ReleaseHypObject(this);
            }

            return 0;
        }

        HYP_CORE_ASSERT(count > 0, "RefCount bug! strong count went negative");

#ifdef HYP_DOTNET
        if (count > 1)
        {
            HypObject_DecScriptObjectRef(GetObjectPointer(this));
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
                ReleaseHypObject(this);
            }

            return 0;
        }

        HYP_CORE_ASSERT(count > 0, "RefCount bug! weak count went negative");

        return (uint32)count;
    }

    //! Get the pointer to the actual object that this header is for. Header must be non-null
    static HYP_API HypObjectBase* GetObjectPointer(HypObjectHeader* header);
    static HYP_API void DestructThisObject(HypObjectHeader* header);
};

/*! \brief Memory storage for T where T is a subclass of HypObjectBase.
 *  Derives from HypObjectHeader to store reference counts and other information at the start of the memory. */
template <class T>
struct HypObjectMemory final : HypObjectHeader
{
    static_assert(std::is_base_of_v<HypObjectBase, T>, "T must be a subclass of HypObjectBase");

    ValueStorage<T> storage;

    HypObjectMemory() = default;
    HypObjectMemory(const HypObjectMemory&) = delete;
    HypObjectMemory& operator=(const HypObjectMemory&) = delete;
    HypObjectMemory(HypObjectMemory&&) noexcept = delete;
    HypObjectMemory& operator=(HypObjectMemory&&) noexcept = delete;
    ~HypObjectMemory() = default;

    HYP_FORCE_INLINE T& Get()
    {
        return storage.Get();
    }

    HYP_FORCE_INLINE T* GetPointer()
    {
        return storage.GetPointer();
    }

    HYP_FORCE_INLINE const T* GetPointer() const
    {
        return storage.GetPointer();
    }
};

template <class T>
static inline void ObjectContainer_OnBlockAllocated(void* ctx, HypObjectMemory<T>* elements, uint32 offset, uint32 count)
{
    static const HypClass* s_class = T::Class();

    for (uint32 index = 0; index < count; index++)
    {
        elements[index].hypClass = s_class;
        elements[index].index = offset + index;
    }
}

template <class T>
class HypObjectContainer final : public HypObjectContainerBase
{
    using HypObjectMemory = HypObjectMemory<T>;

public:
    HypObjectContainer(const HypClass* hypClass)
        : HypObjectContainerBase(TypeId::ForType<T>(), hypClass)
    {
    }

    HypObjectContainer(const HypObjectContainer& other) = delete;
    HypObjectContainer& operator=(const HypObjectContainer& other) = delete;
    HypObjectContainer(HypObjectContainer&& other) noexcept = delete;
    HypObjectContainer& operator=(HypObjectContainer&& other) noexcept = delete;

    virtual ~HypObjectContainer() override
    {
        Array<HypObjectHeader*> headers;

        {
            GlobalPoolLockGuard guard;
            LockPoolOrThreadAssert(guard, PF_NONE);

            for (auto& header : m_headers)
            {
                headers.PushBack(header);
            }
        }

        // destruct all allocated elements
        for (HypObjectHeader* header : headers)
        {
            HYP_CORE_ASSERT(header != nullptr);

            HypObjectHeader::DestructThisObject(header);
        }

        GlobalPoolLockGuard guard;
        LockPoolOrThreadAssert(guard, PF_WRITER | PF_FREE);

        /// @FIXME: This is not safe if the destructor of T tries to allocate more objects from the pool during
        // its destruction

        // Free all allocated elements
        for (HypObjectHeader* header : headers)
        {
            m_pool->Free(header);
        }
    }

    HYP_NODISCARD HypObjectHeader* Allocate(SizeType size)
    {
        static constexpr uint32 MaxObjectAlignment = 16;

        static_assert(alignof(T) <= MaxObjectAlignment, "Invalid alignment for object type T, must be <= MaxObjectAlignment");

        AssertDebug(size != 0, "Object size and alignment must be set before allocating objects");
        AssertDebug(size >= sizeof(T));

        // allocation would be the header size + object size, aligned to the object alignment
        const SizeType totalSize = ByteUtil::AlignAs(ByteUtil::AlignAs(sizeof(HypObjectHeader), MaxObjectAlignment) + size, MaxObjectAlignment);

        GlobalPoolLockGuard guard;
        LockPoolOrThreadAssert(guard, PF_WRITER | PF_ALLOCATE);

        void* mem = m_pool->Alloc(totalSize, MaxObjectAlignment);

        // header needs to have padding in front of it so we can get the header from the object pointer
        constexpr uint32 HeaderOffset = ByteUtil::AlignAs(sizeof(HypObjectHeader), MaxObjectAlignment) - sizeof(HypObjectHeader);

        HypObjectHeader* header = reinterpret_cast<HypObjectHeader*>(reinterpret_cast<UIntPtr>(mem) + HeaderOffset);
        header->index = m_idGenerator.Next() - 1;
        header->hypClass = m_hypClass;
        header->refCountStrong = 0;
        header->refCountWeak = 0;

        m_headers.Emplace(header->index, header);

        return header;
    }

    virtual HypObjectBase* GetObjectPointer(HypObjectHeader* ptr) override
    {
        if (!ptr)
        {
            return nullptr;
        }

        return static_cast<HypObjectMemory*>(ptr)->GetPointer();
    }

    virtual HypObjectHeader* GetObjectHeader(uint32 index) override
    {
        if (index == ~0u)
        {
            return nullptr;
        }

        GlobalPoolLockGuard guard;
        LockPoolOrThreadAssert(guard, PF_NONE);

        if (!m_headers.HasIndex(index))
        {
            return nullptr;
        }

        return m_headers[index];
    }

    virtual void Release(HypObjectHeader* header) override
    {
        HYP_CORE_ASSERT(header != nullptr);

        GlobalPoolLockGuard guard;
        LockPoolOrThreadAssert(guard, PF_WRITER | PF_FREE);

        const uint32 index = header->index;
        HYP_CORE_ASSERT(index != ~0u, "Invalid index");

        m_idGenerator.ReleaseId(index + 1);
        m_pool->Free(header);

        m_headers.EraseAt(index);
    }

private:
    SparsePagedArray<HypObjectHeader*, 1024> m_headers;
};

class HypObjectPool
{
public:
    class ContainerMap
    {
        // Maps type Id to object container
        // Use a linked list so that references are never invalidated.
        LinkedList<Pair<TypeId, HypObjectContainerBase*>> m_map;
        Mutex m_mutex;

    public:
        ContainerMap() = default;
        ContainerMap(const ContainerMap&) = delete;
        ContainerMap& operator=(const ContainerMap&) = delete;
        ContainerMap(ContainerMap&&) noexcept = delete;
        ContainerMap& operator=(ContainerMap&&) noexcept = delete;
        HYP_API ~ContainerMap();

        HYP_API HypObjectContainerBase& Get(TypeId typeId);
        HYP_API HypObjectContainerBase* TryGet(TypeId typeId);

        HYP_API HypObjectContainerBase& GetOrCreate(
            TypeId typeId,
            const HypClass* hypClass,
            HypObjectContainerBase* (*createFn)(const HypClass* hypClass));

        template <class T>
        HypObjectContainer<T>& GetOrCreate(const HypClass* hypClass)
        {
            // static variable to ensure that the object container is only created once and we don't have to lock everytime this is called
            static HypObjectContainer<T>& s_container = static_cast<HypObjectContainer<T>&>(GetOrCreate(TypeId::ForType<T>(), hypClass, +[](const HypClass* hypClass) -> HypObjectContainerBase*
                {
                    return new HypObjectContainer<T>(hypClass);
                }));

            return s_container;
        }
    };

    HYP_API static ContainerMap& GetObjectContainerMap();
};

} // namespace hyperion
