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

struct ObjectBytesBase
{
    IObjectContainer    *container;
    uint32              index;
    AtomicVar<uint16>   ref_count_strong;
    AtomicVar<uint16>   ref_count_weak;

    ObjectBytesBase()
        : container(nullptr),
          index(~0u),
          ref_count_strong(0),
          ref_count_weak(0)
    {
    }

    HYP_FORCE_INLINE uint32 GetRefCountStrong() const
        { return uint32(ref_count_strong.Get(MemoryOrder::ACQUIRE)); }

    HYP_FORCE_INLINE uint32 GetRefCountWeak() const
        { return uint32(ref_count_weak.Get(MemoryOrder::ACQUIRE)); }

    HYP_FORCE_INLINE void IncRefStrong()
        { ref_count_strong.Increment(1, MemoryOrder::RELAXED); }

    HYP_FORCE_INLINE void IncRefWeak()
        { ref_count_weak.Increment(1, MemoryOrder::RELAXED); }
};

// @TODO Change ObjectPool to use the new MemoryPool<T> template class underneath, as it supports dynamic resizing of the buffer
// But first, we need a way to map from address directly to index, so GetObjectIndex() will work properly without needing to iterate all blocks

class IObjectContainer
{
public:
    virtual ~IObjectContainer() = default;

    virtual uint32 NextIndex() = 0;
    
    virtual void IncRefStrong(ObjectBytesBase *) = 0;
    virtual void IncRefStrong(uint32 index) = 0;
    virtual void IncRefWeak(ObjectBytesBase *) = 0;
    virtual void IncRefWeak(uint32 index) = 0;
    virtual void DecRefStrong(ObjectBytesBase *) = 0;
    virtual void DecRefStrong(uint32 index) = 0;
    virtual void DecRefWeak(ObjectBytesBase *) = 0;
    virtual void DecRefWeak(uint32 index) = 0;
    virtual uint32 GetRefCountStrong(ObjectBytesBase *) = 0;
    virtual uint32 GetRefCountStrong(uint32 index) = 0;
    virtual uint32 GetRefCountWeak(ObjectBytesBase *) = 0;
    virtual uint32 GetRefCountWeak(uint32 index) = 0;

    virtual AnyRef GetObject(ObjectBytesBase *) = 0;
    virtual ObjectBytesBase *GetObjectBytes(uint32 index) = 0;
    virtual uint32 GetObjectIndex(const ObjectBytesBase *ptr) = 0;
    virtual TypeID GetObjectTypeID() const = 0;

    virtual const IHypObjectInitializer *GetObjectInitializer(uint32 index) = 0;

    virtual void *ConstructAtIndex(uint32 index) = 0;

    virtual IDGenerator &GetIDGenerator() = 0;
};

template <class T>
struct ObjectBytes final : ObjectBytesBase
{
    alignas(T) ubyte    bytes[sizeof(T)];

    ObjectBytes()
    {
    }

    ObjectBytes(const ObjectBytes &)                = delete;
    ObjectBytes &operator=(const ObjectBytes &)     = delete;
    ObjectBytes(ObjectBytes &&) noexcept            = delete;
    ObjectBytes &operator=(ObjectBytes &&) noexcept = delete;
    ~ObjectBytes()                                  = default;

    template <class... Args>
    T *Construct(Args &&... args)
    {
        Memory::ConstructWithContext<T, HypObjectInitializerGuard<T>>(bytes, std::forward<Args>(args)...);
        Get().SetID(ID<T> { index + 1 });

        return reinterpret_cast<T *>(&bytes[0]);
    }

