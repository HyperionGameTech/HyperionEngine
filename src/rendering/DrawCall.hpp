/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>
#include <core/object/ObjId.hpp>
#include <core/object/Handle.hpp>

#include <core/memory/UniquePtr.hpp>

#include <rendering/GpuBufferHolderMap.hpp>
#include <rendering/Buffers.hpp>

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

extern HYP_API GpuBufferHolderMap* GetGpuBufferHolderMap();

static constexpr uint32 maxEntitiesPerInstanceBatch = 60;

struct alignas(16) EntityInstanceBatch
{
    uint32 batchIndex;
    uint32 numEntities;
    uint32 _pad0;
    uint32 _pad1;

    uint32 indices[maxEntitiesPerInstanceBatch];
    Matrix4 transforms[maxEntitiesPerInstanceBatch];
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

/*! \brief Base struct for all draw calls */
struct DrawCallBase
{
    DrawCallID id;

    Mesh* mesh = nullptr;
    Material* material = nullptr;
    Skeleton* skeleton = nullptr;

    uint32 drawCommandIndex = 0;
};

/*! \brief Non-instanced draw call for a single entity  */
struct DrawCall : DrawCallBase
{
    ObjId<Entity> entityId;
};

/*! \brief A draw call for multiple entities sharing the same mesh and material */
struct InstancedDrawCall : DrawCallBase
{
    EntityInstanceBatch* batch = nullptr;

    uint32 count = 0;
    ObjId<Entity> entityIds[maxEntitiesPerInstanceBatch];
};

/// TODO: Refactor to a basic desc struct for Batch size info,
/// and use g_renderGlobalState to acquire batch holder / acquire and release batches.
class IDrawCallCollectionImpl
{
public:
    ~IDrawCallCollectionImpl() = default;

    HYP_FORCE_INLINE SizeType GetStructSize() const
    {
        return m_bufferHolder->GetStructSize();
    }

    HYP_FORCE_INLINE SizeType GetStructAlignment() const
    {
        return m_bufferHolder->GetStructSize();
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
    IDrawCallCollectionImpl(GpuBufferHolderBase* bufferHolder)
        : m_bufferHolder(bufferHolder)
    {
    }

    GpuBufferHolderBase* m_bufferHolder;
};

struct DrawCallCollection
{
    DrawCallCollection()
        : impl(nullptr),
          renderGroup(nullptr)
    {
    }

    DrawCallCollection(IDrawCallCollectionImpl* impl, RenderGroup* renderGroup)
        : impl(impl),
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
        return impl != nullptr;
    }

    void PushRenderProxy(DrawCallID id, const RenderProxyMesh& renderProxy);
    void PushRenderProxyInstanced(EntityInstanceBatch* batch, DrawCallID id, const RenderProxyMesh& renderProxy);

    EntityInstanceBatch* TakeDrawCallBatch(DrawCallID id);

    void ResetDrawCalls();

    /*! \brief Push \ref{numInstances} instances of the given entity into an entity instance batch.
     *  If not all instances could be pushed to the given draw call's batch, a positive number will be returned.
     *  Otherwise, zero will be returned. */
    uint32 PushEntityToBatch(InstancedDrawCall& drawCall, ObjId<Entity> entity, const MeshInstanceData& meshInstanceData, uint32 numInstances, uint32 instanceOffset);

    IDrawCallCollectionImpl* impl;

    RenderGroup* renderGroup;

    Array<DrawCall> drawCalls;
    Array<InstancedDrawCall> instancedDrawCalls;

    // Map from draw call id to the index in instancedDrawCalls
    HashMap<uint64, Array<SizeType>> instancedDrawCallIndexMap;
};

template <class EntityInstanceBatchType>
class DrawCallCollectionImpl final : public IDrawCallCollectionImpl
{
public:
    static_assert(std::is_base_of_v<EntityInstanceBatch, EntityInstanceBatchType>, "EntityInstanceBatchType must be a derived struct type of EntityInstanceBatch");
    static_assert(offsetof(EntityInstanceBatchType, indices) == 16, "offsetof for member `indices` of the derived EntityInstanceBatch type must be 16 or shader calculations will be incorrect!");

    DrawCallCollectionImpl()
        : IDrawCallCollectionImpl(GetGpuBufferHolderMap()->GetOrCreate<EntityInstanceBatchType>())
    {
    }

    ~DrawCallCollectionImpl() = default;

    virtual EntityInstanceBatch* AcquireBatch() const override
    {
        EntityInstanceBatchType* batch;
        const uint32 batchIndex = reinterpret_cast<GpuBufferHolder<EntityInstanceBatchType, GpuBufferType::SSBO>*>(m_bufferHolder)->AcquireIndex(&batch);

        batch->batchIndex = batchIndex;

        return batch;
    }
};

namespace memory {

template <class T>
struct MemoryPoolInitInfo<T, std::enable_if_t<std::is_base_of_v<EntityInstanceBatch, T>>>
{
    static constexpr uint32 numBytesPerBlock = MathUtil::NextPowerOf2(MathUtil::Max(sizeof(T), 1024 * 1024)); // 1MB minimum block size
    static constexpr uint32 numElementsPerBlock = numBytesPerBlock / sizeof(T);
    static constexpr uint32 numInitialElements = numElementsPerBlock;
};

} // namespace memory

extern HYP_API IDrawCallCollectionImpl* GetDrawCallCollectionImpl(TypeId typeId);
extern HYP_API IDrawCallCollectionImpl* SetDrawCallCollectionImpl(TypeId typeId, UniquePtr<IDrawCallCollectionImpl>&& impl);

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
