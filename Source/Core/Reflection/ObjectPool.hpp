/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Defines.hpp>
#include <Core/Constants.hpp>
#include <Core/Types.hpp>

#include <Core/Reflection/ObjId.hpp>
#include <Core/Reflection/ObjectFwd.hpp>
#include <Core/Reflection/ScriptObjectFunctions.hpp>

#include <Core/Containers/SparsePagedArray.hpp>

#include <Core/Utilities/IdGenerator.hpp>

#include <Core/Threading/Mutex.hpp>
#include <Core/Threading/AtomicVar.hpp>
#include <Core/Threading/AtomicFlag.hpp>

#include <Core/Debug/Debug.hpp>

#include <Core/Memory/Memory.hpp>
#include <Core/Memory/Pool/Pool.hpp>

#include <type_traits>

// #define HYP_OBJECT_POOL_DEBUG
namespace Hyperion {

template <class T>
class ObjectContainer;

class ObjectContainerBase;

struct ObjectHeader;
class Class;

CORE_API extern void ReleaseObject(ObjectHeader* header);

class CORE_API ObjectContainerBase
{
    friend class ObjectContainerMap;

public:
    virtual ~ObjectContainerBase();

    HYP_FORCE_INLINE const TypeId& GetObjectTypeId() const
    {
        return m_typeId;
    }

    HYP_FORCE_INLINE const Class* GetClass() const
    {
        return m_class;
    }

    HYP_FORCE_INLINE Pool* GetPool() const
    {
        return m_pool;
    }

    HYP_FORCE_INLINE void SetPool(Pool* pool)
    {
        m_pool = pool;
    }

    virtual void Initialize() = 0;

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
    void LockIfNeeded(TLockGuard<AtomicFlag>& outGuard, int flags);

    TypeId m_typeId;
    const Class* m_class;
    IdGenerator m_idGenerator;
    Pool* m_pool;
    AtomicFlag m_atomicFlag;
    AtomicVar<uint32> m_generation;
    SparsePagedArray<ObjectHeader*, 1024> m_headers;
};

/*! \brief Metadata for a generic object in the object pool. */
struct ObjectHeader
{
    const Class* cls;
    uint32 index;
    volatile uint32 generation;
    volatile int32 refCountStrong;
    volatile int32 refCountWeak;

    ObjectHeader()
        : cls(nullptr),
          index(~0u),
          generation(0),
          refCountStrong(0),
          refCountWeak(0)
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

    HYP_FORCE_INLINE int32 GetRefCountStrong() const
    {
        return AtomicAdd(const_cast<volatile int32*>(&refCountStrong), 0);
    }

    HYP_FORCE_INLINE int32 GetRefCountWeak() const
    {
        return AtomicAdd(const_cast<volatile int32*>(&refCountWeak), 0);
    }

    CORE_API bool TryIncRefStrong();
    CORE_API int32 IncRefStrong();

    int32 IncRefWeak()
    {
        return AtomicIncrement(&refCountWeak);
    }

    CORE_API int32 DecRefStrong();

    CORE_API int32 DecRefWeak();

    //! Get the pointer to the actual object that this header is for. Header must be non-null
    static CORE_API ObjectBase* GetObjectPointer(ObjectHeader* header);
    static CORE_API void DestructThisObject(ObjectHeader* header);
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

    ~ObjectContainer() override = default;

    void Initialize() override
    {
        if (!m_pool)
        {
            m_pool = T::GetAllocator();
        }

        HYP_CORE_ASSERT(m_pool != nullptr);
    }

