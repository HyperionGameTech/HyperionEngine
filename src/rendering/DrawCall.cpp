/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/DrawCall.hpp>
#include <rendering/IndirectDraw.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/RenderProxy.hpp>

#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>

#include <scene/animation/Skeleton.hpp>

#include <core/logging/Logger.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(RenderCollection);

extern RenderGlobalState* g_renderGlobalState;

HYP_API GpuBufferHolderMap* GetGpuBufferHolderMap()
{
    return g_renderGlobalState->gpuBufferHolders.Get();
}

#pragma region DrawCallCollection

DrawCallCollection::DrawCallCollection(DrawCallCollection&& other) noexcept
    : impl(other.impl),
      drawCalls(std::move(other.drawCalls)),
      instancedDrawCalls(std::move(other.instancedDrawCalls)),
      instancedDrawCallIndexMap(std::move(other.instancedDrawCallIndexMap))
{
}

DrawCallCollection& DrawCallCollection::operator=(DrawCallCollection&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    ResetDrawCalls();

    impl = other.impl;
    drawCalls = std::move(other.drawCalls);
    instancedDrawCalls = std::move(other.instancedDrawCalls);
    instancedDrawCallIndexMap = std::move(other.instancedDrawCallIndexMap);

    return *this;
}

DrawCallCollection::~DrawCallCollection()
{
    if (impl != nullptr)
    {
        ResetDrawCalls();
    }
}

void DrawCallCollection::PushRenderProxy(DrawCallID id, const RenderProxyMesh& renderProxy)
{
    AssertDebug(renderProxy.mesh != nullptr && renderProxy.material != nullptr);

    DrawCall& drawCall = drawCalls.EmplaceBack();
    drawCall.id = id;
    drawCall.mesh = renderProxy.mesh;
    drawCall.material = renderProxy.material;
    drawCall.skeleton = renderProxy.skeleton;
    drawCall.entityId = renderProxy.entity.Id();
    drawCall.drawCommandIndex = ~0u;
}

void DrawCallCollection::PushRenderProxyInstanced(EntityInstanceBatch* batch, DrawCallID id, const RenderProxyMesh& renderProxy)
{
    // Auto-instancing: check if we already have a drawcall we can use for the given DrawCallID.
    auto indexMapIt = instancedDrawCallIndexMap.Find(uint64(id));

    if (indexMapIt == instancedDrawCallIndexMap.End())
    {
        indexMapIt = instancedDrawCallIndexMap.Insert(uint64(id), {}).first;
    }

    const uint32 initialIndexMapSize = uint32(indexMapIt->second.Size());

    uint32 indexMapIndex = 0;
    uint32 instanceOffset = 0;

    const uint32 initialNumInstances = renderProxy.instanceData.numInstances;
    uint32 numInstances = initialNumInstances;

    AssertDebug(initialNumInstances > 0);

    GpuBufferHolderBase* entityInstanceBatches = impl->GetGpuBufferHolder();
    Assert(entityInstanceBatches != nullptr);

    while (numInstances != 0)
    {
        InstancedDrawCall* drawCall;

        if (indexMapIndex < initialIndexMapSize)
        {
            // we have elements for the specific DrawCallID -- try to reuse them as much as possible
            drawCall = &instancedDrawCalls[indexMapIt->second[indexMapIndex++]];

            AssertDebug(drawCall->id == id);
            AssertDebug(drawCall->batch != nullptr);
        }
        else
        {
            // check if we need to allocate new batch (if it has not been provided as first argument)
            if (batch == nullptr)
            {
                batch = impl->AcquireBatch();
            }

            AssertDebug(batch->batchIndex != ~0u);

            drawCall = &instancedDrawCalls.EmplaceBack();

            *drawCall = InstancedDrawCall {};
            drawCall->id = id;
            drawCall->batch = batch;
            drawCall->drawCommandIndex = ~0u;
            drawCall->mesh = renderProxy.mesh;
            drawCall->material = renderProxy.material;
            drawCall->skeleton = renderProxy.skeleton;
            drawCall->count = 0;

            indexMapIt->second.PushBack(instancedDrawCalls.Size() - 1);

            // Used, set it to nullptr so it doesn't get released
            batch = nullptr;
        }

        const uint32 remainingInstances = PushEntityToBatch(*drawCall, renderProxy.entity.Id(), renderProxy.instanceData, numInstances, instanceOffset);

        instanceOffset += numInstances - remainingInstances;
        numInstances = remainingInstances;
    }

    if (batch != nullptr)
    {
        // ticket has not been used at this point (always gets set to 0 after used) - need to release it
        impl->ReleaseBatch(batch);
    }
}

EntityInstanceBatch* DrawCallCollection::TakeDrawCallBatch(DrawCallID id)
{
    const auto it = instancedDrawCallIndexMap.Find(id.Value());

    if (it != instancedDrawCallIndexMap.End())
    {
        for (SizeType drawCallIndex : it->second)
        {
            InstancedDrawCall& drawCall = instancedDrawCalls[drawCallIndex];

            if (!drawCall.batch)
            {
                continue;
            }

            EntityInstanceBatch* batch = drawCall.batch;

            drawCall.batch = nullptr;

            return batch;
        }
    }

    return nullptr;
}

