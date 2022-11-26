#ifndef HYPERION_V2_COMPONENT_HPP
#define HYPERION_V2_COMPONENT_HPP

#include <core/Containers.hpp>
#include <core/lib/TypeMap.hpp>
#include <core/lib/FlatMap.hpp>
#include <core/Handle.hpp>
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
struct HasOpaqueHandleDefined;

#define HAS_OPAQUE_HANDLE(T) \
    class T; \
    template <> \
    struct HasOpaqueHandleDefined< T > \
    { \
    };


#define HAS_OPAQUE_HANDLE_NS(ns, T) \
    namespace ns { \
    class T; \
    } \
    \
    template <> \
    struct HasOpaqueHandleDefined< ns::T > \
    { \
    };

template <class T>
constexpr bool has_opaque_handle_defined = implementation_exists<HasOpaqueHandleDefined<T>>;

template <class T>
class ObjectContainer
{
public:
    static constexpr SizeType max_items = 16384;

    struct ObjectBytes
    {
        // TODO: Memory order

        alignas(T) UByte bytes[sizeof(T)];
        std::atomic<UInt16> ref_count;

        ObjectBytes()
            : ref_count(0)
        {
            Memory::Set(bytes, 0, sizeof(T));
        }

        ~ObjectBytes()
        {
            if (HasValue()) {
                Get().~T();
                Memory::Set(bytes, 0, sizeof(T));
            }
        }

        template <class ...Args>
        T *Construct(Args &&... args)
        {
            AssertThrow(!HasValue());

            return new (bytes) T(std::forward<Args>(args)...);
        }

        UInt IncRef()
        {
            return UInt(ref_count.fetch_add(1)) + 1;
        }

        UInt DecRef()
        {
            AssertThrow(HasValue());

            UInt16 count;

            if ((count = ref_count.fetch_sub(1)) == 1) {
                reinterpret_cast<T *>(bytes)->~T();
                Memory::Set(bytes, 0, sizeof(T));
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
            { return ref_count.load() != 0; }
    };

    ObjectContainer()
        : m_size(0)
    {
    }

    ObjectContainer(const ObjectContainer &other) = delete;
    ObjectContainer &operator=(const ObjectContainer &other) = delete;

    ~ObjectContainer()
    {
        // for (SizeType index = 0; index < m_size; index++) {
        //     if (m_data[index].has_value) {
        //         m_data[index].Destruct();
        //     }
        // }
    }

    HYP_FORCE_INLINE UInt NextIndex()
    {
        return GetIDCreator<T>().NextID() - 1;
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
    HeapArray<ObjectBytes, max_items> m_data;
    SizeType m_size;
};

class ComponentSystem
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

HAS_OPAQUE_HANDLE(Texture);
HAS_OPAQUE_HANDLE(Camera);
HAS_OPAQUE_HANDLE(Entity);
HAS_OPAQUE_HANDLE(Mesh);
HAS_OPAQUE_HANDLE(Framebuffer);
HAS_OPAQUE_HANDLE(RenderPass);
HAS_OPAQUE_HANDLE(Shader);
HAS_OPAQUE_HANDLE(RendererInstance);
HAS_OPAQUE_HANDLE(Skeleton);
HAS_OPAQUE_HANDLE(Scene);
HAS_OPAQUE_HANDLE(Light);
HAS_OPAQUE_HANDLE(TLAS);
HAS_OPAQUE_HANDLE(BLAS);
HAS_OPAQUE_HANDLE(Material);
HAS_OPAQUE_HANDLE(MaterialGroup);
HAS_OPAQUE_HANDLE(World);
HAS_OPAQUE_HANDLE(AudioSource);
HAS_OPAQUE_HANDLE(RenderEnvironment);
HAS_OPAQUE_HANDLE(EnvProbe);
HAS_OPAQUE_HANDLE(UIScene);
HAS_OPAQUE_HANDLE(ParticleSystem);
HAS_OPAQUE_HANDLE(ComputePipeline);
HAS_OPAQUE_HANDLE(ParticleSpawner);
HAS_OPAQUE_HANDLE(Script);
HAS_OPAQUE_HANDLE_NS(physics, RigidBody);

// to get rid of:
HAS_OPAQUE_HANDLE(PostProcessingEffect);
HAS_OPAQUE_HANDLE(ShadowRenderer);
HAS_OPAQUE_HANDLE(VoxelConeTracing);
HAS_OPAQUE_HANDLE(SparseVoxelOctree);
HAS_OPAQUE_HANDLE(CubemapRenderer);
HAS_OPAQUE_HANDLE(UIRenderer);
HAS_OPAQUE_HANDLE(Voxelizer);

} // namespace hyperion::v2

#endif