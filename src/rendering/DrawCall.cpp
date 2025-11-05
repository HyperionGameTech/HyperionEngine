/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <rendering/DrawCall.hpp>
#include <rendering/IndirectDraw.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/RenderProxy.hpp>

#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>

#include <scene/Entity.hpp>

#include <scene/animation/Skeleton.hpp>

#include <core/reflection/Class.hpp>

#include <core/logging/Logger.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(RenderCollection);

extern RenderGlobalState* g_renderGlobalState;

HYP_API extern const char* LookupTypeName(const TypeId& typeId);

HYP_API GpuBufferHolderMap* GetGpuBufferHolderMap()
{
    return g_renderGlobalState->gpuBufferHolders;
}

#pragma region DrawCallCollection

DrawCallCollection::DrawCallCollection(DrawCallCollection&& other) noexcept
    : batchAllocator(other.batchAllocator),
      renderGroup(other.renderGroup),
      drawCalls(std::move(other.drawCalls)),
      instancedDrawCalls(std::move(other.instancedDrawCalls)),
      indexMap(std::move(other.indexMap))
{
}

DrawCallCollection& DrawCallCollection::operator=(DrawCallCollection&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    ResetDrawCalls();

    batchAllocator = other.batchAllocator;
    renderGroup = other.renderGroup;
    drawCalls = std::move(other.drawCalls);
    instancedDrawCalls = std::move(other.instancedDrawCalls);
    indexMap = std::move(other.indexMap);

    return *this;
}

DrawCallCollection::~DrawCallCollection()
{
    if (batchAllocator != nullptr)
    {
        ResetDrawCalls();
    }
}

void DrawCallCollection::PushRenderProxy(DrawCallID id, const RenderProxyMesh& renderProxy)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    AssertDebug(renderProxy.mesh != nullptr && renderProxy.material != nullptr);

    drawCalls.Push(
        id,
        renderProxy.mesh,
        renderProxy.material,
        renderProxy.skeleton,
        renderProxy.entity.Id(),
        renderProxy.numIndices);
}

void DrawCallCollection::PushRenderProxyInstanced(EntityInstanceBatch* batch, DrawCallID id, const RenderProxyMesh& renderProxy)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    // Auto-instancing: check if we already have a drawcall we can use for the given DrawCallID.
    auto indexMapIt = indexMap.Find(uint64(id));

    if (indexMapIt == indexMap.End())
    {
        indexMapIt = indexMap.Insert(uint64(id), {}).first;
    }

    const uint32 initialIndexMapSize = uint32(indexMapIt->second.Size());

    uint32 indexMapIndex = 0;
    uint32 instanceOffset = 0;

    const uint32 initialNumInstances = renderProxy.instanceData.numInstances;
    uint32 numInstances = initialNumInstances;

    AssertDebug(initialNumInstances > 0);

    GpuBufferHolderBase* entityInstanceBatches = batchAllocator->GetGpuBufferHolder();
    Assert(entityInstanceBatches != nullptr);

    while (numInstances != 0)
    {
        SizeType drawCallIndex;

        if (indexMapIndex < initialIndexMapSize)
        {
            // we have elements for the specific DrawCallID -- try to reuse them as much as possible
            drawCallIndex = indexMapIt->second[indexMapIndex++];

            AssertDebug(instancedDrawCalls.ids[drawCallIndex] == id);
            AssertDebug(instancedDrawCalls.batches[drawCallIndex] != nullptr);
        }
        else
        {
            // check if we need to allocate new batch (if it has not been provided as first argument)
            if (batch == nullptr)
            {
                batch = batchAllocator->AcquireBatch();
            }

            AssertDebug(batch->batchIndex != ~0u);

            drawCallIndex = instancedDrawCalls.Push(
                id,
                renderProxy.mesh,
                renderProxy.material,
                renderProxy.skeleton,
                batch,
                renderProxy.numIndices);

            indexMapIt->second.PushBack(drawCallIndex);

            // Used, set it to nullptr so it doesn't get released
            batch = nullptr;
        }

        const uint32 remainingInstances = PushEntityToBatch(drawCallIndex, renderProxy.entity.GetUnsafe(), renderProxy.instanceData, numInstances, instanceOffset);

        instanceOffset += numInstances - remainingInstances;
        numInstances = remainingInstances;
    }

    if (batch != nullptr)
    {
        // it has not been recycled if not nullptr - need to release it!
        batchAllocator->ReleaseBatch(batch);
    }
}