    uint32 DecRefStrong()
    {
        uint16 count;

        if ((count = ref_count_strong.Decrement(1, MemoryOrder::ACQUIRE_RELEASE)) == 1) {
            reinterpret_cast<T *>(bytes)->~T();

#ifdef HYP_DEBUG_MODE
            AssertThrow(container != nullptr);
            AssertThrow(index != ~0u);
#endif

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

template <class ElementType>
static inline void ObjectContainer_OnBlockAllocated(void *pool, ElementType *ptr, uint32 offset, uint32 count)
{
    for (uint32 index = 0; index < count; index++) {
        ptr[index].container = static_cast<IObjectContainer *>(pool);
        ptr[index].index = offset + index;
    }
}

template <class T>
class ObjectContainer final : private MemoryPool<ObjectBytes<T>, 128, ObjectContainer_OnBlockAllocated<ObjectBytes<T>>>, public IObjectContainer
{
    using Base = MemoryPool<ObjectBytes<T>, 128, ObjectContainer_OnBlockAllocated<ObjectBytes<T>>>;

    using ObjectBytes = ObjectBytes<T>;

public:
    static constexpr SizeType max_size = HandleDefinition<T>::max_size;

    ObjectContainer()
        : Base(128)
    {
    }

    ObjectContainer(const ObjectContainer &other)                   = delete;
    ObjectContainer &operator=(const ObjectContainer &other)        = delete;
    ObjectContainer(ObjectContainer &&other) noexcept               = delete;
    ObjectContainer &operator=(ObjectContainer &&other) noexcept    = delete;
    virtual ~ObjectContainer() override                             = default;

    virtual uint32 NextIndex() override
    {
        return Base::AcquireIndex();
    }

    HYP_FORCE_INLINE ObjectBytes *Allocate()
    {
        const uint32 index = Base::AcquireIndex();

        ObjectBytes *element = &Base::GetElement(index);

#ifdef HYP_DEBUG_MODE
        AssertThrow(uintptr_t(element->container) == uintptr_t(this));
        AssertThrow(element->index == index);
#endif

        return element;
    }

    virtual void IncRefStrong(ObjectBytesBase *ptr) override
    {
        static_cast<ObjectBytes *>(ptr)->IncRefStrong();
    }

    virtual void IncRefStrong(uint32 index) override
    {
        Base::GetElement(index).IncRefStrong();
    }

    virtual void IncRefWeak(ObjectBytesBase *ptr) override
    {
        static_cast<ObjectBytes *>(ptr)->IncRefWeak();
    }

    virtual void IncRefWeak(uint32 index) override
    {
        Base::GetElement(index).IncRefWeak();
    }

    virtual void DecRefStrong(ObjectBytesBase *ptr) override
    {
        static_cast<ObjectBytes *>(ptr)->DecRefStrong();
    }

    virtual void DecRefStrong(uint32 index) override
    {
        Base::GetElement(index).DecRefStrong();
    }

    virtual void DecRefWeak(ObjectBytesBase *ptr) override
    {
        static_cast<ObjectBytes *>(ptr)->DecRefWeak();
    }

    virtual void DecRefWeak(uint32 index) override
    {
        Base::GetElement(index).DecRefWeak();
    }

    virtual uint32 GetRefCountStrong(ObjectBytesBase *ptr) override
    {
        return static_cast<ObjectBytes *>(ptr)->GetRefCountStrong();
    }

    virtual uint32 GetRefCountStrong(uint32 index) override
    {
        return Base::GetElement(index).GetRefCountStrong();
    }

    virtual uint32 GetRefCountWeak(ObjectBytesBase *ptr) override
    {
        return static_cast<ObjectBytes *>(ptr)->GetRefCountWeak();
    }

    virtual uint32 GetRefCountWeak(uint32 index) override
    {
        return Base::GetElement(index).GetRefCountWeak();
    }

    virtual AnyRef GetObject(ObjectBytesBase *ptr) override
    {
        if (!ptr) {
            return AnyRef(TypeID::ForType<T>(), nullptr);
        }

        return AnyRef(TypeID::ForType<T>(), static_cast<ObjectBytes *>(ptr)->GetPointer());
    }

    virtual ObjectBytesBase *GetObjectBytes(uint32 index) override
    {
        return &Base::GetElement(index);
    }

    virtual uint32 GetObjectIndex(const ObjectBytesBase *ptr) override
    {
        if (!ptr) {
            return ~0u;
        }

        return static_cast<const ObjectBytes *>(ptr)->index;
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
            return Base::GetElement(index).Get().GetObjectInitializer();
        }

        return nullptr;
    }
    
    virtual void *ConstructAtIndex(uint32 index) override
    {
        ObjectBytes &element = Base::GetElement(index);
        element.Construct();

        return &element.Get();
    }
    
    template <class Arg0, class... Args>
    HYP_FORCE_INLINE void *ConstructAtIndex(uint32 index, Arg0 &&arg0, Args &&... args)
    {
        ObjectBytes &element = Base::GetElement(index);
        element.Construct(std::forward<Arg0>(arg0), std::forward<Args>(args)...);

        return &element.Get();
    }

    virtual IDGenerator &GetIDGenerator() override
    {
        return Base::m_id_generator;
    }
};

class ObjectPool
{
public:
    struct ObjectContainerMap
    {
        // Maps type ID to object container
        // Use a linked list so that iterators are never invalidated.
        LinkedList<Pair<TypeID, UniquePtr<IObjectContainer>>>    map;

#ifdef HYP_ENABLE_MT_CHECK
        DataRaceDetector                                            data_race_detector;
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
            static ObjectContainerDeclaration<T> object_container_declaration(allotted_container);

            return *static_cast<ObjectContainer<T> *>((*allotted_container).Get());
        }
        
        // Calls the callback function to allocate the ObjectContainer (if needed)
        HYP_API UniquePtr<IObjectContainer> *AllotObjectContainer(TypeID type_id);

        HYP_API IObjectContainer &GetObjectContainer(TypeID type_id);
        HYP_API IObjectContainer *TryGetObjectContainer(TypeID type_id);
    };

    HYP_API static ObjectContainerHolder &GetObjectContainerHolder();

    template <class T>
    HYP_FORCE_INLINE static ObjectContainer<T> &GetContainer(UniquePtr<IObjectContainer> &allotted_container)
    {
        static_assert(has_opaque_handle_defined<T>, "Object type not viable for GetContainer<T> : Does not support handles");

        return GetObjectContainerHolder().GetObjectContainer<T>(allotted_container);
    }

    template <class T>
    HYP_FORCE_INLINE static UniquePtr<IObjectContainer> *AllotContainer()
    {
        static_assert(has_opaque_handle_defined<T>, "Object type not viable for GetContainer<T> : Does not support handles");

        return GetObjectContainerHolder().AllotObjectContainer(TypeID::ForType<T>());
    }

    HYP_FORCE_INLINE static IObjectContainer *TryGetContainer(TypeID type_id)
    {
        return GetObjectContainerHolder().TryGetObjectContainer(type_id);
    }

    HYP_FORCE_INLINE static IObjectContainer &GetContainer(TypeID type_id)
    {
        return GetObjectContainerHolder().GetObjectContainer(type_id);
    }
};

} // namespace hyperion

#endif