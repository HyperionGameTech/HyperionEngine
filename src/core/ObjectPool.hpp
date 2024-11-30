/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_OBJECT_POOL_HPP
#define HYPERION_CORE_OBJECT_POOL_HPP

#include <core/IDGenerator.hpp>
#include <core/ID.hpp>
#include <core/Util.hpp>
#include <core/Defines.hpp>

#include <core/object/HypObjectFwd.hpp>

#include <core/threading/Mutex.hpp>
#include <core/threading/DataRaceDetector.hpp>

#include <core/system/Debug.hpp>

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

class IObjectContainer;

struct HypObjectHeader;

class IObjectContainer
{
public:
    virtual ~IObjectContainer() = default;

    virtual uint32 NextIndex() = 0;
    
    virtual void IncRefStrong(HypObjectHeader *) = 0;
    virtual void IncRefWeak(HypObjectHeader *) = 0;
    virtual void DecRefStrong(HypObjectHeader *) = 0;
    virtual void DecRefWeak(HypObjectHeader *) = 0;
    virtual uint32 GetRefCountStrong(HypObjectHeader *) = 0;
    virtual uint32 GetRefCountWeak(HypObjectHeader *) = 0;

    virtual AnyRef GetObject(HypObjectHeader *) = 0;
    virtual HypObjectHeader *GetObjectHeader(uint32 index) = 0;
    virtual HypObjectHeader *GetDefaultObjectHeader() = 0;
    virtual uint32 GetObjectIndex(const HypObjectHeader *ptr) = 0;
    virtual TypeID GetObjectTypeID() const = 0;

    virtual const IHypObjectInitializer *GetObjectInitializer(uint32 index) = 0;

    virtual void *ConstructAtIndex(uint32 index) = 0;

    virtual IDGenerator &GetIDGenerator() = 0;
};

/*! \brief Metadata for a generic object in the object pool. */
struct HypObjectHeader
{
    IObjectContainer    *container;
    uint32              index;
    AtomicVar<uint16>   ref_count_strong;
    AtomicVar<uint16>   ref_count_weak;

#ifdef HYP_DEBUG_MODE
    AtomicVar<bool>     has_value;
#endif

    HypObjectHeader()
        : container(nullptr),
          index(~0u),
          ref_count_strong(0),
          ref_count_weak(0)
    {
#ifdef HYP_DEBUG_MODE
        has_value.Set(false, MemoryOrder::SEQUENTIAL);
#endif
    }

    HYP_FORCE_INLINE bool IsNull() const
        { return index == ~0u; }

    HYP_FORCE_INLINE uint32 GetRefCountStrong() const
        { return uint32(ref_count_strong.Get(MemoryOrder::ACQUIRE)); }

    HYP_FORCE_INLINE uint32 GetRefCountWeak() const
        { return uint32(ref_count_weak.Get(MemoryOrder::ACQUIRE)); }

    HYP_FORCE_INLINE void IncRefStrong()
        { ref_count_strong.Increment(1, MemoryOrder::RELAXED); }

    HYP_FORCE_INLINE void IncRefWeak()
        { ref_count_weak.Increment(1, MemoryOrder::RELAXED); }

    HYP_FORCE_INLINE void DecRefStrong()
        { container->DecRefStrong(this); }

    HYP_FORCE_INLINE void DecRefWeak()
        { container->DecRefWeak(this); }
};

/*! \brief Memory storage for T where T is a subclass of IHypObject.
 *  Derives from HypObjectHeader to store reference counts and other information at the start of the memory. */
template <class T>
struct HypObjectMemory final : HypObjectHeader
{
    static_assert(std::is_base_of_v<IHypObject, T>, "T must be a subclass of IHypObject");

    alignas(T) ubyte    bytes[sizeof(T)];

    HypObjectMemory()                                       = default;
    HypObjectMemory(const HypObjectMemory &)                = delete;
    HypObjectMemory &operator=(const HypObjectMemory &)     = delete;
    HypObjectMemory(HypObjectMemory &&) noexcept            = delete;
    HypObjectMemory &operator=(HypObjectMemory &&) noexcept = delete;
    ~HypObjectMemory()                                      = default;

    template <class... Args>
    T *Construct(Args &&... args)
    {
#ifdef HYP_DEBUG_MODE
        AssertThrow(!has_value.Exchange(true, MemoryOrder::SEQUENTIAL));
#endif

        T *ptr = reinterpret_cast<T *>(&bytes[0]);

        // Note: don't use Memory::ConstructWithContext as we need to set the header pointer before HypObjectInitializerGuard destructs
        HypObjectInitializerGuard<T> context { ptr };

        // Set the object header to point to this
        ptr->IHypObject::m_header = static_cast<HypObjectHeader *>(this);

        Memory::Construct<T>(static_cast<void *>(ptr), std::forward<Args>(args)...);

        return ptr;
    }