EntityInstanceBatch* DrawCallCollection::TakeDrawCallBatch(DrawCallID id)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    const auto it = indexMap.Find(id.Value());

    if (it != indexMap.End())
    {
        for (SizeType drawCallIndex : it->second)
        {
            EntityInstanceBatch* batch = instancedDrawCalls.batches[drawCallIndex];

            if (!batch)
            {
                continue;
            }

            instancedDrawCalls.batches[drawCallIndex] = nullptr;

            return batch;
        }
    }

    return nullptr;
}

void DrawCallCollection::ResetDrawCalls()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    AssertDebug(batchAllocator != nullptr);

    GpuBufferHolderBase* entityInstanceBatches = batchAllocator->GetGpuBufferHolder();
    AssertDebug(entityInstanceBatches != nullptr);

    for (SizeType i = 0; i < instancedDrawCalls.Size(); i++)
    {
        EntityInstanceBatch* batch = instancedDrawCalls.batches[i];

        if (batch != nullptr)
        {
            const uint32 batchIndex = batch->batchIndex;
            AssertDebug(batchIndex != ~0u);

            *batch = EntityInstanceBatch { batchIndex };

            batchAllocator->ReleaseBatch(batch);

            instancedDrawCalls.batches[i] = nullptr;
        }
    }

    drawCalls.Clear();
    instancedDrawCalls.Clear();
    indexMap.Clear();
}

uint32 DrawCallCollection::PushEntityToBatch(SizeType drawCallIndex, Entity* entity, const MeshInstanceData& meshInstanceData, uint32 numInstances, uint32 instanceOffset)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

#ifdef HYP_DEBUG_MODE // Sanity checks
    // type check - cannot be a subclass of Entity, indices would get messed up
    Assert(entity->InstanceClass() == Entity::StaticClass(), "Cannot push Entity subclass to EntityInstanceBatch: {}", entity->InstanceClass()->GetName());

    // bounds check
    Assert(numInstances <= meshInstanceData.numInstances);

    // buffer size check
    for (uint32 bufferIndex = 0; bufferIndex < uint32(meshInstanceData.buffers.Size()); bufferIndex++)
    {
        Assert(meshInstanceData.buffers[bufferIndex].Size() / meshInstanceData.bufferStructSizes[bufferIndex] == meshInstanceData.numInstances);
    }
