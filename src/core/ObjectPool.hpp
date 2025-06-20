/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_OBJECT_POOL_HPP
#define HYPERION_CORE_OBJECT_POOL_HPP

#include <core/ID.hpp>
#include <core/Util.hpp>
#include <core/Defines.hpp>

#include <core/object/HypObjectFwd.hpp>

#include <core/threading/Mutex.hpp>
#include <core/threading/DataRaceDetector.hpp>

#include <core/debug/Debug.hpp>

#include <core/memory/UniquePtr.hpp>
#include <core/memory/Memory.hpp>
#include <core/memory/MemoryPool.hpp>

#include <Constants.hpp>
#include <Types.hpp>

#include <type_traits>

// #define HYP_OBJECT_POOL_DEBUG
namespace hyperion {

class Engine;

template <class T>
struct HandleDefinition;

template <class T>
class ObjectContainer;

class ObjectContainerBase;

struct HypObjectHeader;
class HypClass;

class ObjectContainerBase
{
public:
    virtual ~ObjectContainerBase() = default;

    HYP_FORCE_INLINE const TypeID& GetObjectTypeID() const
    {
        return m_type_id;
    }

    HYP_FORCE_INLINE const HypClass* GetHypClass() const
    {
        return m_hyp_class;
    }

    virtual SizeType NumAllocatedElements() const = 0;
    virtual SizeType NumAllocatedBytes() const = 0;

    virtual void IncRefStrong(HypObjectHeader*) = 0;
    virtual void IncRefWeak(HypObjectHeader*) = 0;
    virtual void DecRefStrong(HypObjectHeader*) = 0;
    virtual void DecRefWeak(HypObjectHeader*) = 0;
    virtual void* Release(HypObjectHeader*) = 0;

    virtual HypObjectBase* GetObjectPointer(HypObjectHeader*) = 0;
    virtual HypObjectHeader* GetObjectHeader(uint32 index) = 0;

    virtual void ReleaseIndex(uint32 index) = 0;

protected:
    ObjectContainerBase(TypeID type_id, const HypClass* hyp_class)
        : m_type_id(type_id),
          m_hyp_class(hyp_class)
    {
    }

    TypeID m_type_id;
    const HypClass* m_hyp_class;
};

/*! \brief Metadata for a generic object in the object pool. */
struct HypObjectHeader
{
    ObjectContainerBase* container;
    uint32 index;
    AtomicVar<uint32> ref_count_strong;
    AtomicVar<uint32> ref_count_weak;

    HypObjectHeader()
        : container(nullptr),
          index(~0u),
          ref_count_strong(0),
          ref_count_weak(0)
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
        return ref_count_strong.Get(MemoryOrder::ACQUIRE);
    }

    HYP_FORCE_INLINE uint32 GetRefCountWeak() const
    {
        return ref_count_weak.Get(MemoryOrder::ACQUIRE);
    }

    HYP_FORCE_INLINE void IncRefStrong()
    {
        container->IncRefStrong(this);
    }

    HYP_FORCE_INLINE void IncRefWeak()
    {
        container->IncRefWeak(this);
    }

    HYP_FORCE_INLINE void DecRefStrong()
    {
        container->DecRefStrong(this);
    }

    HYP_FORCE_INLINE void DecRefWeak()
    {
        container->DecRefWeak(this);
    }

    HYP_FORCE_INLINE void* Release()
    {
        return container->Release(this);
    }
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

    uint32 IncRefStrong()
    {
        const uint32 count = ref_count_strong.Increment(1, MemoryOrder::ACQUIRE_RELEASE) + 1;

        HypObject_OnIncRefCount_Strong(HypObjectPtr(GetPointer()), count);

        return count;
    }

    uint32 IncRefWeak()
    {
        return ref_count_weak.Increment(1, MemoryOrder::ACQUIRE_RELEASE) + 1;
    }

    uint32 DecRefStrong()
    {
        uint32 count;

        if ((count = ref_count_strong.Decrement(1, MemoryOrder::ACQUIRE_RELEASE)) == 1)
        {
            // Increment weak reference count by 1 so any WeakHandleFromThis() calls in the destructor do not immediately cause FreeID() to be called.
            ref_count_weak.Increment(1, MemoryOrder::RELEASE);

            HypObject_OnDecRefCount_Strong(HypObjectPtr(GetPointer()), count - 1);

            GetPointer()->~T();

            if (ref_count_weak.Decrement(1, MemoryOrder::ACQUIRE_RELEASE) == 1)
            {
                // Free the slot for this
                container->ReleaseIndex(index);
            }
        }
        else
        {
            HypObject_OnDecRefCount_Strong(HypObjectPtr(GetPointer()), count - 1);
        }

        return count - 1;
    }

    uint32 DecRefWeak()
    {
        uint32 count;

        if ((count = ref_count_weak.Decrement(1, MemoryOrder::ACQUIRE_RELEASE)) == 1)
        {
            if (ref_count_strong.Get(MemoryOrder::ACQUIRE) == 0)
            {
                // Free the slot for this
                container->ReleaseIndex(index);
            }
        }

        AssertDebug(count != 0);

        return count - 1;
    }

