#ifndef HYPERION_V2_CORE_OBJECT_POOL_HPP
#define HYPERION_V2_CORE_OBJECT_POOL_HPP

#include <core/Containers.hpp>
#include <core/IDCreator.hpp>
#include <core/ID.hpp>
#include <core/lib/Mutex.hpp>
#include <core/Name.hpp>
#include <core/lib/HashMap.hpp>
#include <Constants.hpp>
#include <Types.hpp>
#include <util/Defines.hpp>
#include <system/Debug.hpp>

#include <mutex>
#include <atomic>
#include <type_traits>

namespace hyperion::v2 {

class Engine;

template <class T>
struct HandleDefinition;

template <class T>
constexpr bool has_opaque_handle_defined = implementation_exists<HandleDefinition<T>>;

class ObjectContainerBase
{
public:
    virtual ~ObjectContainerBase() = default;

    virtual void IncRefStrong(UInt index) = 0;
    virtual void IncRefWeak(UInt index) = 0;
    virtual void DecRefStrong(UInt index) = 0;
    virtual void DecRefWeak(UInt index) = 0;
};

template <class T>
class ObjectContainer : public ObjectContainerBase
{
    struct ObjectBytes
    {
        alignas(T) UByte bytes[sizeof(T)];
        std::atomic<UInt16> ref_count_strong;
        std::atomic<UInt16> ref_count_weak;

        ObjectBytes()
            : ref_count_strong(0),
              ref_count_weak(0)
        {
        }

        ~ObjectBytes()
        {
            if (HasValue()) {
                Get().~T();
            }
        }

        template <class ...Args>
        T *Construct(Args &&... args)
        {
            AssertThrow(!HasValue());

            return new (bytes) T(std::forward<Args>(args)...);
        }

        HYP_FORCE_INLINE void IncRefStrong()
        {
            ref_count_strong.fetch_add(1, std::memory_order_relaxed);
        }

        HYP_FORCE_INLINE void IncRefWeak()
        {
            AssertThrow(HasValue());

            ref_count_strong.fetch_add(1, std::memory_order_relaxed);
        }

        UInt DecRefStrong()
        {
            AssertThrow(HasValue());

            UInt16 count;

            if ((count = ref_count_strong.fetch_sub(1)) == 1) {
                reinterpret_cast<T *>(bytes)->~T();
            }

            return UInt(count) - 1;
        }

        UInt DecRefWeak()
        {
            UInt16 count = ref_count_weak.fetch_sub(1);

            return UInt(count) - 1;
        }

        HYP_FORCE_INLINE UInt GetRefCountStrong() const
            { return UInt(ref_count_strong.load()); }

        HYP_FORCE_INLINE UInt GetRefCountWeak() const
            { return UInt(ref_count_weak.load()); }

        HYP_FORCE_INLINE T &Get()
        {
            AssertThrow(HasValue());

            return *reinterpret_cast<T *>(bytes);
        }

    private:

        HYP_FORCE_INLINE bool HasValue() const
            { return ref_count_strong.load() != 0; }
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

    HYP_FORCE_INLINE UInt NextIndex()
    {
        const UInt index = IDCreator<>::template ForType<T>().NextID() - 1;

        AssertThrowMsg(
            index < HandleDefinition<T>::max_size,
            "Maximum number of type '%s' allocated! Maximum: %llu\n",
            T::GetClass().GetName(),
            HandleDefinition<T>::max_size
        );

        return index;
    }

    virtual void IncRefStrong(UInt index) override
    {
        m_data[index].IncRefStrong();
    }

    virtual void IncRefWeak(UInt index) override
    {
        m_data[index].IncRefWeak();
    }

    virtual void DecRefStrong(UInt index) override
    {
        if (m_data[index].DecRefStrong() == 0 && m_data[index].GetRefCountWeak() == 0) {
            IDCreator<>::template ForType<T>().FreeID(index + 1);
        }
    }

    virtual void DecRefWeak(UInt index) override
    {
        if (m_data[index].DecRefWeak() == 0 && m_data[index].GetRefCountStrong() == 0) {
            IDCreator<>::template ForType<T>().FreeID(index + 1);
        }
    }

    HYP_FORCE_INLINE T &Get(UInt index)
    {
        return m_data[index].Get();
    }
    
    template <class ...Args>
    HYP_FORCE_INLINE void ConstructAtIndex(UInt index, Args &&... args)
    {
        T *ptr = m_data[index].Construct(std::forward<Args>(args)...);
        ptr->SetID(ID<T> { index + 1 });
    }

private:
    HeapArray<ObjectBytes, max_size> m_data;
    SizeType m_size;
};

class ObjectPool
{
    static struct ObjectContainerHolder
    {
        struct ObjectContainerMap
        {
            // Mutex for accessing the map
            Mutex                                   mutex;

            // Maps component type ID to object container
            HashMap<TypeID, ObjectContainerBase *>  map;

