/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/DrawCall.hpp>
#include <Rendering/IndirectDraw.hpp>
#include <Rendering/RenderInterface.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/Mesh.hpp>
#include <Rendering/Material.hpp>
#include <Rendering/InstancedMeshData.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Scene/Entity.hpp>

#include <Scene/Animation/Skeleton.hpp>

#include <DrawCall.generated.inl>

namespace Hyperion {

CORE_API extern const char* LookupTypeName(const TypeId& typeId);

// Register allocator for the batch type used if none other is specified
HYP_REGISTER_DRAW_BATCH_TYPE(EntityInstanceBatch);
// Batch type used for typical drawing (instanced meshes) 
HYP_REGISTER_DRAW_BATCH_TYPE(MeshEntityInstanceBatch);

#pragma region DrawCallCollection

DrawCallCollection::~DrawCallCollection()
{
    if (batchAllocator != nullptr)
    {
        ResetDrawCalls();
    }
}

void DrawCallCollection::PushDrawCall(DrawCallID id, const RenderProxyMesh* renderProxy)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(renderProxy != nullptr && renderProxy->mesh != nullptr && renderProxy->material != nullptr);

    drawCalls.Push(id, renderProxy, Resources::GetBinding(renderProxy->entity));
}

void DrawCallCollection::PushInstancedDrawCall(DrawCallID id, const RenderProxyMesh* renderProxy, EntityInstanceBatch* batch)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    Assert(renderProxy != nullptr);

    // Auto-instancing: check if we already have a drawcall we can use for the given DrawCallID.
    auto indexMapIt = indexMap.Find(uint64(id));

    if (indexMapIt == indexMap.End())
    {
        indexMapIt = indexMap.Insert(uint64(id), {}).first;
    }

    const uint32 initialIndexMapSize = uint32(indexMapIt->second.Size());

    uint32 indexMapIndex = 0;
    uint32 instanceOffset = 0;

    uint32 numInstances = MathUtil::Max(renderProxy->numInstances, 1);

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

            drawCallIndex = instancedDrawCalls.Push(id, renderProxy, batch);

            indexMapIt->second.PushBack(drawCallIndex);

            // Used, set it to nullptr so it doesn't get released
            batch = nullptr;
        }

        const uint32 remainingInstances = PushEntityToBatch(
            drawCallIndex,
            renderProxy->entity,
            renderProxy->instanceData,
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

    for (size_t i = 0; i < instancedDrawCalls.Size(); i++)
    {
        EntityInstanceBatch* batch = instancedDrawCalls.batches[i];

        if (batch != nullptr)
        {
            const uint32 batchIndex = batch->batchIndex;
            AssertDebug(batchIndex != ~0u);

            // `batch` points to a BatchType instance (e.g. MeshEntityInstanceBatch) that is larger than
            // EntityInstanceBatch. Resetting via `*batch = EntityInstanceBatch { batchIndex }` only clears the
            // EntityInstanceBatch base subobject and leaves any derived-only fields (e.g. previousTransforms)
            // stale, so zero the full allocated struct instead.
            Memory::Zero(batch, batchAllocator->GetStructSize());
            batch->batchIndex = batchIndex;

            batchAllocator->ReleaseBatch(batch);

            instancedDrawCalls.batches[i] = nullptr;
        }
    }

    drawCalls.Clear();
    instancedDrawCalls.Clear();
    indexMap.Clear();
}

HYP_NODISCARD uint32 DrawCallCollection::PushEntityToBatch(
    size_t drawCallIndex,
    Entity* entity,
    const InstanceData& instanceData,
    uint32 numInstances,
    uint32 instanceOffset)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    const size_t batchStructSize = batchAllocator->GetStructSize();

    EntityInstanceBatch* batch = instancedDrawCalls.batches[drawCallIndex];
    uint32& count = instancedDrawCalls.counts[drawCallIndex];

    bool dirty = false;

    const bool hasBufferData = (instanceData.bufferStructSizes[0] != 0);

    if (hasBufferData)
    {
        while (batch->numEntities < MaxEntitiesPerBatch && numInstances != 0)
        {
            const uint32 idx = batch->numEntities++;

            batch->indices[idx] = (Resources::GetBinding(entity) & 0xFFFFFFu) | (idx << 24);

            // Starts at the offset of `transforms` in EntityInstanceBatch - data in buffers is expected to be
            // after the `indices` element
            uint32 fieldOffset = offsetof(EntityInstanceBatch, transforms);

            for (uint32 bufferIndex = 0; bufferIndex < GetArrayCount(instanceData.buffers); bufferIndex++)
            {
                const uint32 bufferStructSize = instanceData.bufferStructSizes[bufferIndex];
                const uint32 bufferStructAlignment = instanceData.bufferStructAlignments[bufferIndex];

                if (bufferStructSize == 0)
                {
                    continue;
                }

                AssertDebug(instanceData.buffers[bufferIndex].Size() % bufferStructSize == 0,
                            "Buffer size is not a multiple of buffer struct size! Buffer size: {}, Buffer struct size: {}",
                            instanceData.buffers[bufferIndex].Size(), bufferStructSize);

                fieldOffset = ByteUtil::AlignAs(fieldOffset, bufferStructAlignment);

                uint8* dstPtr = reinterpret_cast<uint8*>(batch) + fieldOffset + (idx * bufferStructSize);
                const uint8* srcPtr = reinterpret_cast<const uint8*>(instanceData.buffers[bufferIndex].Data()) + (instanceOffset * bufferStructSize);

                // sanity checks
                AssertDebug((dstPtr + bufferStructSize) - reinterpret_cast<const uint8*>(batch) <= batchStructSize,
                            "Buffer struct size is larger than batch size! Buffer struct size: {}, Buffer struct alignment: {}, Batch size: {}, Entity index: {}, Field offset: {}",
                            bufferStructSize, bufferStructAlignment, batchStructSize, idx, fieldOffset);

                AssertDebug(instanceData.buffers[bufferIndex].Size() >= (instanceOffset + 1) * bufferStructSize,
                            "Buffer size is not large enough to copy data! Buffer size: {}, Buffer struct size: {}, Instance offset: {}",
                            instanceData.buffers[bufferIndex].Size(), bufferStructSize, instanceOffset);

                Memory::Copy(dstPtr, srcPtr, bufferStructSize);

                fieldOffset += MaxEntitiesPerBatch * bufferStructSize;
            }

            instanceOffset++;

            count++;

            --numInstances;

            dirty = true;
        }
    }
    else
    {
        while (batch->numEntities < MaxEntitiesPerBatch && numInstances != 0)
        {
            const uint32 idx = batch->numEntities++;

            batch->indices[idx] = (Resources::GetBinding(entity) & 0xFFFFFFu) | (idx << 24);
            batch->transforms[idx] = Mat4f::identity;

            count++;

            --numInstances;

            dirty = true;
        }
    }

    if (dirty)
    {
        batchAllocator->MarkBatchDirty(batch);
    }

    return numInstances;
}