    uint32 DecRefStrong()
    {
        uint16 count;

        if ((count = ref_count_strong.Decrement(1, MemoryOrder::ACQUIRE_RELEASE)) == 1) {
#ifdef HYP_DEBUG_MODE
            AssertThrow(has_value.Exchange(false, MemoryOrder::SEQUENTIAL));

            AssertThrow(container != nullptr);
            AssertThrow(index != ~0u);
#endif

            reinterpret_cast<T *>(bytes)->~T();

            if (ref_count_weak.Get(MemoryOrder::ACQUIRE) == 0) {
                // Free the slot for this
                container->GetIDGenerator().FreeID(index + 1);
            }
        }

        return uint32(count) - 1;
    }

    uint32 DecRefWeak()
    {
        uint16 count;

        if ((count = ref_count_weak.Decrement(1, MemoryOrder::ACQUIRE_RELEASE)) == 1) {
#ifdef HYP_DEBUG_MODE
            AssertThrow(container != nullptr);
            AssertThrow(index != ~0u);
#endif

            if (ref_count_strong.Get(MemoryOrder::ACQUIRE) == 0) {
                // Free the slot for this
                container->GetIDGenerator().FreeID(index + 1);
            }
        }

        return uint32(count) - 1;
    }

    HYP_FORCE_INLINE T &Get()
        { return *reinterpret_cast<T *>(bytes); }

    HYP_FORCE_INLINE T *GetPointer()
        { return reinterpret_cast<T *>(bytes); }

    HYP_FORCE_INLINE const T *GetPointer() const
        { return reinterpret_cast<const T *>(bytes); }
};

template <class T>
static inline void ObjectContainer_OnBlockAllocated(void *ctx, HypObjectMemory<T> *elements, uint32 offset, uint32 count)
{
    IObjectContainer *container = static_cast<ObjectContainer<T> *>(ctx);
    AssertThrow(container != nullptr);

    for (uint32 index = 0; index < count; index++) {
        elements[index].container = container;
        elements[index].index = offset + index;
    }
}

template <class T>
class ObjectContainer final : public IObjectContainer
{
    using MemoryPoolType = MemoryPool<HypObjectMemory<T>, 2048, ObjectContainer_OnBlockAllocated<T>>;

    using HypObjectMemory = HypObjectMemory<T>;

public:
    ObjectContainer()
        : m_pool(2048, /* create_initial_blocks */ true, /* block_init_ctx */ this)
    {
        // Initialize the default / null object
        m_default_object.container = this;
        m_default_object.index = ~0u;
    }

    ObjectContainer(const ObjectContainer &other)                   = delete;
    ObjectContainer &operator=(const ObjectContainer &other)        = delete;
    ObjectContainer(ObjectContainer &&other) noexcept               = delete;
    ObjectContainer &operator=(ObjectContainer &&other) noexcept    = delete;
    virtual ~ObjectContainer() override                             = default;

    virtual uint32 NextIndex() override
    {
        return m_pool.AcquireIndex();
    }

    HYP_NODISCARD HYP_FORCE_INLINE HypObjectMemory *Allocate()
    {
        HypObjectMemory *element;
        uint32 index = m_pool.AcquireIndex(&element);

#ifdef HYP_DEBUG_MODE
        AssertThrow(uintptr_t(element->container) == uintptr_t(this));
        AssertThrow(element->index == index);
#endif

        return element;
    }

    virtual void IncRefStrong(HypObjectHeader *ptr) override
    {
        static_cast<HypObjectMemory *>(ptr)->IncRefStrong();
    }

    virtual void IncRefWeak(HypObjectHeader *ptr) override
    {
        static_cast<HypObjectMemory *>(ptr)->IncRefWeak();
    }

    virtual void DecRefStrong(HypObjectHeader *ptr) override
    {
        // Have to call the derived implementation or it will recursive infinitely
        static_cast<HypObjectMemory *>(ptr)->HypObjectMemory::DecRefStrong();
    }

    virtual void DecRefWeak(HypObjectHeader *ptr) override
    {
        // Have to call the derived implementation or it will recursive infinitely
        static_cast<HypObjectMemory *>(ptr)->HypObjectMemory::DecRefWeak();
    }

    virtual uint32 GetRefCountStrong(HypObjectHeader *ptr) override
    {
        return static_cast<HypObjectMemory *>(ptr)->GetRefCountStrong();
    }

    virtual uint32 GetRefCountWeak(HypObjectHeader *ptr) override
    {
        return static_cast<HypObjectMemory *>(ptr)->GetRefCountWeak();
    }

