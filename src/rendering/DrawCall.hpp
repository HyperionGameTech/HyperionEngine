/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>
#include <core/reflection/ObjId.hpp>
#include <core/reflection/Handle.hpp>

#include <core/memory/UniquePtr.hpp>

#include <rendering/GpuBufferHolderMap.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/RenderMemory.hpp>
#include <rendering/RenderObject.hpp>

#include <core/Types.hpp>

namespace hyperion {

class Mesh;
class Material;
class Skeleton;
class Entity;
class RenderGroup;
class RenderProxyMesh;
struct DrawCommandData;
class IndirectDrawState;
class GpuBufferHolderBase;
struct MeshInstanceData;

HYP_API extern GpuBufferHolderMap* GetGpuBufferHolderMap();

static constexpr uint32 MaxEntitiesPerBatch = 60;

struct alignas(16) EntityInstanceBatch
{
    uint32 batchIndex;
    uint32 numEntities;
    uint32 _pad0;
    uint32 _pad1;

    uint32 indices[MaxEntitiesPerBatch];
    Mat4f transforms[MaxEntitiesPerBatch];
};

static_assert(sizeof(EntityInstanceBatch) == 4096);

/*! \brief Unique identifier for a draw call based on Mesh Id and Material Id.
 *  \details This struct is used to uniquely identify a draw call in the rendering system.
 *  It combines the mesh Id and material Id into a single 64-bit value, where the lower 32 bits
 *  represent the mesh Id and the upper 32 bits represent the material Id. */
struct DrawCallID
{
    static constexpr uint64 meshMask = uint64(0xFFFFFFFF);
    static constexpr uint64 materialMask = uint64(0xFFFFFFFF) << 32;

    uint64 value;

    constexpr DrawCallID()
        : value(0)
    {
    }

    constexpr DrawCallID(ObjId<Mesh> meshId)
        : value(meshId.Value())
    {
    }

    constexpr DrawCallID(ObjId<Mesh> meshId, ObjId<Material> materialId)
        : value(uint64(meshId.Value()) | (uint64(materialId.Value()) << 32))
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

/*! \brief Struct of Arrays layout for non-instanced draw calls for better cache performance */
struct DrawCallStorage
{
    Array<DrawCallID, RenderAllocator> ids;
    Array<Mesh*, RenderAllocator> meshes;
    Array<Material*, RenderAllocator> materials;
    Array<Skeleton*, RenderAllocator> skeletons;
    Array<uint32, RenderAllocator> drawCommandIndices;
    Array<uint32, RenderAllocator> numIndices;
    Array<ObjId<Entity>, RenderAllocator> entityIds;

    HYP_FORCE_INLINE SizeType Size() const
    {
        return ids.Size();
    }

    HYP_FORCE_INLINE bool Empty() const
    {
        return ids.Empty();
    }

    HYP_FORCE_INLINE bool Any() const
    {
        return ids.Any();
    }

    void Clear()
    {
        ids.Clear();
        meshes.Clear();
        materials.Clear();
        skeletons.Clear();
        drawCommandIndices.Clear();
        numIndices.Clear();
        entityIds.Clear();
    }

    SizeType Push(DrawCallID id, Mesh* mesh, Material* material, Skeleton* skeleton, ObjId<Entity> entityId, uint32 numIndicesValue)
    {
        const SizeType index = ids.Size();

        ids.PushBack(id);
        meshes.PushBack(mesh);
        materials.PushBack(material);
        skeletons.PushBack(skeleton);
        drawCommandIndices.PushBack(~0u);
        numIndices.PushBack(numIndicesValue);
        entityIds.PushBack(entityId);

        return index;
    }
};

/*! \brief Struct of Arrays layout for instanced draw calls for better cache performance */
struct InstancedDrawCallStorage
{
    Array<DrawCallID, RenderAllocator> ids;
    Array<Mesh*, RenderAllocator> meshes;
    Array<Material*, RenderAllocator> materials;
    Array<Skeleton*, RenderAllocator> skeletons;
    Array<uint32, RenderAllocator> drawCommandIndices;
    Array<uint32, RenderAllocator> numIndices;
    Array<EntityInstanceBatch*, RenderAllocator> batches;
    Array<uint32, RenderAllocator> counts;
    Array<FixedArray<ObjId<Entity>, MaxEntitiesPerBatch>, RenderAllocator> entityIds;

    HYP_FORCE_INLINE SizeType Size() const
    {
        return ids.Size();
    }

    HYP_FORCE_INLINE bool Empty() const
    {
        return ids.Empty();
    }

    HYP_FORCE_INLINE bool Any() const
    {
        return ids.Any();
    }

    void Clear()
    {
        ids.Clear();
        meshes.Clear();
        materials.Clear();
        skeletons.Clear();
        drawCommandIndices.Clear();
        numIndices.Clear();
        batches.Clear();
        counts.Clear();
        entityIds.Clear();
    }

