/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/IndirectDraw.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/DrawCall.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/DepthPyramidRenderer.hpp>
#include <rendering/Frame.hpp>
#include <rendering/ComputePipeline.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/GpuBuffer.hpp>
#include <rendering/RenderHelpers.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/ShaderInstance.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/PlaceholderData.hpp>

#include <rendering/renderers/EnvProbeRenderer.hpp>
#include <rendering/renderers/DeferredRenderer.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <scene/View.hpp>

#include <Core/math/MathUtil.hpp>

namespace Hyperion {

struct alignas(16) ComputeVisibilityConstants
{
    Vec2u depthPyramidDimensions;
    uint32 batchOffset;
    uint32 numInstances;
    uint32 entityInstanceBatchStride;
};

static void ZeroizeBuffer(Frame* frame, GpuBuffer* stagingBuffer, GpuBuffer* dstBuffer)
{
    if (dstBuffer->IsCpuAccessible())
    {
        Assert(dstBuffer != nullptr);

        dstBuffer->Memset(dstBuffer->Size(), 0);

        return;
    }
    
    Assert(stagingBuffer != nullptr && dstBuffer != nullptr);

    CheckResult(stagingBuffer->EnsureCapacity(dstBuffer->Size()));

    // upload zeros to the buffer using a staging buffer.
    if (!stagingBuffer->IsCreated())
    {
        CheckResult(stagingBuffer->Create());
    }

    // set all to zero
    stagingBuffer->Memset(stagingBuffer->Size(), 0);

    RenderQueue& rq = frame->renderQueue;

    rq << InsertBarrier(stagingBuffer, RS_COPY_SRC);
    rq << InsertBarrier(dstBuffer, RS_COPY_DST);

    rq << CopyBuffer(stagingBuffer, dstBuffer, dstBuffer->Size());

    rq << InsertBarrier(dstBuffer, RS_INDIRECT_ARG);
}

static inline bool CreateOrResizeBuffer(
    Frame* frame,
    GpuBufferRef& buffer,
    SizeType newBufferSize)
{
    if constexpr (IndirectDrawState::UseNextPow2Size)
    {
        newBufferSize = MathUtil::NextPowerOf2(newBufferSize);
    }

    if (buffer && buffer->Size() < newBufferSize)
    {
        const GpuBufferType prevBufferType = buffer->GetBufferType();
        const bool prevWasCpuAccessible = buffer->IsCpuAccessible();

        EnqueueDeletion(std::move(buffer));
        buffer = g_renderInterface->MakeGpuBuffer(prevBufferType, newBufferSize);

        if (prevWasCpuAccessible)
        {
            buffer->SetRequireCpuAccessible(true);
        }

        CheckResult(buffer->Create());

        return true;
    }

    if (!buffer->IsCreated())
    {
        CheckResult(buffer->Create());

        return true;
    }

    return false;
}

static bool ResizeIndirectDrawCommandsBuffer(
    Frame* frame,
    const TByteBuffer<RenderAllocator>& drawCommandsBuffer,
    GpuBufferRef& indirectBuffer,
    GpuBuffer* stagingBuffer)
{
    RenderQueue& rq = frame->renderQueue;

    const bool wasCreatedOrResized = CreateOrResizeBuffer(frame, indirectBuffer, drawCommandsBuffer.Size());

    if (!wasCreatedOrResized)
    {
        return false;
    }

    ZeroizeBuffer(frame, stagingBuffer, indirectBuffer);

    return true;
}

static bool ResizeInstancesBuffer(
    Frame* frame,
    uint32 numObjectInstances,
    GpuBufferRef& instanceBuffer,
    GpuBuffer* stagingBuffer)
{
    const bool wasCreatedOrResized = CreateOrResizeBuffer(
        frame,
        instanceBuffer,
        numObjectInstances * sizeof(ObjectInstance));

    if (wasCreatedOrResized)
    {
        ZeroizeBuffer(frame, stagingBuffer, instanceBuffer);
    }

    return wasCreatedOrResized;
}

static bool ResizeIfNeeded(
    Frame* frame,
    FixedArray<GpuBufferRef, NumFramesInFlight>& indirectBuffers,
    FixedArray<GpuBufferRef, NumFramesInFlight>& instanceBuffers,
    FixedArray<GpuBufferRef, NumFramesInFlight>& stagingBuffers,
    uint32 numObjectInstances,
    const TByteBuffer<RenderAllocator>& drawCommandsBuffer,
    uint8 dirtyBits)
{
    bool resizeHappened = false;

    GpuBufferRef& indirectBuffer = indirectBuffers[frame->GetFrameIndex()];
    GpuBufferRef& instanceBuffer = instanceBuffers[frame->GetFrameIndex()];
    GpuBufferRef& stagingBuffer = stagingBuffers[frame->GetFrameIndex()];

    if ((dirtyBits & (1u << frame->GetFrameIndex())) || !indirectBuffer)
    {
        resizeHappened |= ResizeIndirectDrawCommandsBuffer(frame, drawCommandsBuffer, indirectBuffer, stagingBuffer);
    }

    if ((dirtyBits & (1u << frame->GetFrameIndex())) || !instanceBuffer)
    {
        resizeHappened |= ResizeInstancesBuffer(frame, numObjectInstances, instanceBuffer, stagingBuffer);
    }

    return resizeHappened;
}

#pragma region IndirectDrawState

static constexpr uint8 AllBitsDirty = (1u << NumFramesInFlight) - 1;

IndirectDrawState::IndirectDrawState()
    : m_numDrawCommands(0),
      m_dirtyBits(AllBitsDirty)
{
}

IndirectDrawState::~IndirectDrawState()
{
    EnqueueDeletion(std::move(m_indirectBuffers));
    EnqueueDeletion(std::move(m_instanceBuffers));
    EnqueueDeletion(std::move(m_stagingBuffers));
}

void IndirectDrawState::Create()
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    TByteBuffer<RenderAllocator> drawCommandsBuffer;
    g_renderInterface->PopulateIndirectDrawCommandsBuffer(GpuBufferRef::Null(), GpuBufferRef::Null(), 0, drawCommandsBuffer);

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
    {
        m_instanceBuffers[frameIndex] = g_renderInterface->MakeGpuBuffer(GpuBufferType::STORAGE_BUFFER, sizeof(ObjectInstance));
        m_instanceBuffers[frameIndex]->SetRequireCpuAccessible(true);
#ifdef HYP_DEBUG_MODE
        m_instanceBuffers[frameIndex]->SetDebugName(NAME_FMT("IndirectDraw_InstancesBuffer_Frame{}", frameIndex));
#endif
        CheckResult(m_instanceBuffers[frameIndex]->Create());

        m_indirectBuffers[frameIndex] = g_renderInterface->MakeGpuBuffer(GpuBufferType::INDIRECT_ARGS_BUFFER, drawCommandsBuffer.Size());
#ifdef HYP_DEBUG_MODE
        m_indirectBuffers[frameIndex]->SetDebugName(NAME_FMT("IndirectDraw_IndirectBuffer_Frame{}", frameIndex));
#endif

        CheckResult(m_indirectBuffers[frameIndex]->Create());

        m_stagingBuffers[frameIndex] = g_renderInterface->MakeGpuBuffer(GpuBufferType::STAGING_BUFFER, drawCommandsBuffer.Size());
#ifdef HYP_DEBUG_MODE
        m_stagingBuffers[frameIndex]->SetDebugName(NAME_FMT("IndirectDraw_StagingBuffer_Frame{}", frameIndex));
#endif

        CheckResult(m_stagingBuffers[frameIndex]->Create());
    }
}

