#ifndef HYPERION_RENDER_RESOURCE_MANAGER_HPP
#define HYPERION_RENDER_RESOURCE_MANAGER_HPP

#include <core/memory/UniquePtr.hpp>
#include <core/containers/Bitset.hpp>
#include <core/Handle.hpp>
#include <core/ID.hpp>

#include <rendering/SafeDeleter.hpp>

#include <Types.hpp>

namespace hyperion {

extern HYP_API SafeDeleter *g_safe_deleter;

enum ResourceUsageType : uint
{
    RESOURCE_USAGE_TYPE_INVALID = uint(-1),
    RESOURCE_USAGE_TYPE_MESH = 0,
    RESOURCE_USAGE_TYPE_MATERIAL,
    RESOURCE_USAGE_TYPE_SKELETON,
    RESOURCE_USAGE_TYPE_MAX
};

template <class T>
struct ResourceUsageTypeMap { };

template <>
struct ResourceUsageTypeMap<Mesh>
    { enum { value = RESOURCE_USAGE_TYPE_MESH }; };

template <>
struct ResourceUsageTypeMap<Material>
    { enum { value = RESOURCE_USAGE_TYPE_MATERIAL }; };

template <>
struct ResourceUsageTypeMap<Skeleton>
    { enum { value = RESOURCE_USAGE_TYPE_SKELETON }; };

struct ResourceUsageMapBase
{
    Bitset  usage_bits;

    virtual ~ResourceUsageMapBase() = default;

    virtual void TakeUsagesFrom(ResourceUsageMapBase *other, bool use_soft_references) = 0;

    virtual void Reset()
    {
        usage_bits.Clear();
    }
};

template <class T>
struct ResourceUsageMap : ResourceUsageMapBase
{
    HashMap<ID<T>, Handle<T>>   handles;

    virtual ~ResourceUsageMap() override
    {
        for (auto &it : handles) {
            // Use SafeRelease to defer the actual destruction of the resource.
            // This is used so that any resources that will require a mutex lock to release render side resources
            // will not cause a deadlock.
            DebugLog(LogType::Debug, "Safe releasing handle of type %s for resource ID: %u\n", TypeName<T>().Data(), it.first.Value());
            
            g_safe_deleter->SafeReleaseHandle(std::move(it.second));
        }
    }

    virtual void TakeUsagesFrom(ResourceUsageMapBase *other, bool use_soft_references) override
    {
        ResourceUsageMap<T> *other_map = static_cast<ResourceUsageMap<T> *>(other);

        // If any of the bitsets are different sizes, resize them to match the largest one,
        // this makes ~ and & operations work as expected
        if (usage_bits.NumBits() > other_map->usage_bits.NumBits()) {
            other_map->usage_bits.Resize(usage_bits.NumBits());
        } else if (usage_bits.NumBits() < other_map->usage_bits.NumBits()) {
            usage_bits.Resize(other_map->usage_bits.NumBits());
        }

        SizeType first_set_bit_index;

        Bitset removed_id_bits = usage_bits & ~other_map->usage_bits;

        // Iterate over the bits that were removed, and drop the references to them.
        while ((first_set_bit_index = removed_id_bits.FirstSetBitIndex()) != -1) {
            const ID<T> id = ID<T>::FromIndex(first_set_bit_index);

            // sanity check
            AssertThrow(usage_bits.Test(first_set_bit_index));
            AssertThrow(!other_map->usage_bits.Test(first_set_bit_index));

            const auto it = handles.Find(id);

#ifdef HYP_DEBUG_MODE
            AssertThrow(it != handles.End());

            if (!it->second.IsValid()) {
                DebugLog(LogType::Warn, "When removing no longer used resources, handle for object of type %s with ID #%u was not valid\n",
                    TypeName<T>().Data(), id.Value());
            }
#endif
            // Use SafeRelease to defer the actual destruction of the resource.
            // This is used so that any resources that will require a mutex lock to release render side resources
            // will not cause a deadlock.
            g_safe_deleter->SafeReleaseHandle(std::move(it->second));

            removed_id_bits.Set(first_set_bit_index, false);
        }

        Bitset newly_added_id_bits = other_map->usage_bits & ~usage_bits;

        while ((first_set_bit_index = newly_added_id_bits.FirstSetBitIndex()) != -1) {
            const ID<T> id = ID<T>::FromIndex(first_set_bit_index);

            // sanity check
            AssertThrow(!usage_bits.Test(first_set_bit_index));
            AssertThrow(other_map->usage_bits.Test(first_set_bit_index));

            // if (use_soft_references) {
            //     // Increment reference count
            //     handles.Set(id, Handle<T>(id));
            // } else {
                // Move the handle from the other map to this map
                const auto it = other_map->handles.Find(id);

#ifdef HYP_DEBUG_MODE
                AssertThrow(it != other_map->handles.End());
                AssertThrow(it->second.IsValid());
#endif

                handles.Set(id, it->second);//std::move(it->second));
                DebugLog(LogType::Debug, "NEWLY ADDED BIT FOR %s WITH ID %u\n", TypeName<T>().Data(), id.Value());
            // }

            newly_added_id_bits.Set(first_set_bit_index, false);
        }

        // NOTE: Copy instead of move is intentional
        usage_bits = other_map->usage_bits;
    }

