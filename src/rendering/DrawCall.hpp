/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_DRAW_CALL_HPP
#define HYPERION_DRAW_CALL_HPP

#include <core/Defines.hpp>
#include <core/Id.hpp>
#include <core/Handle.hpp>

#include <core/memory/UniquePtr.hpp>

#include <rendering/GpuBufferHolderMap.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <Types.hpp>

namespace hyperion {

class Engine;
class Mesh;
class Material;
class Skeleton;
class Entity;
class RenderGroup;
class RenderProxy;
struct DrawCommandData;
class IndirectDrawState;
class GpuBufferHolderBase;
struct MeshInstanceData;
class RenderMesh;
class RenderMaterial;
class RenderSkeleton;

extern HYP_API GpuBufferHolderMap* GetGpuBufferHolderMap();

static constexpr uint32 max_entities_per_instance_batch = 60;

struct alignas(16) EntityInstanceBatch
{
    uint32 batch_index;
    uint32 num_entities;
    uint32 _pad0;
    uint32 _pad1;

    uint32 indices[max_entities_per_instance_batch];
    Matrix4 transforms[max_entities_per_instance_batch];
};

static_assert(sizeof(EntityInstanceBatch) == 4096);

/*! \brief Unique identifier for a draw call based on Mesh Id and Material Id.
 *  \details This struct is used to uniquely identify a draw call in the rendering system.
 *  It combines the mesh Id and material Id into a single 64-bit value, where the lower 32 bits
 *  represent the mesh Id and the upper 32 bits represent the material Id. */
struct DrawCallID
{
    static constexpr uint64 mesh_mask = uint64(0xFFFFFFFF);
    static constexpr uint64 material_mask = uint64(0xFFFFFFFF) << 32;

    uint64 value;

    constexpr DrawCallID()
        : value(0)
    {
    }

    constexpr DrawCallID(Id<Mesh> mesh_id)
        : value(mesh_id.Value())
    {
    }

    constexpr DrawCallID(Id<Mesh> mesh_id, Id<Material> material_id)
        : value(uint64(mesh_id.Value()) | (uint64(material_id.Value()) << 32))
    {
    }

    HYP_FORCE_INLINE constexpr operator uint64() const
    {
        return value;
    }

    HYP_FORCE_INLINE bool operator==(const DrawCallID& other) const
    {
        return value == other.value;
    }

    HYP_FORCE_INLINE bool operator!=(const DrawCallID& other) const
    {
        return value != other.value;
    }

    HYP_FORCE_INLINE bool HasMaterial() const
    {
        return bool(value & (uint64(~0u) << 32));
    }

    HYP_FORCE_INLINE constexpr uint64 Value() const
    {
        return value;
    }
};

/*! \brief Base struct for all draw calls.
 *  \details This struct contains the common data for all draw calls, such as the Id, render mesh, material, and skeleton.
 *  It is used as a base class for both `DrawCall` and `InstancedDrawCall`. */
struct DrawCallBase
{
    DrawCallID id;

    RenderMesh* render_mesh = nullptr;
    RenderMaterial* render_material = nullptr;
    RenderSkeleton* render_skeleton = nullptr;

    uint32 draw_command_index = 0;
};

/*! \brief Represents a draw call for a single entity.
 *  \details This is used for non-instanced draw calls, where each entity has its own draw call.
 *  The `entity_id` is the Id of the entity that this draw call represents. */
struct DrawCall : DrawCallBase
{
    Id<Entity> entity_id;
};

/*! \brief Represents a draw call for multiple entities sharing the same mesh and material.
 *  \details This is used for instanced draw calls, where multiple entities share the same mesh and material.
 *  The `batch` is the entity instance batch that contains the instances of the entities. */
struct InstancedDrawCall : DrawCallBase
{
    EntityInstanceBatch* batch = nullptr;

    uint32 count = 0;
    Id<Entity> entity_ids[max_entities_per_instance_batch];
};

/// TODO: Refactor to a basic desc struct for Batch size info,
/// and use g_render_global_state to acquire batch holder / acquire and release batches.
class IDrawCallCollectionImpl
{
public:
    virtual ~IDrawCallCollectionImpl() = default;