    virtual AnyRef GetObject(HypObjectHeader *ptr) override
    {
        if (!ptr) {
            return AnyRef(TypeID::ForType<T>(), nullptr);
        }

        return AnyRef(TypeID::ForType<T>(), static_cast<HypObjectMemory *>(ptr)->GetPointer());
    }

    virtual HypObjectHeader *GetObjectHeader(uint32 index) override
    {
        return &m_pool.GetElement(index);
    }

    virtual HypObjectHeader *GetDefaultObjectHeader() override
    {
        return &m_default_object;
    }

    virtual uint32 GetObjectIndex(const HypObjectHeader *ptr) override
    {
        if (!ptr) {
            return ~0u;
        }

        return static_cast<const HypObjectMemory *>(ptr)->index;
    }

    virtual TypeID GetObjectTypeID() const override
    {
        static const TypeID type_id = TypeID::ForType<T>();

        return type_id;
    }

    virtual const IHypObjectInitializer *GetObjectInitializer(uint32 index) override
    {
        // check is temporary for now
        if constexpr (IsHypObject<T>::value) {
            return m_pool.GetElement(index).Get().GetObjectInitializer();
        }

        return nullptr;
    }
    
    virtual void *ConstructAtIndex(uint32 index) override
    {
        HypObjectMemory &element = m_pool.GetElement(index);
        element.Construct();

        return &element.Get();
    }
    
    template <class Arg0, class... Args>
    HYP_FORCE_INLINE void *ConstructAtIndex(uint32 index, Arg0 &&arg0, Args &&... args)
    {
        HypObjectMemory &element = m_pool.GetElement(index);
        element.Construct(std::forward<Arg0>(arg0), std::forward<Args>(args)...);

        return &element.Get();
    }

    virtual IDGenerator &GetIDGenerator() override
    {
        return m_pool.GetIDGenerator();
    }

private:
    MemoryPoolType  m_pool;
    HypObjectMemory m_default_object;
};

class ObjectPool
{
public:
    struct ObjectContainerMap
    {
        // Maps type ID to object container
        // Use a linked list so that iterators are never invalidated.
        LinkedList<Pair<TypeID, UniquePtr<IObjectContainer>>>   map;

#ifdef HYP_ENABLE_MT_CHECK
        DataRaceDetector                                        data_race_detector;
#endif

        ObjectContainerMap()                                            = default;
        ObjectContainerMap(const ObjectContainerMap &)                  = delete;
        ObjectContainerMap &operator=(const ObjectContainerMap &)       = delete;
        ObjectContainerMap(ObjectContainerMap &&) noexcept              = delete;
        ObjectContainerMap &operator=(ObjectContainerMap &&) noexcept   = delete;
        ~ObjectContainerMap()                                           = default;
    };

    struct ObjectContainerHolder
    {
        template <class T>
        struct ObjectContainerDeclaration
        {
            ObjectContainerDeclaration(UniquePtr<IObjectContainer> *allotted_container)
            {
                AssertThrowMsg(allotted_container != nullptr, "Allotted container pointer was null!");

                if (*allotted_container != nullptr) {
                    // Already allocated
                    return;
                }

                (*allotted_container) = MakeUnique<ObjectContainer<T>>();
            }

            ObjectContainerDeclaration(const ObjectContainerDeclaration &)                  = delete;
            ObjectContainerDeclaration &operator=(const ObjectContainerDeclaration &)       = delete;
            ObjectContainerDeclaration(ObjectContainerDeclaration &&) noexcept              = delete;
            ObjectContainerDeclaration &operator=(ObjectContainerDeclaration &&) noexcept   = delete;
            ~ObjectContainerDeclaration()                                                   = default;
        };

        ObjectContainerMap object_container_map;
        
        template <class T>
        ObjectContainer<T> &GetObjectContainer(UniquePtr<IObjectContainer> *allotted_container)
        {
            static_assert(has_opaque_handle_defined<T>, "Object type not viable for GetContainer<T> : Does not support handles");

            static ObjectContainerDeclaration<T> object_container_declaration(allotted_container);
            // static ObjectContainer<T> *container = dynamic_cast<ObjectContainer<T> *>((*allotted_container).Get());

            return *static_cast<ObjectContainer<T> *>((*allotted_container).Get());
        }
        
        // Calls the callback function to allocate the ObjectContainer (if needed)
        HYP_API UniquePtr<IObjectContainer> *AllotObjectContainer(TypeID type_id);

        HYP_API IObjectContainer &GetObjectContainer(TypeID type_id);
        HYP_API IObjectContainer *TryGetObjectContainer(TypeID type_id);
    };

    HYP_API static ObjectContainerHolder &GetObjectContainerHolder();
};

} // namespace hyperion

#endif