#endif

    const SizeType batchStructSize = batchAllocator->GetStructSize();

    EntityInstanceBatch* batch = instancedDrawCalls.batches[drawCallIndex];
    uint32& count = instancedDrawCalls.counts[drawCallIndex];
    FixedArray<ObjId<Entity>, MaxEntitiesPerBatch>& entityIdsArray = instancedDrawCalls.entityIds[drawCallIndex];

    bool dirty = false;

    if (meshInstanceData.buffers.Any())
    {
        while (batch->numEntities < MaxEntitiesPerBatch && numInstances != 0)
        {
            const uint32 entityIndex = batch->numEntities++;

            batch->indices[entityIndex] = uint32(entity->Id().ToIndex());

            // Starts at the offset of `transforms` in EntityInstanceBatch - data in buffers is expected to be
            // after the `indices` element
            uint32 fieldOffset = offsetof(EntityInstanceBatch, transforms);

            for (uint32 bufferIndex = 0; bufferIndex < uint32(meshInstanceData.buffers.Size()); bufferIndex++)
            {
                const uint32 bufferStructSize = meshInstanceData.bufferStructSizes[bufferIndex];
                const uint32 bufferStructAlignment = meshInstanceData.bufferStructAlignments[bufferIndex];

                AssertDebug(meshInstanceData.buffers[bufferIndex].Size() % bufferStructSize == 0,
                    "Buffer size is not a multiple of buffer struct size! Buffer size: %u, Buffer struct size: %u",
                    meshInstanceData.buffers[bufferIndex].Size(), bufferStructSize);

                fieldOffset = ByteUtil::AlignAs(fieldOffset, bufferStructAlignment);

                void* dstPtr = reinterpret_cast<void*>((UIntPtr(batch)) + fieldOffset + (entityIndex * bufferStructSize));
                void* srcPtr = reinterpret_cast<void*>(UIntPtr(meshInstanceData.buffers[bufferIndex].Data()) + (instanceOffset * bufferStructSize));

                // sanity checks
                AssertDebug((UIntPtr(dstPtr) + bufferStructSize) - UIntPtr(batch) <= batchStructSize,
                    "Buffer struct size is larger than batch size! Buffer struct size: %u, Buffer struct alignment: %u, Batch size: %u, Entity index: %u, Field offset: %u",
                    bufferStructSize, bufferStructAlignment, batchStructSize, entityIndex, fieldOffset);

                AssertDebug(meshInstanceData.buffers[bufferIndex].Size() >= (instanceOffset + 1) * bufferStructSize,
                    "Buffer size is not large enough to copy data! Buffer size: %u, Buffer struct size: %u, Instance offset: %u",
                    meshInstanceData.buffers[bufferIndex].Size(), bufferStructSize, instanceOffset);

                Memory::MemCpy(dstPtr, srcPtr, bufferStructSize);

                fieldOffset += MaxEntitiesPerBatch * bufferStructSize;
            }

            instanceOffset++;

            entityIdsArray[count++] = entity->Id();

            --numInstances;

            dirty = true;
        }
    }
    else
    {
        while (batch->numEntities < MaxEntitiesPerBatch && numInstances != 0)
        {
            const uint32 entityIndex = batch->numEntities++;

            batch->indices[entityIndex] = uint32(entity->Id().ToIndex());
            batch->transforms[entityIndex] = Mat4f::identity;

            entityIdsArray[count++] = entity->Id();

            --numInstances;

            dirty = true;
        }
    }

    if (dirty)
    {
        batchAllocator->GetGpuBufferHolder()->MarkDirty(batch->batchIndex);
    }

    return numInstances;
}

#pragma endregion DrawCallCollection

#pragma region TEntityBatchAllocator

static TypeMap<UniquePtr<EntityBatchAllocatorBase>> s_drawCallCollectionImplMap = {};
static Mutex s_drawCallCollectionImplMapMutex = {};

EntityBatchAllocatorBase::EntityBatchAllocatorBase(GpuBufferHolderBase* bufferHolder)
    : m_bufferHolder(bufferHolder)
{
    Assert(m_bufferHolder != nullptr);

    const TypeInfo* structTypeInfo = m_bufferHolder->GetStructTypeInfo();
    Assert(structTypeInfo != nullptr);

    m_structSize = structTypeInfo->size;
    m_structAlignment = structTypeInfo->alignment;
}

HYP_API EntityBatchAllocatorBase* GetEntityBatchAllocator(TypeId typeId)
{
    Mutex::Guard guard(s_drawCallCollectionImplMapMutex);

    auto it = s_drawCallCollectionImplMap.Find(typeId);

    return it != s_drawCallCollectionImplMap.End() ? it->second.Get() : nullptr;
}

HYP_API EntityBatchAllocatorBase* SetEntityBatchAllocator(TypeId typeId, UniquePtr<EntityBatchAllocatorBase>&& impl)
{
    Mutex::Guard guard(s_drawCallCollectionImplMapMutex);

    auto it = s_drawCallCollectionImplMap.Set(typeId, std::move(impl)).first;

    return it->second.Get();
}

#pragma endregion TEntityBatchAllocator

} // namespace hyperion
