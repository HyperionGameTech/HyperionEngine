/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_DRAW_CALL_HPP
#define HYPERION_DRAW_CALL_HPP

#include <core/ID.hpp>
#include <core/Defines.hpp>

#include <rendering/Buffers.hpp>
#include <rendering/SafeDeleter.hpp>
#include <rendering/RenderProxy.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <Types.hpp>

namespace hyperion {

class Engine;
class Mesh;
class Material;
class Skeleton;

extern HYP_API SafeDeleter *g_safe_deleter;

struct DrawCommandData;
class IndirectDrawState;

struct DrawCallID
{
    static_assert(sizeof(ID<Mesh>) == 4, "Handle ID should be 32 bit for DrawCallID to be able to store two IDs.");

    using ValueType = uint64;

    ValueType value;

    static constexpr uint64 mesh_mask = uint64(0xFFFFFFFF);
    static constexpr uint64 material_mask = uint64(0xFFFFFFFF) << 32;

    DrawCallID()
        : value(0)
    {
    }

    DrawCallID(ID<Mesh> mesh_id)
        : value(mesh_id.Value())
    {
    }

    DrawCallID(ID<Mesh> mesh_id, ID<Material> material_id)
        : value(uint64(mesh_id.Value()) | (uint64(material_id.Value()) << 32))
    {
    }

    HYP_FORCE_INLINE bool operator==(const DrawCallID &other) const
        { return value == other.value; }

    HYP_FORCE_INLINE bool operator!=(const DrawCallID &other) const
        { return value != other.value; }

    HYP_FORCE_INLINE bool HasMaterial() const
        { return bool(value & (uint64(~0u) << 32)); }

    HYP_FORCE_INLINE ValueType Value() const
        { return value; }
};

struct DrawCall
{
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
    Array<DrawCall>                                 draw_calls;

    // Map from draw call ID to index in draw_calls
    HashMap<DrawCallID::ValueType, Array<SizeType>> index_map;

    DrawCallCollection() = default;
    DrawCallCollection(const DrawCallCollection &other)                 = delete;
    DrawCallCollection &operator=(const DrawCallCollection &other)      = delete;

    DrawCallCollection(DrawCallCollection &&other) noexcept;
    DrawCallCollection &operator=(DrawCallCollection &&other) noexcept  = delete;

    ~DrawCallCollection();

    void PushDrawCallToBatch(BufferTicket<EntityInstanceBatch> batch_index, DrawCallID id, const RenderProxy &render_proxy);
    DrawCall *TakeDrawCall(DrawCallID id);
    void ResetDrawCalls();
};

} // namespace hyperion

#endif
