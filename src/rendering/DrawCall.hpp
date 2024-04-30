/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_DRAW_CALL_HPP
#define HYPERION_DRAW_CALL_HPP

#include <core/Containers.hpp>
#include <core/ID.hpp>
#include <core/Util.hpp>
#include <core/Defines.hpp>

#include <rendering/Buffers.hpp>
#include <rendering/SafeDeleter.hpp>
#include <rendering/EntityDrawData.hpp>

#include <core/system/Debug.hpp>

#include <Types.hpp>

namespace hyperion {

using renderer::CommandBuffer;

class Engine;
class Mesh;
class Material;
class Skeleton;

extern HYP_API SafeDeleter *g_safe_deleter;

struct DrawCommandData;
class IndirectDrawState;

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

    virtual void TakeUsagesFrom(ResourceUsageMapBase *other) = 0;
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
            DebugLog(LogType::Debug, "Safe releasing handle of type %s for resource ID: %u", TypeName<T>().Data(), it.first.Value());
            
            g_safe_deleter->SafeReleaseHandle(std::move(it.second));
        }
    }

    virtual void TakeUsagesFrom(ResourceUsageMapBase *other) override
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
            const auto it = handles.Find(ID<T>::FromIndex(first_set_bit_index));

#ifdef HYP_DEBUG_MODE
            AssertThrow(it != handles.End());
            AssertThrow(it->second.IsValid());
#endif
            // Use SafeRelease to defer the actual destruction of the resource.
            // This is used so that any resources that will require a mutex lock to release render side resources
            // will not cause a deadlock.
            g_safe_deleter->SafeReleaseHandle(std::move(it->second));

            removed_id_bits.Set(first_set_bit_index, false);
        }

        Bitset newly_added_id_bits = other_map->usage_bits & ~usage_bits;

        while ((first_set_bit_index = newly_added_id_bits.FirstSetBitIndex()) != -1) {
            // Move the handle from the other map to this map
            const auto it = other_map->handles.Find(ID<T>::FromIndex(first_set_bit_index));

#ifdef HYP_DEBUG_MODE
            AssertThrow(it != other_map->handles.End());
            AssertThrow(it->second.IsValid());
#endif

            handles.Set(it->first, std::move(it->second));

            newly_added_id_bits.Set(first_set_bit_index, false);
        }

        usage_bits = std::move(other_map->usage_bits);
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
    FixedArray<UniquePtr<ResourceUsageMapBase>, RESOURCE_USAGE_TYPE_MAX>    m_resource_usage_maps;

public:

    RenderResourceManager()                                                 = default;
    RenderResourceManager(const RenderResourceManager &other)               = delete;
    RenderResourceManager &operator=(const RenderResourceManager &other)    = delete;
    RenderResourceManager(RenderResourceManager &&other) noexcept           = default;
    RenderResourceManager &operator=(RenderResourceManager &&other)         = default;
    ~RenderResourceManager()                                                = default;

    void TakeUsagesFrom(RenderResourceManager &other)
    {
        if (&other == this) {
            return;
        }

        for (uint i = 0; i < RESOURCE_USAGE_TYPE_MAX; i++) {
            AssertThrow(m_resource_usage_maps[i] == nullptr);
            AssertThrow(other.m_resource_usage_maps[i] != nullptr);

            m_resource_usage_maps[i]->TakeUsagesFrom(other.m_resource_usage_maps[i].Get());
        }
    }

    template <class T>
    HYP_FORCE_INLINE
    ResourceUsageMap<T> *GetResourceUsageMap()
    {
        ResourceUsageMapBase *ptr = m_resource_usage_maps[ResourceUsageTypeMap<T>::value].Get();

        if (ptr == nullptr) {
            // UniquePtr of derived class
            m_resource_usage_maps[ResourceUsageTypeMap<T>::value].Reset(new ResourceUsageMap<T>());

            ptr = m_resource_usage_maps[ResourceUsageTypeMap<T>::value].Get();
        }

        return static_cast<ResourceUsageMap<T> *>(ptr);
    }

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
    }

    template <class T>
    HYP_FORCE_INLINE
    void SetIsUsed(ResourceUsageMapBase *ptr, ID<T> id, bool is_used)
    {
        SetIsUsed<T>(static_cast<ResourceUsageMap<T> *>(ptr), id, Handle<T>(), is_used);
    }

    template <class T>
    HYP_FORCE_INLINE
    bool IsUsed(ID<T> id) const
    {
        if (!id.IsValid()) {
            return false;
        }

        const ResourceUsageMap<T> *ptr = static_cast<const ResourceUsageMap<T> *>(m_resource_usage_maps[ResourceUsageTypeMap<T>::value].Get());

        if (ptr == nullptr) {
            return false;
        }

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
};

struct DrawCallID
{
    static_assert(sizeof(ID<Mesh>) == 4, "Handle ID should be 32 bit for DrawCallID to be able to store two IDs.");

    uint64 value;

    DrawCallID()
        : value(0)
    {
    }

    DrawCallID(ID<Mesh> mesh_id)
        : value(mesh_id.Value())
    {
    }

    DrawCallID(ID<Mesh> mesh_id, ID<Material> material_id)
        : value(mesh_id.Value() | (uint64(material_id.Value()) << 32))
    {
    }

    bool operator==(const DrawCallID &other) const
        { return value == other.value; }

    bool operator!=(const DrawCallID &other) const
        { return value != other.value; }

    bool HasMaterial() const
        { return bool((uint64(~0u) << 32) & value); }

    operator uint64() const
        { return value; }

    uint64 Value() const
        { return value; }
};

struct DrawCall
{
#ifdef HYP_USE_INDEXED_ARRAY_FOR_OBJECT_DATA
    static constexpr bool unique_per_material = false;
#else
    static constexpr bool unique_per_material = true;
#endif

    DrawCallID                          id;
    BufferTicket<EntityInstanceBatch>   batch_index;
    uint32                              draw_command_index;

    ID<Mesh>                            mesh_id;
    ID<Material>                        material_id;
    ID<Skeleton>                        skeleton_id;

    uint32                              entity_id_count;
    ID<Entity>                          entity_ids[max_entities_per_instance_batch];
};

struct DrawCallCollection
{
    Array<DrawCall>                     draw_calls;

    // Map from draw call ID to index in draw_calls
    HashMap<uint64, Array<SizeType>>    index_map;

    DrawCallCollection() = default;
    DrawCallCollection(const DrawCallCollection &other)             = delete;
    DrawCallCollection &operator=(const DrawCallCollection &other)  = delete;

    DrawCallCollection(DrawCallCollection &&other) noexcept;
    DrawCallCollection &operator=(DrawCallCollection &&other) noexcept;

    ~DrawCallCollection();

    void PushDrawCall(BufferTicket<EntityInstanceBatch> batch_index, DrawCallID id, EntityDrawData entity_draw_data);
    DrawCall *TakeDrawCall(DrawCallID id);
    void Reset();
};

} // namespace hyperion

#endif
