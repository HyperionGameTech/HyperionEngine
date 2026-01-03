/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/IndirectDraw.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/DrawCall.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/DepthPyramidRenderer.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/Frame.hpp>
#include <rendering/ComputePipeline.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/GpuBuffer.hpp>
#include <rendering/RenderHelpers.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/Mesh.hpp>

#include <rendering/renderers/EnvGridRenderer.hpp>
#include <rendering/renderers/EnvProbeRenderer.hpp>
#include <rendering/renderers/DeferredRenderer.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <scene/View.hpp>

#include <core/math/MathUtil.hpp>

namespace Hyperion {

static inline bool ResizeBuffer(
    Frame* frame,
    GpuBuffer* buffer,
    SizeType newBufferSize)
{
    if constexpr (IndirectDrawState::UseNextPow2Size)
    {
        newBufferSize = MathUtil::NextPowerOf2(newBufferSize);
    }

    bool sizeChanged = false;

    HYP_GFX_ASSERT(buffer->EnsureCapacity(newBufferSize, &sizeChanged));

    if (!buffer->IsCreated())
    {
        HYP_GFX_ASSERT(buffer->Create());

        sizeChanged = true;
    }

    return sizeChanged;
}

static bool ResizeIndirectDrawCommandsBuffer(
    Frame* frame,
    const TByteBuffer<RenderAllocator>& drawCommandsBuffer,
    GpuBuffer* indirectBuffer,
    GpuBuffer* stagingBuffer)
{
    const bool wasCreatedOrResized = ResizeBuffer(frame, indirectBuffer, drawCommandsBuffer.Size());

    if (!wasCreatedOrResized)
    {
        return false;
    }

    HYP_GFX_ASSERT(stagingBuffer->EnsureCapacity(indirectBuffer->Size()));

    // upload zeros to the buffer using a staging buffer.
    if (!stagingBuffer->IsCreated())
    {
        HYP_GFX_ASSERT(stagingBuffer->Create());
    }

    // set all to zero
    stagingBuffer->Memset(stagingBuffer->Size(), 0);

    frame->renderQueue << InsertBarrier(stagingBuffer, RS_COPY_SRC);
    frame->renderQueue << InsertBarrier(indirectBuffer, RS_COPY_DST);

    frame->renderQueue << CopyBuffer(stagingBuffer, indirectBuffer, stagingBuffer->Size());

    frame->renderQueue << InsertBarrier(indirectBuffer, RS_INDIRECT_ARG);

    return true;
}

static bool ResizeInstancesBuffer(
    Frame* frame,
    uint32 numObjectInstances,
    GpuBuffer* instanceBuffer,
    GpuBuffer* stagingBuffer)
{
    const bool wasCreatedOrResized = ResizeBuffer(
        frame,
        instanceBuffer,
        numObjectInstances * sizeof(ObjectInstance));

    if (wasCreatedOrResized)
    {
        instanceBuffer->Memset(instanceBuffer->Size(), 0);
    }

    return wasCreatedOrResized;
}

static bool ResizeIfNeeded(
    Frame* frame,
    const FixedArray<GpuBufferRef, NumFramesInFlight>& indirectBuffers,
    const FixedArray<GpuBufferRef, NumFramesInFlight>& instanceBuffers,
    const FixedArray<GpuBufferRef, NumFramesInFlight>& stagingBuffers,
    uint32 numObjectInstances,
    const TByteBuffer<RenderAllocator>& drawCommandsBuffer,
    uint8 dirtyBits)
{
    bool resizeHappened = false;

    GpuBuffer* indirectBuffer = indirectBuffers[frame->GetFrameIndex()];
    GpuBuffer* instanceBuffer = instanceBuffers[frame->GetFrameIndex()];
    GpuBuffer* stagingBuffer = stagingBuffers[frame->GetFrameIndex()];

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

#pragma region Render commands

#pragma endregion Render commands

#pragma region IndirectDrawState

static constexpr uint32 AllBitsDirty = (1u << NumFramesInFlight) - 1;

IndirectDrawState::IndirectDrawState()
    : m_numDrawCommands(0),
      m_dirtyBits(AllBitsDirty)
{
}

IndirectDrawState::~IndirectDrawState()
{
    SafeDelete(std::move(m_indirectBuffers));
    SafeDelete(std::move(m_instanceBuffers));
    SafeDelete(std::move(m_stagingBuffers));
}

void IndirectDrawState::Create()
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    TByteBuffer<RenderAllocator> drawCommandsBuffer;
    g_renderBackend->PopulateIndirectDrawCommandsBuffer(GpuBufferRef::Null(), GpuBufferRef::Null(), 0, drawCommandsBuffer);

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
    {
        m_instanceBuffers[frameIndex] = g_renderBackend->MakeGpuBuffer(GpuBufferType::SSBO, sizeof(ObjectInstance));
        m_instanceBuffers[frameIndex]->SetDebugName(NAME_FMT("IndirectDraw_InstancesBuffer_Frame{}", frameIndex));
        m_instanceBuffers[frameIndex]->SetRequireCpuAccessible(true);
        DeferCreate(m_instanceBuffers[frameIndex]);

        m_indirectBuffers[frameIndex] = g_renderBackend->MakeGpuBuffer(GpuBufferType::INDIRECT_ARGS_BUFFER, drawCommandsBuffer.Size());
        m_indirectBuffers[frameIndex]->SetDebugName(NAME_FMT("IndirectDraw_IndirectBuffer_Frame{}", frameIndex));
        DeferCreate(m_indirectBuffers[frameIndex]);

        m_stagingBuffers[frameIndex] = g_renderBackend->MakeGpuBuffer(GpuBufferType::STAGING_BUFFER, drawCommandsBuffer.Size());
        m_stagingBuffers[frameIndex]->SetDebugName(NAME_FMT("IndirectDraw_StagingBuffer_Frame{}", frameIndex));
        DeferCreate(m_stagingBuffers[frameIndex]);
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

    g_renderBackend->PopulateIndirectDrawCommandsBuffer(
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

    g_renderBackend->PopulateIndirectDrawCommandsBuffer(
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

        frame->renderQueue << InsertBarrier(stagingBuffer, RS_COPY_SRC);
        frame->renderQueue << InsertBarrier(indirectBuffer, RS_COPY_DST);

        frame->renderQueue << CopyBuffer(stagingBuffer, indirectBuffer, stagingBuffer->Size());

        frame->renderQueue << InsertBarrier(indirectBuffer, RS_INDIRECT_ARG);
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
    SafeDelete(std::move(m_objectVisibility));
}

void IndirectRenderer::Create(EntityBatchAllocatorBase* batchAllocator)
{
    Assert(batchAllocator != nullptr);
    m_batchAllocator = batchAllocator;

    m_indirectDrawState.Create();

    ShaderRef objectVisibilityShader = g_shaderManager->GetOrCreate(NAME("ObjectVisibility"));
    Assert(objectVisibilityShader.IsValid());

    DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(
        objectVisibilityShader->GetCompiledShader()->GetDescriptorTableDeclaration());

    Assert(batchAllocator != nullptr);

    GpuBufferHolderBase* entityInstanceBatches = batchAllocator->GetGpuBufferHolder();
    const SizeType batchStructSize = batchAllocator->GetStructSize();

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = descriptorTable->GetDescriptorSet("ObjectVisibilityDescriptorSet"_sh, frameIndex);
        Assert(descriptorSet != nullptr);

        auto* shaderBufferElement = descriptorSet->GetLayout().GetElement(NAME("EntityInstanceBatchesBuffer"));
        Assert(shaderBufferElement != nullptr);

        if (shaderBufferElement->size != ~0u)
        {
            // case 1: the EntityInstanceBatchesBuffer is an array of EntityInstanceBatch structs

            const SizeType shaderBufferSize = shaderBufferElement->size;

            if (shaderBufferSize >= batchStructSize)
            {
                const SizeType sizeMod = shaderBufferSize % batchStructSize;

                Assert(sizeMod == 0, "EntityInstanceBatchesBuffer descriptor has size {} but DrawCallCollection has batch struct size of {}",
                    shaderBufferSize, batchStructSize);
            }
            else
            {
                // case 2: packing the EntityInstanceBatch buffer data into scalar data
                AssertDebug(shaderBufferSize == 16, "Expected EntityInstanceBatchesBuffer descriptor to have size 16 (uvec4), but got {}", shaderBufferSize);
                AssertDebug(batchStructSize % 16 == 0, "Expected batch struct size to be divisible by 16!");
            }
        }

        descriptorSet->SetElement("ObjectInstancesBuffer"_sh, m_indirectDrawState.GetInstanceBuffer(frameIndex));
        descriptorSet->SetElement("IndirectDrawCommandsBuffer"_sh, m_indirectDrawState.GetIndirectBuffer(frameIndex));
        descriptorSet->SetElement("EntityInstanceBatchesBuffer"_sh, entityInstanceBatches->GetBuffer(frameIndex));
    }

    DeferCreate(descriptorTable);

    m_objectVisibility = g_renderBackend->MakeComputePipeline(objectVisibilityShader, descriptorTable);
    DeferCreate(m_objectVisibility);
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

    Assert(renderSetup.passData->cullData.depthPyramidImageView != nullptr);

    const uint32 frameIndex = frame->GetFrameIndex();

    Assert(m_indirectDrawState.GetIndirectBuffer(frameIndex).IsValid());
    Assert(m_indirectDrawState.GetIndirectBuffer(frameIndex)->Size() != 0);

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

    // if (m_cachedCullDataUpdatedBits & (1u << frameIndex)) {
    //     m_descriptorSets[frameIndex]->GetDescriptor(6)
    //         ->SetElementSRV(0, m_cachedCullData.depthPyramidImageView);

    //     m_descriptorSets[frameIndex]->ApplyUpdates();

    //     m_cachedCullDataUpdatedBits &= ~(1u << frameIndex);
    // }

    frame->renderQueue << BindDescriptorTable(
        m_objectVisibility->GetDescriptorTable(),
        m_objectVisibility,
        { { "Global"_sh, { { "CamerasBuffer"_sh, ShaderDataOffset<CameraShaderData>(renderSetup.view->GetCamera()) } } } },
        frameIndex);

    const uint32 viewDescriptorSetIndex = m_objectVisibility->GetDescriptorTable()->GetDescriptorSetIndex("View"_sh);

    if (viewDescriptorSetIndex != ~0u)
    {
        frame->renderQueue << BindDescriptorSet(
            renderSetup.passData->descriptorSets[frameIndex],
            m_objectVisibility,
            {},
            viewDescriptorSetIndex);
    }

    frame->renderQueue << InsertBarrier(m_indirectDrawState.GetIndirectBuffer(frameIndex), RS_INDIRECT_ARG);

    struct
    {
        Vec2u depthPyramidDimensions;
        uint32 batchOffset;
        uint32 numInstances;
        uint32 entityInstanceBatchStride;
    } pushConstants;

    AssertDebug(m_batchAllocator->GetStructSize() % 4 == 0);

    DeferredRendererPassData* pd = ObjCast<DeferredRendererPassData>(renderSetup.passData);
    Assert(pd != nullptr);

    pushConstants.depthPyramidDimensions = pd->depthPyramidRenderer->GetExtent();
    pushConstants.batchOffset = 0;
    pushConstants.numInstances = numInstances;
    pushConstants.entityInstanceBatchStride = ByteUtil::AlignAs(m_batchAllocator->GetStructSize(), m_batchAllocator->GetStructAlignment());

    m_objectVisibility->SetPushConstants(&pushConstants, sizeof(pushConstants));

    frame->renderQueue << BindComputePipeline(m_objectVisibility);

    frame->renderQueue << DispatchCompute(m_objectVisibility, Vec3u { numBatches, 1, 1 });
    frame->renderQueue << InsertBarrier(m_indirectDrawState.GetIndirectBuffer(frameIndex), RS_INDIRECT_ARG);
}

void IndirectRenderer::RebuildDescriptors(Frame* frame)
{
    HYP_SCOPE;

    const uint32 frameIndex = frame->GetFrameIndex();

    const DescriptorTableRef& descriptorTable = m_objectVisibility->GetDescriptorTable();

    const DescriptorSetRef& descriptorSet = descriptorTable->GetDescriptorSet("ObjectVisibilityDescriptorSet"_sh, frameIndex);
    Assert(descriptorSet != nullptr);

    descriptorSet->SetElement("ObjectInstancesBuffer"_sh, m_indirectDrawState.GetInstanceBuffer(frameIndex));
    descriptorSet->SetElement("IndirectDrawCommandsBuffer"_sh, m_indirectDrawState.GetIndirectBuffer(frameIndex));

    descriptorSet->Update();
}

#pragma endregion IndirectRenderer

} // namespace Hyperion