    HYP_NODISCARD T* Release()
    {
        T* ptr = storage.GetPointer();

        return ptr;
    }

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
    ObjectContainerBase* container = static_cast<ObjectContainer<T>*>(ctx);
    AssertThrow(container != nullptr);

    for (uint32 index = 0; index < count; index++)
    {
        elements[index].container = container;
        elements[index].index = offset + index;
    }
}

template <class T>
class ObjectContainer final : public ObjectContainerBase
{
    using MemoryPoolType = MemoryPool<HypObjectMemory<T>, MemoryPoolInitInfo<T>, ObjectContainer_OnBlockAllocated<T>>;

    using HypObjectMemory = HypObjectMemory<T>;

public:
    ObjectContainer()
        : ObjectContainerBase(TypeID::ForType<T>(), GetClass(TypeID::ForType<T>())),
          m_pool(2048, /* create_initial_blocks */ true, /* block_init_ctx */ this)
    {
    }

    ObjectContainer(const ObjectContainer& other) = delete;
    ObjectContainer& operator=(const ObjectContainer& other) = delete;
    ObjectContainer(ObjectContainer&& other) noexcept = delete;
    ObjectContainer& operator=(ObjectContainer&& other) noexcept = delete;
    virtual ~ObjectContainer() override = default;

    virtual SizeType NumAllocatedElements() const override
    {
        return m_pool.NumAllocatedElements();
    }

    virtual SizeType NumAllocatedBytes() const override
    {
        return m_pool.NumAllocatedBytes();
    }

    HYP_NODISCARD HYP_FORCE_INLINE HypObjectMemory* Allocate()
    {
        HypObjectMemory* element;
        m_pool.AcquireIndex(&element);

        return element;
    }

    virtual void IncRefStrong(HypObjectHeader* ptr) override
    {
        static_cast<HypObjectMemory*>(ptr)->HypObjectMemory::IncRefStrong();
    }

    virtual void IncRefWeak(HypObjectHeader* ptr) override
    {
        static_cast<HypObjectMemory*>(ptr)->HypObjectMemory::IncRefWeak();
    }

    virtual void DecRefStrong(HypObjectHeader* ptr) override
    {
        // Have to call the derived implementation or it will recursive infinitely
        static_cast<HypObjectMemory*>(ptr)->HypObjectMemory::DecRefStrong();
    }

    virtual void DecRefWeak(HypObjectHeader* ptr) override
    {
        // Have to call the derived implementation or it will recursive infinitely
        static_cast<HypObjectMemory*>(ptr)->HypObjectMemory::DecRefWeak();
    }

    virtual void* Release(HypObjectHeader* ptr) override
    {
        return static_cast<HypObjectMemory*>(ptr)->HypObjectMemory::Release();
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
        return &m_pool.GetElement(index);
    }

    virtual void ReleaseIndex(uint32 index) override
    {
        m_pool.ReleaseIndex(index);
    }

    // private:
    MemoryPoolType m_pool;
};

class ObjectPool
{
public:
    class ObjectContainerMap
    {
        // Maps type ID to object container
        // Use a linked list so that references are never invalidated.
        LinkedList<Pair<TypeID, UniquePtr<ObjectContainerBase>>> m_map;
        Mutex m_mutex;

    public:
        ObjectContainerMap() = default;
        ObjectContainerMap(const ObjectContainerMap&) = delete;
        ObjectContainerMap& operator=(const ObjectContainerMap&) = delete;
        ObjectContainerMap(ObjectContainerMap&&) noexcept = delete;
        ObjectContainerMap& operator=(ObjectContainerMap&&) noexcept = delete;
        ~ObjectContainerMap() = default;

        template <class T>
        ObjectContainer<T>& GetOrCreate()
        {
            // static variable to ensure that the object container is only created once and we don't have to lock everytime this is called
            static ObjectContainer<T>& container = static_cast<ObjectContainer<T>&>(GetOrCreate(TypeID::ForType<T>(), []() -> UniquePtr<ObjectContainerBase>
                {
                    return MakeUnique<ObjectContainer<T>>();
                }));

            return container;
        }

        HYP_API ObjectContainerBase& Get(TypeID type_id);
        HYP_API ObjectContainerBase* TryGet(TypeID type_id);

    private:
        ObjectContainerBase& GetOrCreate(TypeID type_id, UniquePtr<ObjectContainerBase> (*create_fn)(void))
        {
            Mutex::Guard guard(m_mutex);

            auto it = m_map.FindIf([type_id](const auto& element)
                {
                    return element.first == type_id;
                });

            if (it != m_map.End())
            {
                if (it->second == nullptr)
                {
                    it->second = create_fn();
                }

                return *it->second;
            }

            UniquePtr<ObjectContainerBase> container = create_fn();
            AssertThrow(container != nullptr);

            return *m_map.EmplaceBack(type_id, std::move(container)).second;
        }
    };

    HYP_API static ObjectContainerMap& GetObjectContainerHolder();
};

} // namespace hyperion

#endif