            ObjectContainerMap()                                            = default;
            ObjectContainerMap(const ObjectContainerMap &)                  = delete;
            ObjectContainerMap &operator=(const ObjectContainerMap &)       = delete;
            ObjectContainerMap(ObjectContainerMap &&) noexcept              = delete;
            ObjectContainerMap &operator=(ObjectContainerMap &&) noexcept   = delete;
            ~ObjectContainerMap()                                           = default;
        };

        template <class T>
        struct ObjectContainerDeclaration
        {
            ObjectContainer<T>  container;

            ObjectContainerDeclaration(TypeID type_id, ObjectContainerMap &object_container_map)
            {
                Mutex::Guard guard(object_container_map.mutex);

                object_container_map.map.Set(type_id, &container);
            }

            ObjectContainerDeclaration(const ObjectContainerDeclaration &)                  = delete;
            ObjectContainerDeclaration &operator=(const ObjectContainerDeclaration &)       = delete;
            ObjectContainerDeclaration(ObjectContainerDeclaration &&) noexcept              = delete;
            ObjectContainerDeclaration &operator=(ObjectContainerDeclaration &&) noexcept   = delete;
            ~ObjectContainerDeclaration()                                                   = default;
        };

        static ObjectContainerMap s_object_container_map;

        template <class T>
        static ObjectContainer<T> &GetObjectContainer()
        {
            static ObjectContainerDeclaration<T> object_container_declaration(TypeID::ForType<T>(), s_object_container_map);

            return object_container_declaration.container;
        }

        static ObjectContainerBase *TryGetObjectContainer(TypeID type_id)
        {
            Mutex::Guard guard(s_object_container_map.mutex);

            auto it = s_object_container_map.map.Find(type_id);

            if (it == s_object_container_map.map.End()) {
                return nullptr;
            }

            return it->second;
        }
    } s_object_container_holder;

public:
    template <class T>
    ObjectContainer<T> &GetContainer()
    {
        static_assert(has_opaque_handle_defined<T>, "Object type not viable for GetContainer<T> : Does not support handles");

        return s_object_container_holder.GetObjectContainer<T>();
    }

    ObjectContainerBase *TryGetContainer(TypeID type_id)
    {
        return s_object_container_holder.TryGetObjectContainer(type_id);
    }
};

#define DEF_HANDLE(T, _max_size) \
    class T; \
    template <> \
    struct HandleDefinition< T > \
    { \
        static constexpr const char *class_name = HYP_STR(T); \
        static constexpr SizeType max_size = (_max_size); \
        \
        static constexpr const char *GetClassNameString() \
        { \
            return class_name; \
        } \
        \
        static Name GetNameForType() \
        { \
            static const Name name = HYP_NAME(T); \
            return name; \
        } \
    }


#define DEF_HANDLE_NS(ns, T, _max_size) \
    namespace ns { \
    class T; \
    } \
    \
    template <> \
    struct HandleDefinition< ns::T > \
    { \
        static constexpr const char *class_name = HYP_STR(ns) "::" HYP_STR(T); \
        static constexpr SizeType max_size = (_max_size); \
        \
        static Name GetNameForType() \
        { \
            static const Name name = HYP_NAME(ns::T); \
            return name; \
        } \
    }

DEF_HANDLE(Texture,                      16384);
DEF_HANDLE(Camera,                       64);
DEF_HANDLE(Entity,                       32768);
DEF_HANDLE(Node,                         65536);
DEF_HANDLE(Bone,                         65536);
DEF_HANDLE(Mesh,                         65536);
DEF_HANDLE(Framebuffer,                  256);
DEF_HANDLE(Shader,                       16384);
DEF_HANDLE(RenderGroup,                  256);
DEF_HANDLE(Skeleton,                     512);
DEF_HANDLE(Scene,                        64);
DEF_HANDLE(RenderEnvironment,            64);
DEF_HANDLE(Light,                        256);
DEF_HANDLE(TLAS,                         32);
DEF_HANDLE(BLAS,                         16384);
DEF_HANDLE(Material,                     32768);
DEF_HANDLE(MaterialGroup,                256);
DEF_HANDLE(World,                        8);
DEF_HANDLE(AudioSource,                  8192);
DEF_HANDLE(EnvProbe,                     2048);
DEF_HANDLE(UIScene,                      8);
DEF_HANDLE(ParticleSystem,               8);
DEF_HANDLE(ComputePipeline,              16384);
DEF_HANDLE(ParticleSpawner,              512);
DEF_HANDLE(Script,                       8192);
DEF_HANDLE(UIObject,                     8192);
DEF_HANDLE_NS(physics, RigidBody,        8192);

DEF_HANDLE(PostProcessingEffect,         512);
DEF_HANDLE(UIRenderer,                   1);

DEF_HANDLE(GaussianSplattingInstance,    16);
DEF_HANDLE(GaussianSplatting,            16);

#undef DEF_HANDLE
#undef DEF_HANDLE_NS

} // namespace hyperion::v2

#endif