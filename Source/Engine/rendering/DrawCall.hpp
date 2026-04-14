/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/reflection/ObjId.hpp>
#include <Core/reflection/Handle.hpp>
#include <Core/reflection/TypeInfoFwd.hpp>
#include <Core/reflection/TypeInfo.hpp>

#include <Core/utilities/IdGenerator.hpp>

#include <rendering/StructuredBuffer.hpp>
#include <rendering/RenderMemory.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/RenderGroup.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

class Mesh;
class Material;
class Skeleton;
class Entity;
class RenderProxyMesh;
struct DrawCommandData;
class IndirectDrawState;
struct InstanceData;

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

    // pad 48 bytes so sizeof % 64 == 0
    HYP_PAD_STRUCT_HERE(48);

    HYP_FIELD()
    FixedArray<uint32, MaxEntitiesPerBatch> indices;

    HYP_FIELD()
    FixedArray<Mat4f, MaxEntitiesPerBatch> transforms;
};

static_assert(sizeof(EntityInstanceBatch) % 64 == 0);

HYP_STRUCT(NoScriptBindings)
struct MeshEntityInstanceBatch : EntityInstanceBatch
{
    HYP_STRUCT_BODY(MeshEntityInstanceBatch);

    HYP_FIELD()
    FixedArray<Mat4f, MaxEntitiesPerBatch> previousTransforms;
};

static_assert(sizeof(MeshEntityInstanceBatch) % 64 == 0);

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
    Array<uint32, AllocatorType> entityBindingIndices;

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
        entityBindingIndices.Clear();
    }

    size_t Push(DrawCallID id, Mesh* mesh, Material* material, Skeleton* skeleton, uint32 entityBindingIndex, uint32 numIndicesValue)
    {
        const size_t index = ids.Size();

        ids.PushBack(id);
        meshes.PushBack(mesh);
        materials.PushBack(material);
        skeletons.PushBack(skeleton);
        drawCommandIndices.PushBack(~0u);
        numIndices.PushBack(numIndicesValue);
        entityBindingIndices.PushBack(entityBindingIndex);

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

        return index;
    }
};

class EntityBatchAllocatorBase
{
public:
    virtual ~EntityBatchAllocatorBase() = default;

    EntityBatchAllocatorBase(const EntityBatchAllocatorBase& other) = delete;
    EntityBatchAllocatorBase& operator=(const EntityBatchAllocatorBase& other) = delete;

    EntityBatchAllocatorBase(EntityBatchAllocatorBase&& other) noexcept = delete;
    EntityBatchAllocatorBase& operator=(EntityBatchAllocatorBase&& other) noexcept = delete;

    HYP_FORCE_INLINE size_t GetStructSize() const
    {
        return m_structSize;
    }

    HYP_FORCE_INLINE size_t GetStructAlignment() const
    {
        return m_structAlignment;
    }

    HYP_FORCE_INLINE StructuredBuffer& GetStructuredBuffer()
    {
        return m_sbuffer;
    }

    HYP_FORCE_INLINE const StructuredBuffer& GetStructuredBuffer() const
    {
        return m_sbuffer;
    }

    void Initialize();

    void ReleaseBatch(EntityInstanceBatch* batch);

    void MarkBatchDirty(EntityInstanceBatch* batch);

    void Flush()
    {
        if (m_sbuffer.IsDirty())
        {
            m_sbuffer.Flush();
        }
    }

    virtual EntityInstanceBatch* AcquireBatch() = 0;

protected:
    explicit EntityBatchAllocatorBase(const TypeInfo* structTypeInfo, uint32 maxBatches);

    StructuredBuffer m_sbuffer;
    mutable IdGenerator m_idGenerator;
    size_t m_structSize;
    size_t m_structAlignment;
};

struct DrawCallCollection
{
    DrawCallCollection()
        : batchAllocator(nullptr)
    {
    }

    DrawCallCollection(const DrawCallCollection& other) = delete;
    DrawCallCollection& operator=(const DrawCallCollection& other) = delete;

    DrawCallCollection(DrawCallCollection&& other) noexcept = default;
    DrawCallCollection& operator=(DrawCallCollection&& other) noexcept = delete;

    ~DrawCallCollection();