void IndirectDrawState::PushDrawCall(SizeType drawCallIndex, const DrawCallStorage& drawCalls, DrawCommandData& out)
{
    HYP_SCOPE;

    out = {};

    const uint32 drawCommandIndex = m_numDrawCommands++;

    ObjectInstance& instance = m_objectInstances.EmplaceBack();
    instance.entityId = drawCalls.entityIds[drawCallIndex].Value();
    instance.drawCommandIndex = drawCommandIndex;
    instance.batchIndex = ~0u;

    out.drawCommandIndex = drawCommandIndex;

    g_renderInterface->PopulateIndirectDrawCommandsBuffer(
        drawCalls.meshes[drawCallIndex]->GetVertexBuffer(),
        drawCalls.meshes[drawCallIndex]->GetIndexBuffer(),
        drawCommandIndex,
        m_drawCommandsBuffer);

    m_dirtyBits = AllBitsDirty;
}

void IndirectDrawState::PushInstancedDrawCall(SizeType drawCallIndex, const InstancedDrawCallStorage& drawCalls, DrawCommandData& out)
{
    out = {};

    const uint32 drawCommandIndex = m_numDrawCommands++;

    const uint32 count = drawCalls.counts[drawCallIndex];
    const FixedArray<ObjId<Entity>, MaxEntitiesPerBatch>& entityIds = drawCalls.entityIds[drawCallIndex];
    EntityInstanceBatch* batch = drawCalls.batches[drawCallIndex];

    for (uint32 index = 0; index < count; index++)
    {
        ObjectInstance& instance = m_objectInstances.EmplaceBack();
        instance.entityId = entityIds[index].Value();
        instance.drawCommandIndex = drawCommandIndex;
        instance.batchIndex = batch->batchIndex;
    }

    out.drawCommandIndex = drawCommandIndex;

    g_renderInterface->PopulateIndirectDrawCommandsBuffer(
        drawCalls.meshes[drawCallIndex]->GetVertexBuffer(),
        drawCalls.meshes[drawCallIndex]->GetIndexBuffer(),
        drawCommandIndex,
        m_drawCommandsBuffer);

    m_dirtyBits = AllBitsDirty;
}