#pragma endregion DrawCallCollection

#pragma region TEntityBatchAllocator

static Map<TypeId, EntityBatchAllocatorBase*> s_entityBatchAllocatorMap;

using CreateFnMap = Map<TypeId, PFNCreateEntityBatchAllocator>;

static Mutex& GetEntityBatchAllocatorMutex()
{
    static Mutex s_entityBatchAllocatorMutex;
    return s_entityBatchAllocatorMutex;
}

static CreateFnMap& GetEntityBatchAllocatorCreateFnMap()
{
    static CreateFnMap s_entityBatchAllocatorCreateFnMap;
    return s_entityBatchAllocatorCreateFnMap;
}

EntityBatchAllocatorBase::EntityBatchAllocatorBase(const TypeInfo* structTypeInfo, uint32 maxBatches)
    : m_sbuffer(maxBatches, TypeInfo_GetSize(*structTypeInfo))
{
    Assert(structTypeInfo != nullptr);

    m_structSize = structTypeInfo->size;
    m_structAlignment = structTypeInfo->alignment;
}

void EntityBatchAllocatorBase::Initialize()
{
    m_sbuffer.Initialize();
}

void EntityBatchAllocatorBase::ReleaseBatch(EntityInstanceBatch* batch)
{
    m_indexAllocator.Free(batch->batchIndex);
}

void EntityBatchAllocatorBase::MarkBatchDirty(EntityInstanceBatch* batch)
{
    m_sbuffer.MarkDirty(batch->batchIndex * m_structSize, m_structSize);
}

EntityBatchAllocatorBase* GetEntityBatchAllocator(const TypeId& typeId)
{
    if (!typeId)
    {
        return nullptr;
    }

    Mutex& mtx = GetEntityBatchAllocatorMutex();

    auto it = s_entityBatchAllocatorMap.Find(typeId);

    if (it != s_entityBatchAllocatorMap.End())
    {
        return it->second;
    }

    CreateFnMap& funcs = GetEntityBatchAllocatorCreateFnMap();

    Mutex::Guard guard(mtx);

    auto createFnIt = funcs.Find(typeId);
    AssertDebug(createFnIt != funcs.End());

    EntityBatchAllocatorBase* pBatchAllocator = createFnIt->second();
    AssertDebug(pBatchAllocator != nullptr);

    pBatchAllocator->Initialize();

    s_entityBatchAllocatorMap.Set(typeId, pBatchAllocator);

    return pBatchAllocator;
}

HYP_NODISCARD static bool SetEntityBatchAllocator(const TypeId& typeId, EntityBatchAllocatorBase* pBatchAllocator)
{
    if (!typeId || !pBatchAllocator)
    {
        return false;
    }

    Mutex& mtx = GetEntityBatchAllocatorMutex();

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
        {
            Mutex& mtx = GetEntityBatchAllocatorMutex();
            CreateFnMap& funcs = GetEntityBatchAllocatorCreateFnMap();

            Mutex::Guard guard(mtx);

            PFNCreateEntityBatchAllocator createFn = nullptr;

            auto createFnIt = funcs.Find(typeId);
            AssertDebug(createFnIt != funcs.End());

            createFn = createFnIt->second;
            AssertDebug(createFn != nullptr);

            batchAllocator = createFn();
            AssertDebug(batchAllocator != nullptr);

            batchAllocator->Initialize();
        }

        if (!SetEntityBatchAllocator(typeId, batchAllocator))
        {
            PoolDelete(*g_renderPool, batchAllocator);
            batchAllocator = nullptr;
        }
    }

    return batchAllocator;
}

const Map<TypeId, EntityBatchAllocatorBase*>& GetAllEntityBatchAllocators()
{
    return s_entityBatchAllocatorMap;
}

void RegisterEntityBatchAllocator(const TypeId& typeId, PFNCreateEntityBatchAllocator createFn)
{
    if (!typeId || !createFn)
    {
        return;
    }

    Mutex& mtx = GetEntityBatchAllocatorMutex();
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
