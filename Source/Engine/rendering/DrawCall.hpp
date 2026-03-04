/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Defines.hpp>

#include <Core/reflection/ObjId.hpp>
#include <Core/reflection/Handle.hpp>
#include <Core/reflection/TypeInfoFwd.hpp>

#include <rendering/GlobalBuffers.hpp>
#include <rendering/RenderMemory.hpp>
#include <rendering/RenderObject.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

class Mesh;
class Material;
class Skeleton;
class Entity;
class RenderGroup;
class RenderProxyMesh;
struct DrawCommandData;
class IndirectDrawState;
class GpuBufferHolderBase;
struct InstanceData;

HYP_API extern GpuBufferHolderMap* GetGpuBufferHolderMap();

HYP_STRUCT(NoScriptBindings)
struct EntityInstanceBatch
{
    HYP_STRUCT_BODY(EntityInstanceBatch);

    HYP_FIELD()
    uint32 batchIndex = ~0u;

    HYP_FIELD()
    uint32 numEntities = 0;

    uint32 _pad0;
    uint32 _pad1;

    HYP_FIELD()
    FixedArray<uint32, MaxEntitiesPerBatch> indices;

    HYP_FIELD()
    FixedArray<Mat4f, MaxEntitiesPerBatch> transforms;
};

HYP_STRUCT(NoScriptBindings)
struct MeshEntityInstanceBatch : EntityInstanceBatch
{
    HYP_STRUCT_BODY(MeshEntityInstanceBatch);

    HYP_FIELD()
    FixedArray<Mat4f, MaxEntitiesPerBatch> previousTransforms;
};

/*! \brief Unique identifier for a draw call based on Mesh Id and Material Id.
 *  \details This struct is used to uniquely identify a draw call in the rendering system.
 *  It combines the mesh Id and material Id into a single 64-bit value, where the lower 32 bits
 *  represent the mesh Id and the upper 32 bits represent the material Id. */
struct DrawCallID
{
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

struct DrawCallStorage
{
    using AllocatorType = RenderAllocator;

    Array<DrawCallID, AllocatorType> ids;
    Array<Mesh*, AllocatorType> meshes;
    Array<Material*, AllocatorType> materials;
    Array<Skeleton*, AllocatorType> skeletons;
    Array<uint32, AllocatorType> drawCommandIndices;
    Array<uint32, AllocatorType> numIndices;
    Array<ObjId<Entity>, AllocatorType> entityIds;

    HYP_FORCE_INLINE size_t Size() const
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

