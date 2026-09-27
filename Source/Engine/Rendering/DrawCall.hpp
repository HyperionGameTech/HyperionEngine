/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Defines.hpp>

#include <Core/Types.hpp>

#include <Core/Containers/SparsePagedArray.hpp>

#include <Core/Reflection/ObjId.hpp>
#include <Core/Reflection/Handle.hpp>
#include <Core/Reflection/TypeInfoFwd.hpp>
#include <Core/Reflection/TypeInfo.hpp>

#include <Core/Utilities/IndexAllocator.hpp>
#include <Core/Utilities/Span.hpp>

#include <Rendering/RawBuffer.hpp>
#include <Rendering/RenderMemory.hpp>
#include <Rendering/RenderTypes.hpp>
#include <Rendering/RenderGroup.hpp>
#include <Rendering/Shared.hpp>

namespace Hyperion {

class Mesh;
class Material;
class Skeleton;
class Entity;
class RenderProxyList;
struct RenderProxyMesh;
struct RenderProxySprite;
struct DrawCommandData;
class IndirectDrawState;

/*! \brief The instances one instanced draw call draws.
 *  Each entry of `indices` holds the entity binding in the low 24 bits and the entry's own index in the high 8,
 *  and the entry reads its per-instance data from the instance data buffer at `instanceSlots[index]`. */
HYP_STRUCT(NoScriptBindings)
struct EntityInstanceBatch
{
    HYP_STRUCT_BODY(EntityInstanceBatch);

    HYP_FIELD()
    uint32 batchIndex = ~0u;

    HYP_FIELD()
    uint32 numEntities = 0;

    // pad, indices start at offset 64
    PadToMultiple<ubyte, 56> padding;

    HYP_FIELD()
    FixedArray<uint32, MaxInstancesPerBatch> indices;

    HYP_FIELD()
    FixedArray<uint32, MaxInstancesPerBatch> instanceSlots;
};

static_assert(offsetof(EntityInstanceBatch, indices) == 64, "offset of `indices` must match shader");
static_assert(sizeof(EntityInstanceBatch) % 64 == 0);
static_assert(MaxInstancesPerBatch <= 256, "an entry's index within its batch is stored in 8 bits");

/*! \brief Unique identifier for a draw call based on Mesh ID, Mesh LOD index and Material ID.
 *  \details This struct is used to uniquely identify a draw call in the rendering system. */
struct DrawCallID
{
    union
    {
        uint64 value;

        struct
        {
            uint32 meshIdValue : 29;
            uint32 lodIndex : 3;
            uint32 materialIdValue;
        };
    };

    static_assert(MaxMeshLods <= (1u << 3), "MaxMeshLods no longer fits in DrawCallID's lodIndex field");

    DrawCallID()
        : value(0)
    {
    }

    DrawCallID(ObjId<Mesh> meshId, uint8 lodIndex = 0)
        : meshIdValue(meshId.Value()),
          lodIndex(lodIndex),
          materialIdValue(ObjId<Material>::invalid.Value())
    {
        AssertDebug(meshId.Value() <= 0x1fffffffu);
    }

    DrawCallID(ObjId<Mesh> meshId, ObjId<Material> materialId, uint8 lodIndex = 0)
        : meshIdValue(meshId.Value()),
          lodIndex(lodIndex),
          materialIdValue(materialId.Value())
    {
        AssertDebug(meshId.Value() <= 0x1fffffffu);
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
        return materialIdValue != 0;
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
    Array<const RenderProxyMesh*, AllocatorType> meshProxies;
    Array<uint32, AllocatorType> entityBindingIndices;
    Array<uint32, AllocatorType> drawCommandIndices;

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
        meshProxies.Clear();
        entityBindingIndices.Clear();
        drawCommandIndices.Clear();
    }

    size_t Push(DrawCallID id, const RenderProxyMesh* meshProxy, uint32 entityBindingIndex)
    {
        const size_t index = ids.Size();

        ids.PushBack(id);
        meshProxies.PushBack(meshProxy);
        entityBindingIndices.PushBack(entityBindingIndex);
        drawCommandIndices.PushBack(0);

        return index;
    }
};

/*! \brief Struct of Arrays layout for instanced draw calls for better cache performance */
struct InstancedDrawCallStorage
{
    using AllocatorType = RenderAllocator; // Non temp allocator since we recycle the batches from the previous frame.

