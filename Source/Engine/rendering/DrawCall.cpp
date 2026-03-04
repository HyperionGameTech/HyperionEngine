/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/DrawCall.hpp>
#include <rendering/IndirectDraw.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>
#include <rendering/InstancedMeshProxy.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <scene/Entity.hpp>

#include <scene/animation/Skeleton.hpp>

#include <DrawCall.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(RenderCollection);

HYP_API extern const char* LookupTypeName(const TypeId& typeId);

HYP_API GpuBufferHolderMap* GetGpuBufferHolderMap()
{
    return g_renderInterface->gpuBufferHolders;
}

// Register allocator for the batch type used if none other is specified
HYP_REGISTER_DRAW_BATCH_TYPE(EntityInstanceBatch);
HYP_REGISTER_DRAW_BATCH_TYPE(MeshEntityInstanceBatch);

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
    AssertOnThread(g_renderThread);

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
    AssertOnThread(g_renderThread);

    // Auto-instancing: check if we already have a drawcall we can use for the given DrawCallID.
    auto indexMapIt = indexMap.Find(uint64(id));

    if (indexMapIt == indexMap.End())
    {
        indexMapIt = indexMap.Insert(uint64(id), {}).first;
    }

    const uint32 initialIndexMapSize = uint32(indexMapIt->second.Size());

    uint32 indexMapIndex = 0;
    uint32 instanceOffset = 0;

    const uint32 initialNumInstances = renderProxy.numInstances;
    uint32 numInstances = initialNumInstances;

    AssertDebug(initialNumInstances > 0);

    GpuBufferHolderBase* entityInstanceBatches = batchAllocator->GetGpuBufferHolder();
    Assert(entityInstanceBatches != nullptr);

    while (numInstances != 0)
    {
        size_t drawCallIndex;

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

        const uint32 remainingInstances = PushEntityToBatch(
            drawCallIndex,
            renderProxy.entity.GetUnsafe(),
            renderProxy.instanceData,
            numInstances,
            instanceOffset);

        instanceOffset += numInstances - remainingInstances;
        numInstances = remainingInstances;
    }

    if (batch != nullptr)
    {
        // it has not been recycled if not nullptr - need to release it!
        batchAllocator->ReleaseBatch(batch);
    }
}