    size_t Push(DrawCallID id, Mesh* mesh, Material* material, Skeleton* skeleton, ObjId<Entity> entityId, uint32 numIndicesValue)
    {
        const size_t index = ids.Size();

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
    using AllocatorType = RenderAllocator; // Non temp allocator since we recycle the batches from the previous frame.

    Array<DrawCallID, AllocatorType> ids;
    Array<Mesh*, AllocatorType> meshes;
    Array<Material*, AllocatorType> materials;
    Array<Skeleton*, AllocatorType> skeletons;
    Array<uint32, AllocatorType> drawCommandIndices;
    Array<uint32, AllocatorType> numIndices;
    Array<EntityInstanceBatch*, AllocatorType> batches;
    Array<uint32, AllocatorType> counts;
    Array<FixedArray<ObjId<Entity>, MaxEntitiesPerBatch>, AllocatorType> entityIds;

    HYP_FORCE_INLINE size_t Size() const
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

    size_t Push(DrawCallID id, Mesh* mesh, Material* material, Skeleton* skeleton, EntityInstanceBatch* batch, uint32 numIndicesValue)
    {
        const size_t index = ids.Size();

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
    virtual ~EntityBatchAllocatorBase() = default;

    EntityBatchAllocatorBase(const EntityBatchAllocatorBase& other)
        : m_bufferHolder(other.m_bufferHolder),
          m_structSize(other.m_structSize),
          m_structAlignment(other.m_structAlignment)
    {
    }

    EntityBatchAllocatorBase(EntityBatchAllocatorBase&& other) noexcept
        : m_bufferHolder(other.m_bufferHolder),
          m_structSize(other.m_structSize),
          m_structAlignment(other.m_structAlignment)
    {
        other.m_bufferHolder = nullptr;
        other.m_structSize = 0;
        other.m_structAlignment = 0;
    }

    HYP_FORCE_INLINE size_t GetStructSize() const
    {
        return m_structSize;
    }

    HYP_FORCE_INLINE size_t GetStructAlignment() const
    {
        return m_structAlignment;
    }

    void ReleaseBatch(EntityInstanceBatch* batch) const;

    HYP_FORCE_INLINE GpuBufferHolderBase* GetGpuBufferHolder() const
    {
        return m_bufferHolder;
    }

    virtual EntityInstanceBatch* AcquireBatch() const = 0;

protected:
    explicit EntityBatchAllocatorBase(GpuBufferHolderBase* bufferHolder);

    GpuBufferHolderBase* m_bufferHolder;
    size_t m_structSize;
    size_t m_structAlignment;
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

    EntityInstanceBatch* RecycleDrawBatch(DrawCallID id);

    void ResetDrawCalls();

    /*! \brief Push \p numInstances instances of the given entity into an entity instance batch.
     *  If not all instances could be pushed to the given draw call's batch, a positive number will be returned.
     *  Otherwise, zero will be returned. */
    uint32 PushEntityToBatch(
        size_t drawCallIndex,
        Entity* entity,
        const InstanceData& instanceData,
        uint32 numInstances,
        uint32 instanceOffset);

    EntityBatchAllocatorBase* batchAllocator;

    RenderGroup* renderGroup;

    DrawCallStorage drawCalls;
    InstancedDrawCallStorage instancedDrawCalls;

    // Map from draw call id to the index in instancedDrawCalls
    using InstancedDrawCallIndexMap = HashMap<uint64, Array<size_t, InlineAllocator<3, RenderAllocator>>, NodeAllocator<RenderAllocator>>;
    InstancedDrawCallIndexMap indexMap;
};

template <class BatchType>
class TEntityBatchAllocator final : public EntityBatchAllocatorBase
{
public:
    static_assert(std::is_base_of_v<EntityInstanceBatch, BatchType>, "BatchType must be a derived struct type of EntityInstanceBatch");
    static_assert(offsetof(BatchType, indices) == 16, "offsetof for member `indices` of the derived EntityInstanceBatch type must be 16 or shader calculations will be incorrect!");

    TEntityBatchAllocator(uint32 initialCount, bool cpuAccessible)
        : EntityBatchAllocatorBase(GetGpuBufferHolderMap()->GetOrCreate<BatchType>(initialCount, cpuAccessible))
    {
    }

    TEntityBatchAllocator(const TEntityBatchAllocator& other) = default;
    TEntityBatchAllocator(TEntityBatchAllocator&& other) noexcept = default;

    ~TEntityBatchAllocator() = default;

    virtual EntityInstanceBatch* AcquireBatch() const override
    {
        BatchType* batch;
        const uint32 batchIndex = reinterpret_cast<GpuBufferHolder<BatchType, GpuBufferType::STORAGE_BUFFER>*>(m_bufferHolder)->AcquireIndex(&batch);

        batch->batchIndex = batchIndex;

        return batch;
    }
};

using PFNCreateEntityBatchAllocator = EntityBatchAllocatorBase* (*)();

EntityBatchAllocatorBase* GetEntityBatchAllocator(const TypeId& typeId);
EntityBatchAllocatorBase* GetOrCreateEntityBatchAllocator(const TypeId& typeId);

template <class T>
static inline EntityBatchAllocatorBase* GetOrCreateEntityBatchAllocator()
{
    return GetOrCreateEntityBatchAllocator(TypeId::ForType<T>());
}

// used internally
extern void RegisterEntityBatchAllocator(const TypeId& typeId, PFNCreateEntityBatchAllocator createFn);

#define HYP_REGISTER_DRAW_BATCH_TYPE(BatchType)                                                                                       \
    namespace {                                                                                                                       \
    struct BatchType##AllocatorRegistrationHelper                                                                                     \
    {                                                                                                                                 \
        BatchType##AllocatorRegistrationHelper()                                                                                      \
        {                                                                                                                             \
            RegisterEntityBatchAllocator(TypeId::ForType<BatchType>(), []() -> EntityBatchAllocatorBase*                              \
                {                                                                                                                     \
                    return PoolNew<TEntityBatchAllocator<BatchType>>(*g_renderPool, /* initialCount */ 0, /* cpuAccessible */ false); \
                });                                                                                                                   \
        }                                                                                                                             \
    };                                                                                                                                \
    static BatchType##AllocatorRegistrationHelper s_##BatchType##AllocatorRegistrationHelper;                                         \
    }

} // namespace Hyperion
