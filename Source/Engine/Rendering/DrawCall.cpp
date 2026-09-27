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

#include <Rendering/Util/DeletionQueue.hpp>

#include <Framework/EngineStats.hpp>

#include <DrawCall.generated.inl>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Rendering);

EngineStatCounter<uint32> g_statInstanceBatchesInUse("Rendering/Instancing/BatchesInUse", false);

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

void DrawCallCollection::PushInstances(DrawCallID id, const RenderProxyMesh* renderProxy, Span<const uint32> instanceSlots, DrawCallCollection* previous)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    Assert(renderProxy != nullptr);
    AssertDebug(batchAllocator != nullptr);

    auto indexMapIt = indexMap.Find(uint64(id));

    if (indexMapIt == indexMap.End())
    {
        indexMapIt = indexMap.Insert(uint64(id), {}).first;
    }

    const uint32 entityBinding = Resources::GetBinding(renderProxy->entity);

    // every batch for this id but the last is full, so only the last can take more
    size_t drawCallIndex = indexMapIt->second.Any() ? indexMapIt->second.Back() : ~size_t(0);
    EntityInstanceBatch* batch = drawCallIndex != ~size_t(0) ? instancedDrawCalls.batches[drawCallIndex] : nullptr;

    bool batchWritten = false;

    for (uint32 instanceSlot : instanceSlots)
    {
        if (batch == nullptr || batch->numEntities >= MaxInstancesPerBatch)
        {
            if (batchWritten)
            {
                batchAllocator->MarkBatchDirty(batch);

                batchWritten = false;
            }

            batch = previous != nullptr ? previous->TakeBatch(id) : nullptr;

            if (batch == nullptr)
            {
                batch = batchAllocator->AcquireBatch();
            }

            // out of batches, the rest of these instances aren't drawn this frame
            if (batch == nullptr)
            {
                return;
            }

            drawCallIndex = instancedDrawCalls.Push(id, renderProxy, batch);

            indexMapIt->second.PushBack(drawCallIndex);
        }

        const uint32 entryIndex = batch->numEntities++;

        batch->indices[entryIndex] = (entityBinding & 0xFFFFFFu) | (entryIndex << 24);
        batch->instanceSlots[entryIndex] = instanceSlot;

        instancedDrawCalls.counts[drawCallIndex]++;

        batchWritten = true;
    }

    if (batchWritten)
    {
        batchAllocator->MarkBatchDirty(batch);
    }
}

EntityInstanceBatch* DrawCallCollection::TakeBatch(DrawCallID id)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    const auto it = indexMap.Find(id.Value());

    if (it == indexMap.End())
    {
        return nullptr;
    }

    for (size_t drawCallIndex : it->second)
    {
        EntityInstanceBatch* batch = instancedDrawCalls.batches[drawCallIndex];

        if (!batch)
        {
            continue;
        }

        instancedDrawCalls.batches[drawCallIndex] = nullptr;

        batch->numEntities = 0;

        return batch;
    }

    return nullptr;
}

void DrawCallCollection::ResetDrawCalls()
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(batchAllocator != nullptr);

    for (EntityInstanceBatch*& batch : instancedDrawCalls.batches)
    {
        if (batch != nullptr)
        {
            batchAllocator->ReleaseBatch(batch);

            batch = nullptr;
        }
    }

    drawCalls.Clear();
    instancedDrawCalls.Clear();
    indexMap.Clear();
}

#pragma endregion DrawCallCollection

#pragma region EntityBatchAllocator

EntityBatchAllocator::EntityBatchAllocator()
    : m_sbuffer(MaxEntityInstanceBatches, sizeof(EntityInstanceBatch))
{
}

void EntityBatchAllocator::Initialize()
{
    m_sbuffer.Initialize();
}

void EntityBatchAllocator::Shutdown()
{
    m_sbuffer.Shutdown();
}

EntityInstanceBatch* EntityBatchAllocator::AcquireBatch()
{
    const uint32 batchIndex = m_indexAllocator.Allocate();

    if (batchIndex >= MaxEntityInstanceBatches)
    {
        m_indexAllocator.Free(batchIndex);

        HYP_LOG_ONCE(Rendering, Error, "Entity instance batch limit ({}) exceeded, some instances aren't drawn. Consider increasing MaxEntityInstanceBatches.", MaxEntityInstanceBatches);

        return nullptr;
    }

    EntityInstanceBatch* batch = reinterpret_cast<EntityInstanceBatch*>(m_sbuffer.cpuBuffer.Data() + batchIndex * sizeof(EntityInstanceBatch));
    batch->batchIndex = batchIndex;
    batch->numEntities = 0;

    g_statInstanceBatchesInUse++;

    return batch;
}

void EntityBatchAllocator::ReleaseBatch(EntityInstanceBatch* batch)
{
    m_indexAllocator.Free(batch->batchIndex);

    g_statInstanceBatchesInUse--;
}

void EntityBatchAllocator::MarkBatchDirty(EntityInstanceBatch* batch)
{
    m_sbuffer.MarkDirty(batch->batchIndex * sizeof(EntityInstanceBatch), sizeof(EntityInstanceBatch));
}

#pragma endregion EntityBatchAllocator

} // namespace Hyperion