    Array<DrawCallID, AllocatorType> ids;
    Array<const RenderProxyMesh*, AllocatorType> meshProxies;
    Array<EntityInstanceBatch*, AllocatorType> batches;
    Array<uint32, AllocatorType> drawCommandIndices;
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
        meshProxies.Clear();
        batches.Clear();
        drawCommandIndices.Clear();
        counts.Clear();
    }

    size_t Push(DrawCallID id, const RenderProxyMesh* renderProxy, EntityInstanceBatch* batch)
    {
        const size_t index = ids.Size();

        ids.PushBack(id);
        meshProxies.PushBack(renderProxy);
        batches.PushBack(batch);
        drawCommandIndices.PushBack(0);
        counts.PushBack(0);

        return index;
    }
};

class EntityBatchAllocator
{
public:
    EntityBatchAllocator();

    EntityBatchAllocator(const EntityBatchAllocator& other) = delete;
    EntityBatchAllocator& operator=(const EntityBatchAllocator& other) = delete;

    EntityBatchAllocator(EntityBatchAllocator&& other) noexcept = delete;
    EntityBatchAllocator& operator=(EntityBatchAllocator&& other) noexcept = delete;

    ~EntityBatchAllocator() = default;

    HYP_FORCE_INLINE RWStructuredBuffer& GetStructuredBuffer()
    {
        return m_sbuffer;
    }

    HYP_FORCE_INLINE const RWStructuredBuffer& GetStructuredBuffer() const
    {
        return m_sbuffer;
    }

    void Initialize();
    void Shutdown();

    /// Returns nullptr when all MaxEntityInstanceBatches are in use
    EntityInstanceBatch* AcquireBatch();

    void ReleaseBatch(EntityInstanceBatch* batch);

    void MarkBatchDirty(EntityInstanceBatch* batch);

    void Flush()
    {
        if (m_sbuffer.IsDirty())
        {
            m_sbuffer.FlushBatched();
        }
    }

private:
    RWStructuredBuffer m_sbuffer;
    AtomicIndexAllocator m_indexAllocator;
};

class DrawCallCollection
{
public:
    DrawCallCollection() = default;

    DrawCallCollection(const DrawCallCollection& other) = delete;
    DrawCallCollection& operator=(const DrawCallCollection& other) = delete;

    DrawCallCollection(DrawCallCollection&& other) noexcept = default;
    DrawCallCollection& operator=(DrawCallCollection&& other) noexcept = delete;

    ~DrawCallCollection();

    void PushDrawCall(DrawCallID id, const RenderProxyMesh* renderProxy);

    /*! \brief Push one instance of \p renderProxy per entry of \p instanceSlots, into the batches of the draw call for \p id.
     *  New batches are taken from \p previous (last frame's draw calls) first, when given. */
    void PushInstances(DrawCallID id, const RenderProxyMesh* renderProxy, Span<const uint32> instanceSlots, DrawCallCollection* previous);

    /// Takes an emptied batch from one of the draw calls for \p id, so the caller can reuse it
    EntityInstanceBatch* TakeBatch(DrawCallID id);

    void ResetDrawCalls();

    void TakeDrawCalls(DrawCallCollection& out)
    {
        out.batchAllocator = batchAllocator;
        out.attributes = attributes;
        out.renderProxyList = renderProxyList;
        out.indirectRenderer = indirectRenderer;
        out.drawCalls = std::move(drawCalls);
        out.instancedDrawCalls = std::move(instancedDrawCalls);
        out.indexMap = std::move(indexMap);

        if (isInit)
        {
            out.isInit = true;
        }
    }

    EntityBatchAllocator* batchAllocator = nullptr;

    RenderableAttributeSet attributes;
    ParallelRenderingState* parallelRenderingState = nullptr;

    RenderProxyList* renderProxyList = nullptr;

    IndirectRenderer* indirectRenderer = nullptr;
    SparsePagedArray<RenderProxyMesh*, 256, RenderAllocator> meshProxies;

    DrawCallStorage drawCalls;
    InstancedDrawCallStorage instancedDrawCalls;

    // Map from draw call id to the index in instancedDrawCalls
    using InstancedDrawCallIndexMap = Map<uint64, FatArray<size_t, InlineAllocator<3, RenderAllocator>>, RenderAllocator>;
    InstancedDrawCallIndexMap indexMap;

    EnumFlags<RenderGroupFlags> flags = {};

    bool isInit = false;
    bool suppressStats = false;
};

} // namespace Hyperion
