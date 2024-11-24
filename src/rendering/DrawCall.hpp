/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_DRAW_CALL_HPP
#define HYPERION_DRAW_CALL_HPP

#include <core/Defines.hpp>
#include <core/ID.hpp>
#include <core/Handle.hpp>

#include <core/memory/UniquePtr.hpp>

#include <rendering/SafeDeleter.hpp>
#include <rendering/EntityInstanceBatchHolderMap.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <Types.hpp>

namespace hyperion {

class Engine;
class Mesh;
class Material;
class Skeleton;
struct RenderProxy;
struct DrawCommandData;
class IndirectDrawState;
class GPUBufferHolderBase;

struct MeshInstanceData;

extern HYP_API Handle<Engine>   g_engine;
extern HYP_API SafeDeleter      *g_safe_deleter;

extern HYP_API EntityInstanceBatchHolderMap *GetEntityInstanceBatchHolderMap();

static constexpr uint32 max_entities_per_instance_batch = 60;

struct alignas(16) EntityInstanceBatch
{
    uint32  num_entities;
    uint32  _pad0;
    uint32  _pad1;
    uint32  _pad2;
    uint32  indices[max_entities_per_instance_batch];
    Matrix4 transforms[max_entities_per_instance_batch];
};

static_assert(sizeof(EntityInstanceBatch) == 4096);

static constexpr uint32 max_entity_instance_batches = (128ull * 1024ull * 1024ull) / sizeof(EntityInstanceBatch);

struct DrawCallID
{
    static_assert(sizeof(ID<Mesh>) == 4, "Handle ID should be 32 bit for DrawCallID to be able to store two IDs.");

    static constexpr uint64 mesh_mask = uint64(0xFFFFFFFF);
    static constexpr uint64 material_mask = uint64(0xFFFFFFFF) << 32;

    uint64  value;

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

    HYP_FORCE_INLINE constexpr operator uint64() const
        { return value; }

    HYP_FORCE_INLINE bool operator==(const DrawCallID &other) const
        { return value == other.value; }

    HYP_FORCE_INLINE bool operator!=(const DrawCallID &other) const
        { return value != other.value; }

    HYP_FORCE_INLINE bool HasMaterial() const
        { return bool(value & (uint64(~0u) << 32)); }

    HYP_FORCE_INLINE constexpr uint64 Value() const
        { return value; }
};

struct DrawCall
{
    DrawCallID      id;
    uint32          batch_index;
    uint32          draw_command_index;

    ID<Mesh>        mesh_id;
    ID<Material>    material_id;
    ID<Skeleton>    skeleton_id;

    uint32          entity_id_count;
    ID<Entity>      entity_ids[max_entities_per_instance_batch];
};

class IDrawCallCollectionImpl
{
public:
    virtual ~IDrawCallCollectionImpl() = default;

    virtual SizeType GetBatchSizeOf() const = 0;
    virtual EntityInstanceBatch &GetBatch(SizeType index) const = 0;
    virtual uint32 GetNumBatches() const = 0;
    virtual uint32 AcquireBatchIndex() const = 0;
    virtual void ReleaseBatchIndex(uint32 batch_index) const = 0;
    virtual GPUBufferHolderBase *GetEntityInstanceBatchHolder() const = 0;
};

class DrawCallCollection
{
public:
    DrawCallCollection(IDrawCallCollectionImpl *impl)
        : m_impl(impl)
    {
    }

    DrawCallCollection(const DrawCallCollection &other)                 = delete;
    DrawCallCollection &operator=(const DrawCallCollection &other)      = delete;

    DrawCallCollection(DrawCallCollection &&other) noexcept;
    DrawCallCollection &operator=(DrawCallCollection &&other) noexcept;

    ~DrawCallCollection();

    HYP_FORCE_INLINE IDrawCallCollectionImpl *GetImpl() const
        { return m_impl; }

    HYP_FORCE_INLINE Array<DrawCall> &GetDrawCalls()
        { return m_draw_calls; }

    HYP_FORCE_INLINE const Array<DrawCall> &GetDrawCalls() const
        { return m_draw_calls; }

    void PushDrawCallToBatch(uint32 batch_index, DrawCallID id, const RenderProxy &render_proxy);
    uint32 TakeDrawCallBatchIndex(DrawCallID id);
    void ResetDrawCalls();

protected:

private:
    /*! \brief Push \ref{num_instances} instances of the given entity into an entity instance batch.
    *  If not all instances could be pushed to the given draw call's batch, a positive number will be returned.
    *  Otherwise, zero will be returned. */
    uint32 PushEntityToBatch(DrawCall &draw_call, ID<Entity> entity, const MeshInstanceData &mesh_instance_data, uint32 num_instances, uint32 instance_data_offset);

    IDrawCallCollectionImpl             *m_impl;

    Array<DrawCall>                     m_draw_calls;

    // Map from draw call ID to index in draw_calls
    HashMap<uint64, Array<SizeType>>    m_index_map;
};

template <class EntityInstanceBatchType>
class DrawCallCollectionImpl final : public IDrawCallCollectionImpl
{
public:
    static_assert(std::is_base_of_v<EntityInstanceBatch, EntityInstanceBatchType>, "EntityInstanceBatchType must be a derived struct type of EntityInstanceBatch");

    DrawCallCollectionImpl(uint32 count)
        : m_entity_instance_batches(GetEntityInstanceBatchHolderMap()->GetOrCreate<EntityInstanceBatchType>(count))
    {
    }

    virtual ~DrawCallCollectionImpl() override = default;

    virtual SizeType GetBatchSizeOf() const override
    {
        return sizeof(EntityInstanceBatchType);
    }

    virtual EntityInstanceBatch &GetBatch(SizeType index) const override
    {
        return m_entity_instance_batches->Get(index);
    }

    virtual uint32 GetNumBatches() const override
    {
        return m_entity_instance_batches->Count();
    }

    virtual uint32 AcquireBatchIndex() const override
    {
        return m_entity_instance_batches->AcquireTicket();
    }

    virtual void ReleaseBatchIndex(uint32 batch_index) const override
    {
        m_entity_instance_batches->ReleaseTicket(batch_index);
    }

    virtual GPUBufferHolderBase *GetEntityInstanceBatchHolder() const override
    {
        // Need to use reinterpret_cast because GPUBufferHolder is forward declared here
        return reinterpret_cast<GPUBufferHolderBase *>(m_entity_instance_batches);
    }

private:
    GPUBufferHolder<EntityInstanceBatchType, GPUBufferType::STORAGE_BUFFER> *m_entity_instance_batches;
};

extern HYP_API IDrawCallCollectionImpl *GetDrawCallCollectionImpl(TypeID type_id);
extern HYP_API IDrawCallCollectionImpl *SetDrawCallCollectionImpl(TypeID type_id, UniquePtr<IDrawCallCollectionImpl> &&impl);

template <class EntityInstanceBatchType>
IDrawCallCollectionImpl *GetOrCreateDrawCallCollectionImpl(uint32 count)
{
    if (IDrawCallCollectionImpl *impl = GetDrawCallCollectionImpl(TypeID::ForType<EntityInstanceBatchType>())) {
        return impl;
    }

    return SetDrawCallCollectionImpl(TypeID::ForType<EntityInstanceBatchType>(), MakeUnique<DrawCallCollectionImpl<EntityInstanceBatchType>>(count));
}

} // namespace hyperion

#endif
