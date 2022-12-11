#ifndef HYPERION_V2_DRAW_CALL_HPP
#define HYPERION_V2_DRAW_CALL_HPP

#include <core/Containers.hpp>
#include <Constants.hpp>
#include <core/ID.hpp>
#include <core/lib/AtomicSemaphore.hpp>
#include <rendering/Buffers.hpp>
#include <util/Defines.hpp>
#include <Types.hpp>

#include <rendering/ShaderGlobals.hpp>

#include <unordered_map>

namespace hyperion::v2 {

using renderer::CommandBuffer;
using renderer::GPUBuffer;

class Engine;
class Mesh;
class Material;
class Skeleton;

struct DrawCommandData;
class IndirectDrawState;

struct DrawCallID
{
    static_assert(sizeof(ID<Mesh>) == 4, "Handle ID should be 32 bit for DrawCallID to be able to store two IDs.");

    UInt64 value;

    DrawCallID()
        : value(0)
    {
    }

    DrawCallID(ID<Mesh> mesh_id)
        : value(mesh_id.Value())
    {
    }

    DrawCallID(ID<Mesh> mesh_id, ID<Material> material_id)
        : value(mesh_id.Value() | (UInt64(material_id.Value()) << 32))
    {
    }

    bool operator==(const DrawCallID &other) const
        { return value == other.value; }

    bool operator!=(const DrawCallID &other) const
        { return value != other.value; }

    bool HasMaterial() const
        { return bool((UInt64(~0u) << 32) & value); }

    operator UInt64() const
        { return value; }

    UInt64 Value() const
        { return value; }
};

struct DrawCall
{
    static constexpr bool unique_per_material = !use_indexed_array_for_object_data;

    DrawCallID id;
    EntityBatchIndex batch_index;
    SizeType draw_command_index;

    ID<Material> material_id;
    ID<Skeleton> skeleton_id;

    UInt entity_id_count;
    ID<Entity> entity_ids[max_entities_per_instance_batch];

    UInt packed_data[4];

    Handle<Mesh> mesh;
    //Mesh *mesh;
};

struct DrawCallCollection
{
    Array<DrawCall> draw_calls;
    std::unordered_map<UInt64 /* DrawCallID */, Array<SizeType>> index_map;

    DrawCallCollection() = default;
    DrawCallCollection(const DrawCallCollection &other) = delete;
    DrawCallCollection &operator=(const DrawCallCollection &other) = delete;

    DrawCallCollection(DrawCallCollection &&other) noexcept;
    DrawCallCollection &operator=(DrawCallCollection &&other) noexcept;

    ~DrawCallCollection();

    void PushDrawCall(EntityBatchIndex batch_index, DrawCallID id, const EntityDrawProxy &entity);
    DrawCall *TakeDrawCall(DrawCallID id);
    void Reset();
};

} // namespace hyperion::v2

#endif