    virtual void Reset() override
    {
        ResourceUsageMapBase::Reset();
        
        for (auto &it : handles) {
            // Use SafeRelease to defer the actual destruction of the resource.
            // This is used so that any resources that will require a mutex lock to release render side resources
            // will not cause a deadlock.
            DebugLog(LogType::Debug, "Safe releasing handle of type %s for resource ID: %u\n", TypeName<T>().Data(), it.first.Value());
            
            g_safe_deleter->SafeReleaseHandle(std::move(it.second));
        }

        handles.Clear();
    }
};

// holds a handle for any resource needed in
// rendering, so that objects like meshes and materials
// do not get destroyed while being rendered.
// rather than passing a Handle<T> around,
// you only need to use the ID.
class RenderResourceManager
{
private:
    // @TODO: Refactor to not need dynamic allocation
    FixedArray<UniquePtr<ResourceUsageMapBase>, RESOURCE_USAGE_TYPE_MAX>    m_resource_usage_maps;

public:

    RenderResourceManager();
    RenderResourceManager(const RenderResourceManager &other)               = delete;
    RenderResourceManager &operator=(const RenderResourceManager &other)    = delete;
    RenderResourceManager(RenderResourceManager &&other) noexcept           = default;
    RenderResourceManager &operator=(RenderResourceManager &&other)         = default;
    ~RenderResourceManager();

    /*! \brief Steal the tracked handles from \ref{other}. If \ref{use_soft_references} is true,
     *  we don't depend on the handles being in \ref{other}'s map of handles. Instead, we'll
     *  create handles, only for the newly added IDs (tracked via \ref{usage_bits}).
     *  \param other The other resource manager to take tracked handles from
     *  \param use_soft_references Whether or not we depend on handles being available on the other resource manager. */
    void TakeUsagesFrom(RenderResourceManager &other, bool use_soft_references = false)
    {
        for (uint32 i = 0; i < RESOURCE_USAGE_TYPE_MAX; i++) {
            m_resource_usage_maps[i]->TakeUsagesFrom(other.m_resource_usage_maps[i].Get(), use_soft_references);
        }
    }

    template <class T>
    HYP_FORCE_INLINE
    ResourceUsageMap<T> *GetResourceUsageMap() const
        { return static_cast<ResourceUsageMap<T> *>(m_resource_usage_maps[ResourceUsageTypeMap<T>::value].Get()); }

    template <class T>
    void SetIsUsed(ResourceUsageMap<T> *ptr, ID<T> id, Handle<T> &&handle, bool is_used)
    {
        if (!id.IsValid()) {
            DebugLog(
                LogType::Warn,
                "Invalid ID passed to SetIsUsed for resource type %s\n",
                TypeName<T>().Data()
            );

            return;
        }

        if (is_used != ptr->usage_bits.Test(id.ToIndex())) {
            ptr->usage_bits.Set(id.ToIndex(), is_used);

            if (is_used) {
                if (!handle.IsValid()) {
                    // Increment reference count
                    handle = Handle<T>(id);
                }

                ptr->handles.Set(id, std::move(handle));
            } else {
#ifdef HYP_DEBUG_MODE
                DebugLog(LogType::Debug, "Releasing no longer used object of type %s with ID #%u\n", TypeName<T>().Data(), id.Value());
#endif

                // Use SafeRelease to defer the actual destruction of the resource until after a few frames
                g_safe_deleter->SafeReleaseHandle(std::move(ptr->handles.Get(id)));
            }
        }

#ifdef HYP_DEBUG_MODE
        ////////// DEBUGGING //////////
        if (is_used) {
            auto it = ptr->handles.Find(id);

            AssertThrow(it != ptr->handles.End());
            AssertThrow(it->second.IsValid());
        }
#endif
    }

    template <class T>
    HYP_FORCE_INLINE
    void SetIsUsed(ResourceUsageMapBase *ptr, ID<T> id, bool is_used)
    {
        SetIsUsed<T>(static_cast<ResourceUsageMap<T> *>(ptr), id, Handle<T>(), is_used);
    }

    template <class T>
    HYP_FORCE_INLINE
    void SetIsUsed(ID<T> id, bool is_used)
    {
        SetIsUsed<T>(GetResourceUsageMap<T>(), id, Handle<T>(), is_used);
    }

    template <class T>
    HYP_FORCE_INLINE
    bool IsUsed(ID<T> id) const
    {
        if (!id.IsValid()) {
            return false;
        }

        const ResourceUsageMap<T> *ptr = GetResourceUsageMap<T>();

#if 1 //ndef HYP_DEBUG_MODE
        return ptr->usage_bits.Test(id.ToIndex());

#else
        auto it = ptr->handles.Find(id);

        if (!ptr->usage_bits.Test(id.ToIndex())) {
            AssertThrow(it == ptr->handles.End() || !it->second.IsValid());

            return false;
        }

        AssertThrow(it != ptr->handles.End());
        AssertThrow(it->second.IsValid());

        return true;
#endif
    }

    void CollectNeededResourcesForBits(ResourceUsageType type, const Bitset &bits);

    void Reset();

private:

    template <class T>
    HYP_FORCE_INLINE
    void InitResourceUsageMap()
    {
        ResourceUsageMapBase *ptr = m_resource_usage_maps[ResourceUsageTypeMap<T>::value].Get();

        if (ptr == nullptr) {
            // UniquePtr of derived class
            m_resource_usage_maps[ResourceUsageTypeMap<T>::value].Reset(new ResourceUsageMap<T>());
        }
    }
};

} // namespace hyperion

#endif