/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_OBJECT_POOL_HPP
#define HYPERION_CORE_OBJECT_POOL_HPP

#include <core/threading/Mutex.hpp>
#include <core/system/Debug.hpp>
#include <core/containers/LinkedList.hpp>
#include <core/memory/UniquePtr.hpp>
#include <core/IDGenerator.hpp>
#include <core/ID.hpp>
#include <core/Util.hpp>
#include <core/Defines.hpp>

#include <Constants.hpp>
#include <Types.hpp>

#include <type_traits>

// #define HYP_OBJECT_POOL_DEBUG
namespace hyperion {

class Engine;

template <class T>
struct HandleDefinition;

template <class T>
constexpr bool has_opaque_handle_defined = implementation_exists<HandleDefinition<T>>;

class ObjectContainerBase
{
public:
    virtual ~ObjectContainerBase() = default;

    virtual void IncRefStrong(uint32 index) = 0;
    virtual void IncRefWeak(uint32 index) = 0;
    virtual void DecRefStrong(uint32 index) = 0;
    virtual void DecRefWeak(uint32 index) = 0;
    virtual uint32 GetRefCountStrong(uint32 index) = 0;
    virtual uint32 GetRefCountWeak(uint32 index) = 0;
};

template <class T>
class ObjectContainer : public ObjectContainerBase
{
    struct ObjectBytes
    {
        alignas(T) ubyte    bytes[sizeof(T)];
        AtomicVar<uint16>   ref_count_strong;
        AtomicVar<uint16>   ref_count_weak;

#ifdef HYP_OBJECT_POOL_DEBUG
        bool                has_value;
#endif

        ObjectBytes()
            : ref_count_strong(0),
              ref_count_weak(0)
        {
#ifdef HYP_OBJECT_POOL_DEBUG
            has_value = false;
#endif
        }

        ObjectBytes(const ObjectBytes &)                = delete;
        ObjectBytes &operator=(const ObjectBytes &)     = delete;
        ObjectBytes(ObjectBytes &&) noexcept            = delete;
        ObjectBytes &operator=(ObjectBytes &&) noexcept = delete;

        ~ObjectBytes()
        {
#ifdef HYP_OBJECT_POOL_DEBUG
            AssertThrow(!HasValue());
#endif
        }

        template <class ...Args>
        T *Construct(Args &&... args)
        {
#ifdef HYP_OBJECT_POOL_DEBUG
            AssertThrow(!HasValue());
#endif

            return new (bytes) T(std::forward<Args>(args)...);
        }

        HYP_FORCE_INLINE void IncRefStrong()
        {
#ifdef HYP_OBJECT_POOL_DEBUG
            AssertThrow(HasValue());
#endif

            ref_count_strong.Increment(1, MemoryOrder::RELAXED);
        }

        HYP_FORCE_INLINE void IncRefWeak()
        {
            ref_count_weak.Increment(1, MemoryOrder::RELAXED);
        }

        uint32 DecRefStrong()
        {
#ifdef HYP_OBJECT_POOL_DEBUG
            AssertThrow(HasValue());
#endif

            uint16 count;

            if ((count = ref_count_strong.Decrement(1, MemoryOrder::ACQUIRE_RELEASE)) == 1) {
                reinterpret_cast<T *>(bytes)->~T();
            }

            return uint(count) - 1;
        }

        uint32 DecRefWeak()
        {
            const uint16 count = ref_count_weak.Decrement(1, MemoryOrder::ACQUIRE_RELEASE);

            return uint32(count) - 1;
        }

        HYP_FORCE_INLINE uint32 GetRefCountStrong() const
            { return uint32(ref_count_strong.Get(MemoryOrder::ACQUIRE)); }

        HYP_FORCE_INLINE uint32 GetRefCountWeak() const
            { return uint32(ref_count_weak.Get(MemoryOrder::ACQUIRE)); }

        HYP_FORCE_INLINE T &Get()
            { return *reinterpret_cast<T *>(bytes); }

        HYP_FORCE_INLINE T *GetPointer()
            { return reinterpret_cast<T *>(bytes); }

        HYP_FORCE_INLINE const T *GetPointer() const
            { return reinterpret_cast<const T *>(bytes); }

    private:
#ifdef HYP_OBJECT_POOL_DEBUG
        HYP_FORCE_INLINE bool HasValue() const
            { return has_value; }
#endif
    };

public:
    static constexpr SizeType max_size = HandleDefinition<T>::max_size;

    ObjectContainer()
        : m_size(0)
    {
    }

    ObjectContainer(const ObjectContainer &other)                   = delete;
    ObjectContainer &operator=(const ObjectContainer &other)        = delete;
    ObjectContainer(ObjectContainer &&other) noexcept               = delete;
    ObjectContainer &operator=(ObjectContainer &&other) noexcept    = delete;
    virtual ~ObjectContainer() override                             = default;

    HYP_FORCE_INLINE uint32 NextIndex()
    {
        const uint32 index = m_id_generator.NextID() - 1;

        AssertThrowMsg(
            index < HandleDefinition<T>::max_size,
            "Maximum number of type '%s' allocated! Maximum: %llu\n",
            TypeName<T>().Data(),
            HandleDefinition<T>::max_size
        );

        return index;
    }

