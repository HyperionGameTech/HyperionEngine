/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/InstanceDataPool.hpp>
#include <Rendering/RenderInterface.hpp>
#include <Rendering/RenderProxy.hpp>

#include <Core/Math/BoundingSphere.hpp>
#include <Core/Math/MathUtil.hpp>

#include <Core/Utilities/ByteUtil.hpp>

#include <Core/Threading/Threads.hpp>

#include <Core/Logging/Logger.hpp>

#include <Framework/EngineStats.hpp>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Rendering);

EngineStatCounter<uint32> g_statInstanceSlotsInUse("Rendering/Instancing/SlotsInUse", false);
EngineStatCounter<uint32> g_statInstancesUploaded("Rendering/Instancing/InstancesUploaded");

InstanceDataPool::InstanceDataPool()
    : m_slotAllocator(IdentityInstanceSlot + 1, MaxInstanceDataSlots - (IdentityInstanceSlot + 1))
{
    m_entitySlots.Resize(MaxBoundEntities);
}

InstanceDataPool::~InstanceDataPool()
{
    g_statInstanceSlotsInUse -= m_slotAllocator.NumAllocated();
}

void InstanceDataPool::Initialize()
{
    AssertOnThread(g_renderThread);

    const InstanceTransformShaderData identity { Mat4f::Identity(), Mat4f::Identity() };

    RI.namedBuffers[NamedBuffer::InstanceData].Write(
        IdentityInstanceSlot * sizeof(InstanceTransformShaderData),
        sizeof(InstanceTransformShaderData),
        &identity);
}

void InstanceDataPool::ReleaseSlots(EntityInstanceSlots& slots)
{
    if (slots.IsValid())
    {
        m_slotAllocator.Free(slots.base, slots.capacity);

        g_statInstanceSlotsInUse -= slots.capacity;
    }

    slots.base = ~0u;
    slots.count = 0;
    slots.capacity = 0;
    slots.boundingSpheres.Clear();
}

void InstanceDataPool::WriteInstances(uint32 entityBinding, const RenderProxyMesh& proxy)
{
    AssertOnThread(g_renderThread);

    if (entityBinding >= MaxBoundEntities)
    {
        return;
    }

    EntityInstanceSlots& slots = m_entitySlots[entityBinding];

    const InstanceData& instanceData = proxy.instanceData;
    const uint32 numInstances = proxy.numInstances;

    // buffer 0 is always the instance transform
    // FIXME: flimsy
    const bool hasTransforms = numInstances != 0
        && instanceData.bufferStructSizes[0] == sizeof(Mat4f)
        && instanceData.buffers[0].Size() >= numInstances * sizeof(Mat4f);

    if (!hasTransforms)
    {
        ReleaseSlots(slots);

        return;
    }

    // where each buffer's element goes within a slot
    uint32 packedOffsets[InstanceData::MaxBuffers];
    uint32 packedSize = 0;

    for (uint32 bufferIndex = 0; bufferIndex < InstanceData::MaxBuffers; bufferIndex++)
    {
        const uint32 structSize = instanceData.bufferStructSizes[bufferIndex];

        if (structSize == 0)
        {
            packedOffsets[bufferIndex] = ~0u;

            continue;
        }

        if (instanceData.buffers[bufferIndex].Size() < numInstances * structSize)
        {
            HYP_LOG_ONCE(Rendering, Error, "Instance data buffer {} holds fewer than {} instances, skipping instanced entity", bufferIndex, numInstances);

            ReleaseSlots(slots);

            return;
        }

        packedSize = ByteUtil::AlignAs(packedSize, MathUtil::Max(instanceData.bufferStructAlignments[bufferIndex], 1u));
        packedOffsets[bufferIndex] = packedSize;
        packedSize += structSize;
    }

    const bool duplicateTransform = packedSize == sizeof(Mat4f);

    if (packedSize > sizeof(InstanceTransformShaderData))
    {
        HYP_LOG_ONCE(Rendering, Error, "Instance data is {} bytes per instance, more than the {} an instance slot holds, skipping instanced entity", packedSize, sizeof(InstanceTransformShaderData));

        ReleaseSlots(slots);

        return;
    }

    if (!slots.IsValid() || slots.capacity < numInstances)
    {
        ReleaseSlots(slots);

        // room to grow, so adding a few instances doesn't move the whole range
        const uint32 capacity = numInstances == 1 ? 1u : uint32(MathUtil::NextPowerOf2(numInstances));

        slots.base = m_slotAllocator.Allocate(capacity);

        if (!slots.IsValid())
        {
            HYP_LOG_ONCE(Rendering, Error, "Instance data buffer is full ({} slots), some instanced entities aren't drawn. Consider increasing MaxInstanceDataSlots.", MaxInstanceDataSlots);

            return;
        }

        slots.capacity = capacity;

        g_statInstanceSlotsInUse += capacity;
    }

    slots.count = numInstances;

    const Mat4f& modelMatrix = proxy.bufferData.modelMatrix;
    const bool hasMeshBounds = proxy.meshAabb.IsValid();

    Array<InstanceTransformShaderData, RenderAllocator> shaderData;
    shaderData.Resize(numInstances);

    Memory::Zero(shaderData.Data(), shaderData.ByteSize());

    slots.boundingSpheres.Resize(numInstances);

    for (uint32 instanceIndex = 0; instanceIndex < numInstances; instanceIndex++)
    {
        ubyte* slotData = reinterpret_cast<ubyte*>(&shaderData[instanceIndex]);

        for (uint32 bufferIndex = 0; bufferIndex < InstanceData::MaxBuffers; bufferIndex++)
        {
            if (packedOffsets[bufferIndex] == ~0u)
            {
                continue;
            }

            const uint32 structSize = instanceData.bufferStructSizes[bufferIndex];

            Memory::Copy(slotData + packedOffsets[bufferIndex], instanceData.buffers[bufferIndex].Data() + instanceIndex * structSize, structSize);
        }

        if (duplicateTransform)
        {
            shaderData[instanceIndex].previousTransform = shaderData[instanceIndex].transform;
        }

        if (hasMeshBounds)
        {
            const BoundingSphere boundingSphere { (modelMatrix * shaderData[instanceIndex].transform) * proxy.meshAabb };

            slots.boundingSpheres[instanceIndex] = Vec4f(boundingSphere.center, boundingSphere.radius);
        }
        else
        {
            // no bounds to cull or pick a LOD with; a negative radius keeps it always visible
            slots.boundingSpheres[instanceIndex] = Vec4f(0.0f, 0.0f, 0.0f, -1.0f);
        }
    }

    RI.namedBuffers[NamedBuffer::InstanceData].Write(
        slots.base * sizeof(InstanceTransformShaderData),
        numInstances * sizeof(InstanceTransformShaderData),
        shaderData.Data());

    g_statInstancesUploaded += numInstances;
}

void InstanceDataPool::ReleaseInstances(uint32 entityBinding)
{
    AssertOnThread(g_renderThread);

    if (entityBinding >= MaxBoundEntities)
    {
        return;
    }

    ReleaseSlots(m_entitySlots[entityBinding]);
}

const EntityInstanceSlots* InstanceDataPool::GetSlots(uint32 entityBinding) const
{
    AssertOnThread(g_renderThread);

    if (entityBinding >= MaxBoundEntities)
    {
        return nullptr;
    }

    const EntityInstanceSlots& slots = m_entitySlots[entityBinding];

    return slots.IsValid() ? &slots : nullptr;
}

} // namespace Hyperion