void IndirectDrawState::ResetDrawState()
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    m_numDrawCommands = 0;

    m_objectInstances.Clear();

    // use SetSize() to keep the memory allocated
    m_drawCommandsBuffer.SetSize(0);

    m_dirtyBits = AllBitsDirty;
}

void IndirectDrawState::UpdateBufferData(Frame* frame, bool* outWasResized)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    const uint32 frameIndex = frame->GetFrameIndex();

    RenderQueue& rq = frame->renderQueue;

    if ((*outWasResized = ResizeIfNeeded(
             frame,
             m_indirectBuffers,
             m_instanceBuffers,
             m_stagingBuffers,
             m_objectInstances.Size(),
             m_drawCommandsBuffer,
             m_dirtyBits)))
    {
        m_dirtyBits |= (1u << frameIndex);
    }

    if (!(m_dirtyBits & (1u << frameIndex)))
    {
        return;
    }

    GpuBuffer* instanceBuffer = m_instanceBuffers[frameIndex];
    GpuBuffer* indirectBuffer = m_indirectBuffers[frameIndex];
    GpuBuffer* stagingBuffer = m_stagingBuffers[frameIndex];

    // fill instances buffer with data of the meshes
    {
        Assert(stagingBuffer != nullptr);
        Assert(stagingBuffer->Size() >= m_drawCommandsBuffer.Size());

        stagingBuffer->Copy(m_drawCommandsBuffer.Size(), m_drawCommandsBuffer.Data());

        rq << InsertBarrier(stagingBuffer, RS_COPY_SRC);
        rq << InsertBarrier(indirectBuffer, RS_COPY_DST);

        rq << CopyBuffer(stagingBuffer, indirectBuffer, stagingBuffer->Size());

        rq << InsertBarrier(indirectBuffer, RS_INDIRECT_ARG);
    }

    Assert(instanceBuffer->Size() >= m_objectInstances.Size() * sizeof(ObjectInstance));

    // update data for object instances (cpu - gpu)
    instanceBuffer->Copy(m_objectInstances.Size() * sizeof(ObjectInstance), m_objectInstances.Data());

    m_dirtyBits &= ~(1u << frameIndex);
}

#pragma endregion IndirectDrawState

#pragma region IndirectRenderer

IndirectRenderer::IndirectRenderer()
    : m_cachedCullDataUpdatedBits(0x0),
      m_batchAllocator(nullptr)
{
}

IndirectRenderer::~IndirectRenderer()
{
    EnqueueDeletion(std::move(m_cBuffers));
}

void IndirectRenderer::Create(EntityBatchAllocatorBase* batchAllocator)
{
    Assert(batchAllocator != nullptr);
    m_batchAllocator = batchAllocator;

    m_indirectDrawState.Create();

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
    {
        m_cBuffers[frameIndex] = g_renderInterface->MakeGpuBuffer(GpuBufferType::CONSTANT_BUFFER, sizeof(ComputeVisibilityConstants));
#ifdef HYP_DEBUG_MODE
        m_cBuffers[frameIndex]->SetDebugName(NAME_FMT("IndirectRenderer_UniformBuffer_Frame{}", frameIndex));
#endif

        CheckResult(m_cBuffers[frameIndex]->Create());
    }
}

void IndirectRenderer::PushDrawCallsToIndirectState(DrawCallCollection& drawCallCollection)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    for (SizeType i = 0; i < drawCallCollection.drawCalls.Size(); i++)
    {
        DrawCommandData drawCommandData;
        m_indirectDrawState.PushDrawCall(i, drawCallCollection.drawCalls, drawCommandData);

        drawCallCollection.drawCalls.drawCommandIndices[i] = drawCommandData.drawCommandIndex;
    }

    for (SizeType i = 0; i < drawCallCollection.instancedDrawCalls.Size(); i++)
    {
        DrawCommandData drawCommandData;
        m_indirectDrawState.PushInstancedDrawCall(i, drawCallCollection.instancedDrawCalls, drawCommandData);

        drawCallCollection.instancedDrawCalls.drawCommandIndices[i] = drawCommandData.drawCommandIndex;
    }
}