    virtual void IncRefStrong(uint32 index) override
    {
        m_data[index].IncRefStrong();
    }

    virtual void IncRefWeak(uint32 index) override
    {
        m_data[index].IncRefWeak();
    }

    virtual void DecRefStrong(uint32 index) override
    {
        if (m_data[index].DecRefStrong() == 0) {
#ifdef HYP_OBJECT_POOL_DEBUG
            m_data[index].has_value = false;
#endif

            if (m_data[index].GetRefCountWeak() == 0) {
                m_id_generator.FreeID(index + 1);
            }
        }
    }

    virtual void DecRefWeak(uint32 index) override
    {
        if (m_data[index].DecRefWeak() == 0 && m_data[index].GetRefCountStrong() == 0) {
            m_id_generator.FreeID(index + 1);
        }
    }

    virtual uint32 GetRefCountStrong(uint32 index) override
    {
        return m_data[index].GetRefCountStrong();
    }

    virtual uint32 GetRefCountWeak(uint32 index) override
    {
        return m_data[index].GetRefCountWeak();
    }

    HYP_FORCE_INLINE
    T *GetPointer(uint32 index)
        { return m_data[index].GetPointer(); }

    HYP_FORCE_INLINE
    const T *GetPointer(uint32 index) const
        { return m_data[index].GetPointer(); }

    HYP_FORCE_INLINE
    T &Get(uint32 index)
        { return m_data[index].Get(); }

    HYP_FORCE_INLINE
    ObjectBytes &GetObjectBytes(uint32 index)
        { return m_data[index]; }

    HYP_FORCE_INLINE
    const ObjectBytes &GetObjectBytes(uint32 index) const
        { return m_data[index]; }
    
    template <class ...Args>
    HYP_FORCE_INLINE void ConstructAtIndex(uint32 index, Args &&... args)
    {
        T *ptr = m_data[index].Construct(std::forward<Args>(args)...);

#ifdef HYP_OBJECT_POOL_DEBUG
        m_data[index].has_value = true;
#endif

        ptr->SetID(ID<T> { index + 1 });
    }

private:
    HeapArray<ObjectBytes, max_size>    m_data;
    SizeType                            m_size;
    IDGenerator                         m_id_generator;
};

class ObjectPool
{
public:
    struct ObjectContainerMap
    {
        // Mutex for accessing the map
        Mutex                                                       mutex;

        // Maps type ID to object container
        // Use a linked list so that iterators are never invalidated.
        LinkedList<Pair<TypeID, UniquePtr<ObjectContainerBase>>>    map;

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
            ObjectContainerDeclaration(UniquePtr<ObjectContainerBase> *allotted_container)
            {
                AssertThrowMsg(allotted_container != nullptr, "Allotted container pointer was null!");

                if (*allotted_container != nullptr) {
                    // Already allocated
                    return;
                }

                (*allotted_container).Reset(new ObjectContainer<T>());
            }

            ObjectContainerDeclaration(const ObjectContainerDeclaration &)                  = delete;
            ObjectContainerDeclaration &operator=(const ObjectContainerDeclaration &)       = delete;
            ObjectContainerDeclaration(ObjectContainerDeclaration &&) noexcept              = delete;
            ObjectContainerDeclaration &operator=(ObjectContainerDeclaration &&) noexcept   = delete;
            ~ObjectContainerDeclaration()                                                   = default;
        };

        ObjectContainerMap object_container_map;

        template <class T>
        ObjectContainer<T> &GetObjectContainer(UniquePtr<ObjectContainerBase> *allotted_container)
        {
            static ObjectContainerDeclaration<T> object_container_declaration(allotted_container);

            return *static_cast<ObjectContainer<T> *>((*allotted_container).Get());
        }
        
        HYP_API UniquePtr<ObjectContainerBase> *AllotObjectContainer(TypeID type_id);

        HYP_API ObjectContainerBase *TryGetObjectContainer(TypeID type_id);
    };

    HYP_API static ObjectContainerHolder &GetObjectContainerHolder();

    template <class T>
    HYP_FORCE_INLINE static ObjectContainer<T> &GetContainer(UniquePtr<ObjectContainerBase> *allotted_container)
    {
        static_assert(has_opaque_handle_defined<T>, "Object type not viable for GetContainer<T> : Does not support handles");

        return GetObjectContainerHolder().GetObjectContainer<T>(allotted_container);
    }

    template <class T>
    HYP_FORCE_INLINE static UniquePtr<ObjectContainerBase> *AllotContainer()
    {
        static_assert(has_opaque_handle_defined<T>, "Object type not viable for GetContainer<T> : Does not support handles");

        return GetObjectContainerHolder().AllotObjectContainer(TypeID::ForType<T>());
    }

    HYP_FORCE_INLINE static ObjectContainerBase *TryGetContainer(TypeID type_id)
    {
        return GetObjectContainerHolder().TryGetObjectContainer(type_id);
    }
};

} // namespace hyperion

#endif