    virtual SizeType GetBatchSizeOf() const = 0;
    virtual EntityInstanceBatch* AcquireBatch() const = 0;
    virtual void ReleaseBatch(EntityInstanceBatch* batch) const = 0;
    virtual GpuBufferHolderBase* GetEntityInstanceBatchHolder() const = 0;
};

struct DrawCallCollection
{
    DrawCallCollection()
        : impl(nullptr),
          render_group(nullptr)
    {
    }

    DrawCallCollection(IDrawCallCollectionImpl* impl, RenderGroup* render_group)
        : impl(impl),
          render_group(render_group)
    {
    }

    DrawCallCollection(const DrawCallCollection& other) = delete;
    DrawCallCollection& operator=(const DrawCallCollection& other) = delete;

    DrawCallCollection(DrawCallCollection&& other) noexcept;
    DrawCallCollection& operator=(DrawCallCollection&& other) noexcept;

    ~DrawCallCollection();

    void PushRenderProxy(DrawCallID id, const RenderProxy& render_proxy);
    void PushRenderProxyInstanced(EntityInstanceBatch* batch, DrawCallID id, const RenderProxy& render_proxy);

    EntityInstanceBatch* TakeDrawCallBatch(DrawCallID id);

    void ResetDrawCalls();

    /*! \brief Push \ref{num_instances} instances of the given entity into an entity instance batch.
     *  If not all instances could be pushed to the given draw call's batch, a positive number will be returned.
     *  Otherwise, zero will be returned. */
    uint32 PushEntityToBatch(InstancedDrawCall& draw_call, Id<Entity> entity, const MeshInstanceData& mesh_instance_data, uint32 num_instances, uint32 instance_offset);

    IDrawCallCollectionImpl* impl;

    RenderGroup* render_group;

    Array<DrawCall, InlineAllocator<16>> draw_calls;
    Array<InstancedDrawCall, InlineAllocator<16>> instanced_draw_calls;

    // Map from draw call Id to index in instanced_draw_calls
    HashMap<uint64, Array<SizeType>> index_map;
};

template <class EntityInstanceBatchType>
class DrawCallCollectionImpl final : public IDrawCallCollectionImpl
{
public:
    static_assert(std::is_base_of_v<EntityInstanceBatch, EntityInstanceBatchType>, "EntityInstanceBatchType must be a derived struct type of EntityInstanceBatch");

    DrawCallCollectionImpl()
        : m_entity_instance_batches(GetGpuBufferHolderMap()->GetOrCreate<EntityInstanceBatchType>())
    {
    }

    virtual ~DrawCallCollectionImpl() override = default;

    virtual SizeType GetBatchSizeOf() const override
    {
        return sizeof(EntityInstanceBatchType);
    }

    virtual EntityInstanceBatch* AcquireBatch() const override
    {
        EntityInstanceBatchType* batch;
        const uint32 batch_index = m_entity_instance_batches->AcquireIndex(&batch);

        batch->batch_index = batch_index;

        return batch;
    }

    virtual void ReleaseBatch(EntityInstanceBatch* batch) const override
    {
        m_entity_instance_batches->ReleaseIndex(batch->batch_index);
    }

    virtual GpuBufferHolderBase* GetEntityInstanceBatchHolder() const override
    {
        // Need to use reinterpret_cast because GpuBufferHolder is forward declared here
        return reinterpret_cast<GpuBufferHolderBase*>(m_entity_instance_batches);
    }

private:
    GpuBufferHolder<EntityInstanceBatchType, GpuBufferType::SSBO>* m_entity_instance_batches;
};

extern HYP_API IDrawCallCollectionImpl* GetDrawCallCollectionImpl(TypeId type_id);
extern HYP_API IDrawCallCollectionImpl* SetDrawCallCollectionImpl(TypeId type_id, UniquePtr<IDrawCallCollectionImpl>&& impl);

template <class EntityInstanceBatchType>
IDrawCallCollectionImpl* GetOrCreateDrawCallCollectionImpl()
{
    if (IDrawCallCollectionImpl* impl = GetDrawCallCollectionImpl(TypeId::ForType<EntityInstanceBatchType>()))
    {
        return impl;
    }

    return SetDrawCallCollectionImpl(TypeId::ForType<EntityInstanceBatchType>(), MakeUnique<DrawCallCollectionImpl<EntityInstanceBatchType>>());
}

} // namespace hyperion

#endif