    HYP_FORCE_INLINE bool IsValid() const
    {
        return batchAllocator != nullptr && renderGroup.valid;
    }

    void PushDrawCall(DrawCallID id, const RenderProxyMesh& renderProxy);
    void PushInstancedDrawCall(EntityInstanceBatch* batch, DrawCallID id, const RenderProxyMesh& renderProxy);

    EntityInstanceBatch* RecycleDrawBatch(DrawCallID id);

    void ResetDrawCalls();

    /*! \brief Push \p numInstances instances of the given entity into an entity instance batch.
     *  If not all instances could be pushed to the given draw call's batch, a positive number will be returned.
     *  Otherwise, zero will be returned. */
    HYP_NODISCARD uint32 PushEntityToBatch(
        size_t drawCallIndex,
        Entity* entity,
        const InstanceData& instanceData,
        uint32 numInstances,
        uint32 instanceOffset);

    void TakeDrawCalls(DrawCallCollection& out)
    {
        out.batchAllocator = batchAllocator;
        out.renderGroup = renderGroup;
        out.indirectRenderer = indirectRenderer;
        out.drawCalls = std::move(drawCalls);
        out.instancedDrawCalls = std::move(instancedDrawCalls);
        out.indexMap = std::move(indexMap);
    }

    EntityBatchAllocatorBase* batchAllocator;

    RenderGroup renderGroup;

    // map entity id to mesh proxy
    IndirectRenderer* indirectRenderer = nullptr;
    SparsePagedArray<RenderProxyMesh*, 128, RenderAllocator> meshProxies;

    DrawCallStorage drawCalls;
    InstancedDrawCallStorage instancedDrawCalls;

    // Map from draw call id to the index in instancedDrawCalls
    using InstancedDrawCallIndexMap = HashMap<uint64, Array<size_t, InlineAllocator<3, RenderAllocator>>, RenderAllocator>;
    InstancedDrawCallIndexMap indexMap;
};

template <class BatchType>
class TEntityBatchAllocator final : public EntityBatchAllocatorBase
{
public:
    static_assert(std::is_base_of_v<EntityInstanceBatch, BatchType>, "BatchType must be a derived struct type of EntityInstanceBatch");
    static_assert(offsetof(BatchType, indices) == 64, "offsetof for member `indices` of the derived EntityInstanceBatch type must match shader");

    TEntityBatchAllocator()
        : EntityBatchAllocatorBase(&TypeOf<BatchType>(), MaxEntityInstanceBatches)
    {
    }

    TEntityBatchAllocator(const TEntityBatchAllocator& other) = delete;
    TEntityBatchAllocator(TEntityBatchAllocator&& other) noexcept = delete;

    ~TEntityBatchAllocator() = default;

    virtual EntityInstanceBatch* AcquireBatch() override
    {
        const uint32 batchIndex = m_idGenerator.Next() - 1;

        AssertDebug(batchIndex < MaxEntityInstanceBatches,
            "Entity instance batch limit ({}) exceeded! Consider increasing MaxEntityInstanceBatches.", MaxEntityInstanceBatches);

        BatchType* batch = reinterpret_cast<BatchType*>(m_sbuffer.cpuBuffer.Data() + batchIndex * m_structSize);
        batch->batchIndex = batchIndex;
        batch->numEntities = 0;

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

HYP_API const HashMap<TypeId, EntityBatchAllocatorBase*>& GetAllEntityBatchAllocators();

#define HYP_REGISTER_DRAW_BATCH_TYPE(BatchType)                                                                                             \
    namespace {                                                                                                                             \
    struct BatchType##AllocatorRegistrationHelper                                                                                           \
    {                                                                                                                                       \
        BatchType##AllocatorRegistrationHelper()                                                                                            \
        {                                                                                                                                   \
            RegisterEntityBatchAllocator(TypeId::ForType<BatchType>(), []() -> EntityBatchAllocatorBase*                                    \
                {                                                                                                                           \
                    return HYP_POOL_NEW(g_renderPool, TEntityBatchAllocator<BatchType>);                                                    \
                });                                                                                                                         \
        }                                                                                                                                   \
    };                                                                                                                                      \
    static BatchType##AllocatorRegistrationHelper s_##BatchType##AllocatorRegistrationHelper;                                               \
    }

} // namespace Hyperion