EntityInstanceBatch* DrawCallCollection::RecycleDrawBatch(DrawCallID id)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    const auto it = indexMap.Find(id.Value());

    if (it != indexMap.End())
    {
        for (size_t drawCallIndex : it->second)
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
    AssertOnThread(g_renderThread);

    AssertDebug(batchAllocator != nullptr);

    GpuBufferHolderBase* entityInstanceBatches = batchAllocator->GetGpuBufferHolder();
    AssertDebug(entityInstanceBatches != nullptr);

    for (size_t i = 0; i < instancedDrawCalls.Size(); i++)
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

uint32 DrawCallCollection::PushEntityToBatch(
    size_t drawCallIndex,
    Entity* entity,
    const InstanceData& instanceData,
    uint32 numInstances,
    uint32 instanceOffset)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

//#if HYP_DEBUG_MODE // Sanity checks
//    // type check - cannot be a subclass of Entity, indices would get messed up
//    Assert(entity->InstanceClass() == Entity::StaticClass(), "Cannot push Entity subclass to EntityInstanceBatch: {}", entity->InstanceClass()->GetName());
//
//    // buffer size check
//    for (uint32 bufferIndex = 0; bufferIndex < uint32(imp.buffers.Size()); bufferIndex++)
//    {
//        Assert(imp.buffers[bufferIndex].Size() / imp.bufferStructSizes[bufferIndex] == imp.numInstances);
//    }
//#endif

    const size_t batchStructSize = batchAllocator->GetStructSize();

    EntityInstanceBatch* batch = instancedDrawCalls.batches[drawCallIndex];
    uint32& count = instancedDrawCalls.counts[drawCallIndex];
    FixedArray<ObjId<Entity>, MaxEntitiesPerBatch>& entityIdsArray = instancedDrawCalls.entityIds[drawCallIndex];

    bool dirty = false;

    const bool hasBufferData = (instanceData.bufferStructSizes[0] != 0);

    if (hasBufferData)
    {
        while (batch->numEntities < MaxEntitiesPerBatch && numInstances != 0)
        {
            const uint32 entityIndex = batch->numEntities++;

            batch->indices[entityIndex] = uint32(entity->Id().ToIndex());

            // Starts at the offset of `transforms` in EntityInstanceBatch - data in buffers is expected to be
            // after the `indices` element
            uint32 fieldOffset = offsetof(EntityInstanceBatch, transforms);

            for (uint32 bufferIndex = 0; bufferIndex < std::size(instanceData.buffers); bufferIndex++)
            {
                const uint32 bufferStructSize = instanceData.bufferStructSizes[bufferIndex];
                const uint32 bufferStructAlignment = instanceData.bufferStructAlignments[bufferIndex];

                if (bufferStructSize == 0)
                    continue;

                AssertDebug(instanceData.buffers[bufferIndex].Size() % bufferStructSize == 0,
                    "Buffer size is not a multiple of buffer struct size! Buffer size: {}, Buffer struct size: {}",
                    instanceData.buffers[bufferIndex].Size(), bufferStructSize);

                fieldOffset = ByteUtil::AlignAs(fieldOffset, bufferStructAlignment);

                void* dstPtr = reinterpret_cast<void*>((UIntPtr(batch)) + fieldOffset + (entityIndex * bufferStructSize));
                void* srcPtr = reinterpret_cast<void*>(UIntPtr(instanceData.buffers[bufferIndex].Data()) + (instanceOffset * bufferStructSize));

                // sanity checks
                AssertDebug((UIntPtr(dstPtr) + bufferStructSize) - UIntPtr(batch) <= batchStructSize,
                    "Buffer struct size is larger than batch size! Buffer struct size: {}, Buffer struct alignment: {}, Batch size: {}, Entity index: {}, Field offset: {}",
                    bufferStructSize, bufferStructAlignment, batchStructSize, entityIndex, fieldOffset);

                AssertDebug(instanceData.buffers[bufferIndex].Size() >= (instanceOffset + 1) * bufferStructSize,
                    "Buffer size is not large enough to copy data! Buffer size: {}, Buffer struct size: {}, Instance offset: {}",
                    instanceData.buffers[bufferIndex].Size(), bufferStructSize, instanceOffset);

                Memory::Copy(dstPtr, srcPtr, bufferStructSize);

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

static HashMap<TypeId, EntityBatchAllocatorBase*> s_entityBatchAllocatorMap;

using CreateFnMap = HashMap<TypeId, PFNCreateEntityBatchAllocator>;

static Mutex& GetEntityBatchAllocatorCreateFnMapMutex()
{
    static Mutex s_entityBatchAllocatorCreateFnMapMutex;
    return s_entityBatchAllocatorCreateFnMapMutex;
}

static CreateFnMap& GetEntityBatchAllocatorCreateFnMap()
{
    static CreateFnMap s_entityBatchAllocatorCreateFnMap;
    return s_entityBatchAllocatorCreateFnMap;
}

EntityBatchAllocatorBase::EntityBatchAllocatorBase(GpuBufferHolderBase* bufferHolder)
    : m_bufferHolder(bufferHolder)
{
    Assert(m_bufferHolder != nullptr);

    const TypeInfo* structTypeInfo = m_bufferHolder->GetStructTypeInfo();
    Assert(structTypeInfo != nullptr);

    m_structSize = structTypeInfo->size;
    m_structAlignment = structTypeInfo->alignment;
}

void EntityBatchAllocatorBase::ReleaseBatch(EntityInstanceBatch* batch) const
{
    m_bufferHolder->ReleaseIndex(batch->batchIndex);
}

EntityBatchAllocatorBase* GetEntityBatchAllocator(const TypeId& typeId)
{
    if (!typeId)
    {
        return nullptr;
    }

    AssertOnThread(g_renderThread);

    auto it = s_entityBatchAllocatorMap.Find(typeId);

    if (it != s_entityBatchAllocatorMap.End())
    {
        return it->second;
    }

    Mutex& mtx = GetEntityBatchAllocatorCreateFnMapMutex();
    CreateFnMap& funcs = GetEntityBatchAllocatorCreateFnMap();

    Mutex::Guard guard(mtx);

    auto createFnIt = funcs.Find(typeId);
    AssertDebug(createFnIt != funcs.End());

    EntityBatchAllocatorBase* pBatchAllocator = createFnIt->second();
    AssertDebug(pBatchAllocator != nullptr);

    s_entityBatchAllocatorMap.Set(typeId, pBatchAllocator);

    return pBatchAllocator;
}

HYP_NODISCARD static bool SetEntityBatchAllocator(const TypeId& typeId, EntityBatchAllocatorBase* pBatchAllocator)
{
    if (!typeId || !pBatchAllocator)
    {
        return false;
    }

    AssertOnThread(g_renderThread);

    auto it = s_entityBatchAllocatorMap.Find(typeId);
    if (it != s_entityBatchAllocatorMap.End())
    {
        return false;
    }

    s_entityBatchAllocatorMap.Set(typeId, pBatchAllocator);

    return true;
}

EntityBatchAllocatorBase* GetOrCreateEntityBatchAllocator(const TypeId& typeId)
{
    if (!typeId)
    {
        return nullptr;
    }

    EntityBatchAllocatorBase* batchAllocator = GetEntityBatchAllocator(typeId);

    if (!batchAllocator)
    {
        Mutex& mtx = GetEntityBatchAllocatorCreateFnMapMutex();
        CreateFnMap& funcs = GetEntityBatchAllocatorCreateFnMap();

        Mutex::Guard guard(mtx);

        PFNCreateEntityBatchAllocator createFn = nullptr;

        auto createFnIt = funcs.Find(typeId);
        AssertDebug(createFnIt != funcs.End());

        createFn = createFnIt->second;
        AssertDebug(createFn != nullptr);

        batchAllocator = createFn();
        AssertDebug(batchAllocator != nullptr);
        AssertDebug(batchAllocator->GetGpuBufferHolder() == nullptr);

        if (!SetEntityBatchAllocator(typeId, batchAllocator))
        {
            PoolDelete(*g_renderPool, batchAllocator);
            batchAllocator = nullptr;
        }
    }

    return batchAllocator;
}

void RegisterEntityBatchAllocator(const TypeId& typeId, PFNCreateEntityBatchAllocator createFn)
{
    if (!typeId || !createFn)
    {
        return;
    }

    Mutex& mtx = GetEntityBatchAllocatorCreateFnMapMutex();
    CreateFnMap& funcs = GetEntityBatchAllocatorCreateFnMap();

    Mutex::Guard guard(mtx);

    auto it = funcs.Find(typeId);
    if (it != funcs.End())
    {
        return;
    }

    funcs.Set(typeId, createFn);
}

#pragma endregion TEntityBatchAllocator

} // namespace Hyperion
