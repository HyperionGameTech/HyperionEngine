#ifndef HYPERION_V2_CORE_OBJECT_POOL_HPP
#define HYPERION_V2_CORE_OBJECT_POOL_HPP

#include <core/Containers.hpp>
#include <core/lib/TypeMap.hpp>
#include <core/lib/FlatMap.hpp>
#include <core/HandleID.hpp>
#include <Constants.hpp>
#include <Types.hpp>
#include <util/Defines.hpp>
#include <system/Debug.hpp>

#include <mutex>
#include <atomic>
#include <type_traits>

namespace hyperion::v2 {

class Engine;

struct IDCreator
{
    TypeID type_id;
    std::atomic<UInt> id_counter { 0u };
    std::mutex free_id_mutex;
    Queue<UInt> free_indices;

    UInt NextID()
    {
        std::lock_guard guard(free_id_mutex);

        if (free_indices.Empty()) {
            return id_counter.fetch_add(1) + 1;
        }

        return free_indices.Pop();
    }

    void FreeID(UInt index)
    {
        std::lock_guard guard(free_id_mutex);

        free_indices.Push(index);
    }
};

template <class T>
IDCreator &GetIDCreator()
{
    static IDCreator id_creator { TypeID::ForType<T>() };

    return id_creator;
}

template <class T>
struct HandleDefinition;

template <class T>
constexpr bool has_opaque_handle_defined = implementation_exists<HandleDefinition<T>>;

template <class T>
class ObjectContainer
{
public:
    static constexpr SizeType max_size = HandleDefinition<T>::max_size;

    struct ObjectBytes
    {
        alignas(T) UByte bytes[sizeof(T)];
        std::atomic<UInt16> ref_count;

        ObjectBytes()
            : ref_count(0)
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

        HYP_FORCE_INLINE void IncRef()
        {
            ref_count.fetch_add(1, std::memory_order_relaxed);
        }

        UInt DecRef()
        {
            AssertThrow(HasValue());

            UInt16 count;

            if ((count = ref_count.fetch_sub(1)) == 1) {
                reinterpret_cast<T *>(bytes)->~T();
            }

            return UInt(count) - 1;
        }

        HYP_FORCE_INLINE T &Get()
        {
            AssertThrow(HasValue());

            return *reinterpret_cast<T *>(bytes);
        }

    private:

        HYP_FORCE_INLINE bool HasValue() const
            { return ref_count.load(std::memory_order_relaxed) != 0; }
    };

    ObjectContainer()
        : m_size(0)
    {
    }

    ObjectContainer(const ObjectContainer &other) = delete;
    ObjectContainer &operator=(const ObjectContainer &other) = delete;

    ~ObjectContainer() = default;

    HYP_FORCE_INLINE UInt NextIndex()
    {
        const UInt index = GetIDCreator<T>().NextID() - 1;

        AssertThrowMsg(
            index < HandleDefinition<T>::max_size,
            "Maximum number of type '%s' allocated! Maximum: %llu\n",
            T::GetClass().GetName(),
            HandleDefinition<T>::max_size
        );

        return index;
    }

    HYP_FORCE_INLINE void IncRef(UInt index)
    {
        m_data[index].IncRef();
    }

    HYP_FORCE_INLINE void DecRef(UInt index)
    {
        if (m_data[index].DecRef() == 0) {
            GetIDCreator<T>().FreeID(index + 1);
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
        ptr->SetID(typename T::ID { index + 1 });
    }

private:
    HeapArray<ObjectBytes, max_size> m_data;
    SizeType m_size;
};

class ObjectPool
{
public:
    template <class T>
    ObjectContainer<T> &GetContainer()
    {
        static_assert(has_opaque_handle_defined<T>, "Object type not viable for GetContainer<T> : Does not support handles");

        static ObjectContainer<T> container;

        return container;
    }
};

#define DEF_HANDLE(T, _max_size) \
    class T; \
    template <> \
    struct HandleDefinition< T > \
    { \
        static constexpr SizeType max_size = (_max_size); \
    };


#define HAS_OPAQUE_HANDLE_NS(ns, T, _max_size) \
    namespace ns { \
    class T; \
    } \
    \
    template <> \
    struct HandleDefinition< ns::T > \
    { \
        static constexpr SizeType max_size = (_max_size); \
    };

DEF_HANDLE(Texture,                      16384);
DEF_HANDLE(Camera,                       256);
DEF_HANDLE(Entity,                       32768);
DEF_HANDLE(Mesh,                         65536);
DEF_HANDLE(Framebuffer,                  256);
DEF_HANDLE(RenderPass,                   1024);
DEF_HANDLE(Shader,                       1024);
DEF_HANDLE(RendererInstance,             256);
DEF_HANDLE(Skeleton,                     512);
DEF_HANDLE(Scene,                        64);
DEF_HANDLE(RenderEnvironment,            64);
DEF_HANDLE(Light,                        256);
DEF_HANDLE(TLAS,                         32);
DEF_HANDLE(BLAS,                         32768);
DEF_HANDLE(Material,                     32768);
DEF_HANDLE(MaterialGroup,                256);
DEF_HANDLE(World,                        8);
DEF_HANDLE(AudioSource,                  16384);
DEF_HANDLE(EnvProbe,                     16384);
DEF_HANDLE(UIScene,                      8);
DEF_HANDLE(ParticleSystem,               16);
DEF_HANDLE(ComputePipeline,              16384);
DEF_HANDLE(ParticleSpawner,              512);
DEF_HANDLE(Script,                       16384);
HAS_OPAQUE_HANDLE_NS(physics, RigidBody, 16384);

// to get rid of:
DEF_HANDLE(PostProcessingEffect,         512);
DEF_HANDLE(ShadowRenderer,               512);
DEF_HANDLE(CubemapRenderer,              512);
DEF_HANDLE(VoxelConeTracing,             1);
DEF_HANDLE(SparseVoxelOctree,            1);
DEF_HANDLE(UIRenderer,                   1);
DEF_HANDLE(Voxelizer,                    1);

#undef DEF_HANDLE

} // namespace hyperion::v2

#endif