    SizeType Push(DrawCallID id, Mesh* mesh, Material* material, Skeleton* skeleton, EntityInstanceBatch* batch, uint32 numIndicesValue)
    {
        const SizeType index = ids.Size();

        ids.PushBack(id);
        meshes.PushBack(mesh);
        materials.PushBack(material);
        skeletons.PushBack(skeleton);
        drawCommandIndices.PushBack(~0u);
        numIndices.PushBack(numIndicesValue);
        batches.PushBack(batch);
        counts.PushBack(0);
        entityIds.EmplaceBack();

        return index;
    }
};

class EntityBatchAllocatorBase
{
public:
    ~EntityBatchAllocatorBase() = default;

    HYP_FORCE_INLINE SizeType GetStructSize() const
    {
        return m_structSize;
    }

    HYP_FORCE_INLINE SizeType GetStructAlignment() const
    {
        return m_structAlignment;
    }

    HYP_FORCE_INLINE void ReleaseBatch(EntityInstanceBatch* batch) const
    {
        m_bufferHolder->ReleaseIndex(batch->batchIndex);
    }

    HYP_FORCE_INLINE GpuBufferHolderBase* GetGpuBufferHolder() const
    {
        return m_bufferHolder;
    }

    virtual EntityInstanceBatch* AcquireBatch() const = 0;

protected:
    explicit EntityBatchAllocatorBase(GpuBufferHolderBase* bufferHolder);

    GpuBufferHolderBase* m_bufferHolder;
    SizeType m_structSize;
    SizeType m_structAlignment;
};

struct DrawCallCollection
{
    DrawCallCollection()
        : batchAllocator(nullptr),
          renderGroup(nullptr)
    {
    }

    DrawCallCollection(EntityBatchAllocatorBase* batchAllocator, RenderGroup* renderGroup)
        : batchAllocator(batchAllocator),
          renderGroup(renderGroup)
    {
    }

    DrawCallCollection(const DrawCallCollection& other) = delete;
    DrawCallCollection& operator=(const DrawCallCollection& other) = delete;

    DrawCallCollection(DrawCallCollection&& other) noexcept;
    DrawCallCollection& operator=(DrawCallCollection&& other) noexcept;

    ~DrawCallCollection();

    HYP_FORCE_INLINE bool IsValid() const
    {
        return batchAllocator != nullptr;
    }

    void PushRenderProxy(DrawCallID id, const RenderProxyMesh& renderProxy);
    void PushRenderProxyInstanced(EntityInstanceBatch* batch, DrawCallID id, const RenderProxyMesh& renderProxy);

    EntityInstanceBatch* TakeDrawCallBatch(DrawCallID id);

    void ResetDrawCalls();

    /*! \brief Push \ref{numInstances} instances of the given entity into an entity instance batch.
     *  If not all instances could be pushed to the given draw call's batch, a positive number will be returned.
     *  Otherwise, zero will be returned. */
    uint32 PushEntityToBatch(SizeType drawCallIndex, Entity* entity, const MeshInstanceData& meshInstanceData, uint32 numInstances, uint32 instanceOffset);

    EntityBatchAllocatorBase* batchAllocator;

    RenderGroup* renderGroup;

    DrawCallStorage drawCalls;
    InstancedDrawCallStorage instancedDrawCalls;

    // Map from draw call id to the index in instancedDrawCalls
    using InstancedDrawCallIndexMap = HashMap<uint64, Array<SizeType, InlineAllocator<3, RenderAllocator>>, NodeAllocator<RenderAllocator>>;
    InstancedDrawCallIndexMap indexMap;
};

template <class EntityInstanceBatchType>
class TEntityBatchAllocator final : public EntityBatchAllocatorBase
{
public:
    static_assert(std::is_base_of_v<EntityInstanceBatch, EntityInstanceBatchType>, "EntityInstanceBatchType must be a derived struct type of EntityInstanceBatch");
    static_assert(offsetof(EntityInstanceBatchType, indices) == 16, "offsetof for member `indices` of the derived EntityInstanceBatch type must be 16 or shader calculations will be incorrect!");

    TEntityBatchAllocator()
        : EntityBatchAllocatorBase(GetGpuBufferHolderMap()->GetOrCreate<EntityInstanceBatchType>())
    {
    }

    ~TEntityBatchAllocator() = default;

    virtual EntityInstanceBatch* AcquireBatch() const override
    {
        EntityInstanceBatchType* batch;
        const uint32 batchIndex = reinterpret_cast<GpuBufferHolder<EntityInstanceBatchType, GpuBufferType::SSBO>*>(m_bufferHolder)->AcquireIndex(&batch);

        batch->batchIndex = batchIndex;

        return batch;
    }
};

HYP_API extern EntityBatchAllocatorBase* GetEntityBatchAllocator(TypeId typeId);
HYP_API extern EntityBatchAllocatorBase* SetEntityBatchAllocator(TypeId typeId, UniquePtr<EntityBatchAllocatorBase>&& impl);

template <class EntityInstanceBatchType>
EntityBatchAllocatorBase* GetOrCreateEntityBatchAllocator()
{
    if (EntityBatchAllocatorBase* batchAllocator = GetEntityBatchAllocator(TypeId::ForType<EntityInstanceBatchType>()))
    {
        return batchAllocator;
    }

    return SetEntityBatchAllocator(TypeId::ForType<EntityInstanceBatchType>(), MakeUnique<TEntityBatchAllocator<EntityInstanceBatchType>>());
}

} // namespace hyperion