void IndirectRenderer::ExecuteCullShaderInBatches(Frame* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.world && renderSetup.view);
    AssertDebug(renderSetup.passData != nullptr);

    AssertDebug(m_batchAllocator != nullptr);

    AssertDebug(renderSetup.passData->cullData.depthPyramidImageView != nullptr);

    const uint32 frameIndex = frame->GetFrameIndex();

    RenderQueue& rq = frame->renderQueue;

    AssertDebug(m_indirectDrawState.GetIndirectBuffer(frameIndex).IsValid());
    AssertDebug(m_indirectDrawState.GetIndirectBuffer(frameIndex)->Size() != 0);

    const SizeType numInstances = m_indirectDrawState.GetInstances().Size();
    const uint32 numBatches = (uint32(numInstances) / IndirectDrawState::BatchSize) + 1;

    if (numInstances == 0)
    {
        return;
    }

    {
        bool wasBufferResized = false;
        m_indirectDrawState.UpdateBufferData(frame, &wasBufferResized);

        if (wasBufferResized)
        {
            RebuildDescriptors(frame);
        }
    }

    if (m_cachedCullData != renderSetup.passData->cullData)
    {
        m_cachedCullData = renderSetup.passData->cullData;
        m_cachedCullDataUpdatedBits = 0xFF;
    }

    DeferredRendererPassData* pd = ObjCast<DeferredRendererPassData>(renderSetup.passData);
    AssertDebug(pd != nullptr);

    uint32 numShaderUniforms = 0;

    rq << SetCurrentShader(ShaderDesc(NAME("ComputeVisibility")));

    rq << SetShaderUniform(numShaderUniforms++, "CamerasBuffer"_sh, g_renderInterface->gpuBuffers[GRB_CAMERAS]->GetBuffer(frameIndex), TShaderDataOffset<CameraShaderData>(renderSetup.view->GetCamera()));
    rq << SetShaderUniform(numShaderUniforms++, "EntitiesBuffer"_sh, g_renderInterface->gpuBuffers[GRB_ENTITIES]->GetBuffer(frameIndex));
    rq << SetShaderUniform(numShaderUniforms++, "WorldsBuffer"_sh, g_renderInterface->gpuBuffers[GRB_WORLDS]->GetBuffer(frameIndex));
    
    rq << SetShaderUniform(numShaderUniforms++, "SamplerNearest"_sh, g_renderInterface->placeholderData->GetSamplerNearest());
    rq << SetShaderUniform(numShaderUniforms++, "DepthPyramidResult"_sh, renderSetup.passData->cullData.depthPyramidImageView);

    rq << SetShaderUniform(numShaderUniforms++, "ObjectInstancesBuffer"_sh, m_indirectDrawState.GetInstanceBuffer(frameIndex));
    rq << SetShaderUniform(numShaderUniforms++, "IndirectDrawCommandsBuffer"_sh, m_indirectDrawState.GetIndirectBuffer(frameIndex));
    rq << SetShaderUniform(numShaderUniforms++, "EntityInstanceBatchesBuffer"_sh, m_batchAllocator->GetGpuBufferHolder()->GetBuffer(frameIndex));

    ComputeVisibilityConstants constants {};
    constants.depthPyramidDimensions = pd->depthPyramidRenderer->GetExtent();
    constants.batchOffset = 0;
    constants.numInstances = numInstances;
    constants.entityInstanceBatchStride = ByteUtil::AlignAs(m_batchAllocator->GetStructSize(), m_batchAllocator->GetStructAlignment());

    m_cBuffers[frameIndex]->Copy(sizeof(constants), &constants);

    rq << SetShaderUniform(numShaderUniforms++, "ComputeVisibilityConstants"_sh, m_cBuffers[frameIndex]);

    rq << InsertBarrier(m_indirectDrawState.GetIndirectBuffer(frameIndex), RS_INDIRECT_ARG);

    rq << DispatchCompute(Vec3u { numBatches, 1, 1 });
    
    rq << InsertBarrier(m_indirectDrawState.GetIndirectBuffer(frameIndex), RS_INDIRECT_ARG);
}

void IndirectRenderer::RebuildDescriptors(Frame* frame)
{
}

#pragma endregion IndirectRenderer

} // namespace Hyperion