    HYP_NODISCARD ObjectHeader* AllocateObject(size_t size)
    {
        static constexpr uint32 MaxObjectAlignment = 16;

        static_assert(alignof(T) <= MaxObjectAlignment, "Invalid alignment for object type T, must be <= MaxObjectAlignment");

        HYP_CORE_ASSERT(size >= sizeof(T), "size param value (%zu) must be >= sizeof(T) (%zu)", size, sizeof(T));

        // allocation would be the header size + object size, aligned to the object alignment
        const size_t totalSize = ByteUtil::AlignAs(ByteUtil::AlignAs(sizeof(ObjectHeader), MaxObjectAlignment) + size, MaxObjectAlignment);

        TLockGuard<AtomicFlag> guard;
        LockIfNeeded(guard, PF_WRITER | PF_ALLOCATE);

        void* mem = m_pool->Allocate(totalSize, MaxObjectAlignment);

        // header needs to have padding in front of it so we can get the header from the object pointer
        constexpr uint32 HeaderOffset = ByteUtil::AlignAs(sizeof(ObjectHeader), MaxObjectAlignment) - sizeof(ObjectHeader);

        ObjectHeader* header = reinterpret_cast<ObjectHeader*>(reinterpret_cast<UIntPtr>(mem) + HeaderOffset);
        header->index = m_idGenerator.Next() - 1;
        header->cls = m_class;
        header->generation = m_generation.Increment(1, MemoryOrder::ACQUIRE_RELEASE) + 1;
        header->refCountStrong = 1;
        header->refCountWeak = 0;

        m_headers.Emplace(header->index, header);

        return header;
    }

    virtual ObjectHeader* GetObjectHeader(uint32 index, TLockGuard<AtomicFlag>& outGuard) override
    {
        if (index == ~0u)
        {
            return nullptr;
        }

        LockIfNeeded(outGuard, PF_NONE);

        if (!m_headers.HasIndex(index))
        {
            return nullptr;
        }

        return m_headers[index];
    }

    virtual void Release(ObjectHeader* header) override
    {
        HYP_CORE_ASSERT(header != nullptr);

        TLockGuard<AtomicFlag> guard;
        LockIfNeeded(guard, PF_WRITER | PF_FREE);

        const uint32 index = header->index;
        HYP_CORE_ASSERT(index != ~0u, "Invalid index");

        m_idGenerator.ReleaseId(index + 1);

        constexpr uint32 HeaderOffset = ByteUtil::AlignAs(sizeof(ObjectHeader), 16) - sizeof(ObjectHeader);

        void* mem = reinterpret_cast<void*>(reinterpret_cast<UIntPtr>(header) - HeaderOffset);
        m_pool->Free(mem);

        m_headers.EraseAt(index);
    }

    // To match allocator interface
    HYP_NODISCARD void* Allocate(size_t size)
    {
        return reinterpret_cast<void*>(reinterpret_cast<UIntPtr>(AllocateObject(size)) + sizeof(ObjectHeader));
    }

    // To match allocator interface
    HYP_NODISCARD void* Allocate(size_t size, size_t alignment)
    {
        HYP_CORE_ASSERT(alignment <= 16, "ObjectContainer does not support alignments greater than 16!");

        return Allocate(size);
    }

    // To match allocator interface
    void Free(void* ptr)
    {
        if (!ptr)
        {
            return;
        }

        ObjectHeader* header = reinterpret_cast<ObjectHeader*>(reinterpret_cast<UIntPtr>(ptr) - sizeof(ObjectHeader));

        // expected to be called from operator delete for the allocation ref set in Allocate()
        // Delegate to DecRefStrong() which handles virtual destructor call, weak ref protection,
        // and ReleaseObject() when both strong and weak counts reach zero.
        const int32 refCount = header->DecRefStrong();
        HYP_CORE_ASSERT(refCount == 0);
    }
};

class CORE_API ObjectContainerMap
{
    // Maps TypeId to object container
    // Use a linked list so that references are never invalidated.
    TList<Pair<TypeId, ObjectContainerBase*>> m_map;
    Mutex m_mutex;

public:
    ObjectContainerMap() = default;

    ObjectContainerMap(const ObjectContainerMap&) = delete;
    ObjectContainerMap& operator=(const ObjectContainerMap&) = delete;

    ObjectContainerMap(ObjectContainerMap&&) noexcept = delete;
    ObjectContainerMap& operator=(ObjectContainerMap&&) noexcept = delete;

    ~ObjectContainerMap();

    void Shutdown();

    ObjectContainerBase& Get(TypeId typeId);
    ObjectContainerBase* TryGet(TypeId typeId);

    ObjectContainerBase& GetOrCreate(
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

CORE_API ObjectContainerMap& GetObjectContainerMap();

template <class T>
static inline ObjectContainer<T>& GetObjectContainer()
{
    static ObjectContainer<T>& s_container = GetObjectContainerMap().GetOrCreate<T>(GetClass<T>());
    return s_container;
}

} // namespace Hyperion