void DrawCallCollection::ResetDrawCalls()
{
    AssertDebug(impl != nullptr);

    GpuBufferHolderBase* entityInstanceBatches = impl->GetGpuBufferHolder();
    AssertDebug(entityInstanceBatches != nullptr);

    for (InstancedDrawCall& drawCall : instancedDrawCalls)
    {
        if (drawCall.batch != nullptr)
        {
            const uint32 batchIndex = drawCall.batch->batchIndex;
            AssertDebug(batchIndex != ~0u);

            *drawCall.batch = EntityInstanceBatch { batchIndex };

            impl->ReleaseBatch(drawCall.batch);

            drawCall.batch = nullptr;
        }
    }

    drawCalls.Clear();
    instancedDrawCalls.Clear();
    instancedDrawCallIndexMap.Clear();
}

uint32 DrawCallCollection::PushEntityToBatch(InstancedDrawCall& drawCall, ObjId<Entity> entityId, const MeshInstanceData& meshInstanceData, uint32 numInstances, uint32 instanceOffset)
{
#ifdef HYP_DEBUG_MODE // Sanity checks
    // type check - cannot be a subclass of Entity, indices would get messed up
    extern HYP_API const char* LookupTypeName(TypeId typeId);

    static constexpr TypeId entityTypeId = TypeId::ForType<Entity>();
    Assert(entityId.GetTypeId() == entityTypeId, "Cannot push Entity subclass to EntityInstanceBatch: {}", LookupTypeName(entityId.GetTypeId()));

    // bounds check
    Assert(numInstances <= meshInstanceData.numInstances);

    // buffer size check
    for (uint32 bufferIndex = 0; bufferIndex < uint32(meshInstanceData.buffers.Size()); bufferIndex++)
    {
        Assert(meshInstanceData.buffers[bufferIndex].Size() / meshInstanceData.bufferStructSizes[bufferIndex] == meshInstanceData.numInstances);
    }
#endif

    const SizeType batchSizeof = impl->GetStructSize();

    bool dirty = false;

    if (meshInstanceData.buffers.Any())
    {
        while (drawCall.batch->numEntities < maxEntitiesPerInstanceBatch && numInstances != 0)
        {
            const uint32 entityIndex = drawCall.batch->numEntities++;

            drawCall.batch->indices[entityIndex] = uint32(entityId.ToIndex());

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

                void* dstPtr = reinterpret_cast<void*>((uintptr_t(drawCall.batch)) + fieldOffset + (entityIndex * bufferStructSize));
                void* srcPtr = reinterpret_cast<void*>(uintptr_t(meshInstanceData.buffers[bufferIndex].Data()) + (instanceOffset * bufferStructSize));

                // sanity checks
                AssertDebug((uintptr_t(dstPtr) + bufferStructSize) - uintptr_t(drawCall.batch) <= batchSizeof,
                    "Buffer struct size is larger than batch size! Buffer struct size: %u, Buffer struct alignment: %u, Batch size: %u, Entity index: %u, Field offset: %u",
                    bufferStructSize, bufferStructAlignment, batchSizeof, entityIndex, fieldOffset);
                AssertDebug(meshInstanceData.buffers[bufferIndex].Size() >= (instanceOffset + 1) * bufferStructSize,
                    "Buffer size is not large enough to copy data! Buffer size: %u, Buffer struct size: %u, Instance offset: %u",
                    meshInstanceData.buffers[bufferIndex].Size(), bufferStructSize, instanceOffset);

                Memory::MemCpy(dstPtr, srcPtr, bufferStructSize);

                fieldOffset += maxEntitiesPerInstanceBatch * bufferStructSize;
            }

            instanceOffset++;

            drawCall.entityIds[drawCall.count++] = entityId;

            --numInstances;

            dirty = true;
        }
    }
    else
    {
        while (drawCall.batch->numEntities < maxEntitiesPerInstanceBatch && numInstances != 0)
        {
            const uint32 entityIndex = drawCall.batch->numEntities++;

            drawCall.batch->indices[entityIndex] = uint32(entityId.ToIndex());
            drawCall.batch->transforms[entityIndex] = Matrix4::identity;

            drawCall.entityIds[drawCall.count++] = entityId;

            --numInstances;

            dirty = true;
        }
    }

    if (dirty)
    {
        impl->GetGpuBufferHolder()->MarkDirty(drawCall.batch->batchIndex);
    }

    return numInstances;
}

#pragma endregion DrawCallCollection

#pragma region DrawCallCollectionImpl

static TypeMap<UniquePtr<IDrawCallCollectionImpl>> g_drawCallCollectionImplMap = {};
static Mutex g_drawCallCollectionImplMapMutex = {};

HYP_API IDrawCallCollectionImpl* GetDrawCallCollectionImpl(TypeId typeId)
{
    Mutex::Guard guard(g_drawCallCollectionImplMapMutex);

    auto it = g_drawCallCollectionImplMap.Find(typeId);

    return it != g_drawCallCollectionImplMap.End() ? it->second.Get() : nullptr;
}

HYP_API IDrawCallCollectionImpl* SetDrawCallCollectionImpl(TypeId typeId, UniquePtr<IDrawCallCollectionImpl>&& impl)
{
    Mutex::Guard guard(g_drawCallCollectionImplMapMutex);

    auto it = g_drawCallCollectionImplMap.Set(typeId, std::move(impl)).first;

    return it->second.Get();
}

#pragma endregion DrawCallCollectionImpl

} // namespace hyperion
