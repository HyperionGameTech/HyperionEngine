/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/IndirectDraw.hpp>
#include <rendering/DrawCall.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/DepthPyramidRenderer.hpp>
#include <rendering/env_grid/EnvGridRenderer.hpp>
#include <rendering/env_probe/EnvProbeRenderer.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/RenderBackend.hpp>

#include <rendering/RenderFrame.hpp>
#include <rendering/RenderComputePipeline.hpp>
#include <rendering/RenderGpuBuffer.hpp>
#include <rendering/RenderHelpers.hpp>

#include <rendering/Mesh.hpp>

#include <scene/View.hpp>

#include <core/math/MathUtil.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <engine/EngineGlobals.hpp>

namespace hyperion {

static bool ResizeBuffer(FrameBase* frame, GpuBufferBase* buffer, SizeType newBufferSize)
{
    if constexpr (IndirectDrawState::useNextPow2Size)
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
    FrameBase* frame,
    const ByteBuffer& drawCommandsBuffer,
    GpuBufferBase* indirectBuffer,
    GpuBufferBase* stagingBuffer)
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
    FrameBase* frame,
    uint32 numObjectInstances,
    GpuBufferBase* instanceBuffer,
    GpuBufferBase* stagingBuffer)
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
    FrameBase* frame,
    const FixedArray<GpuBufferRef, g_framesInFlight>& indirectBuffers,
    const FixedArray<GpuBufferRef, g_framesInFlight>& instanceBuffers,
    const FixedArray<GpuBufferRef, g_framesInFlight>& stagingBuffers,
    uint32 numObjectInstances,
    const ByteBuffer& drawCommandsBuffer,
    uint8 dirtyBits)
{
    bool resizeHappened = false;

    GpuBufferBase* indirectBuffer = indirectBuffers[frame->GetFrameIndex()];
    GpuBufferBase* instanceBuffer = instanceBuffers[frame->GetFrameIndex()];
    GpuBufferBase* stagingBuffer = stagingBuffers[frame->GetFrameIndex()];

    if ((dirtyBits & (1u << frame->GetFrameIndex())) || !indirectBuffers[frame->GetFrameIndex()].IsValid())
    {
        resizeHappened |= ResizeIndirectDrawCommandsBuffer(frame, drawCommandsBuffer, indirectBuffer, stagingBuffer);
    }

    if ((dirtyBits & (1u << frame->GetFrameIndex())) || !instanceBuffers[frame->GetFrameIndex()].IsValid())
    {
        resizeHappened |= ResizeInstancesBuffer(frame, numObjectInstances, instanceBuffer, stagingBuffer);
    }

    return resizeHappened;
}

#pragma region Render commands

#pragma endregion Render commands

#pragma region IndirectDrawState

IndirectDrawState::IndirectDrawState()
    : m_numDrawCommands(0),
      m_dirtyBits(0x3)
{
}

IndirectDrawState::~IndirectDrawState()
{
    SafeRelease(std::move(m_indirectBuffers));
    SafeRelease(std::move(m_instanceBuffers));
    SafeRelease(std::move(m_stagingBuffers));
}

void IndirectDrawState::Create()
{

    ByteBuffer drawCommandsBuffer;
    g_renderBackend->PopulateIndirectDrawCommandsBuffer(GpuBufferRef::Null(), GpuBufferRef::Null(), 0, drawCommandsBuffer);

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        m_instanceBuffers[frameIndex] = g_renderBackend->MakeGpuBuffer(GpuBufferType::SSBO, sizeof(ObjectInstance));
        DeferCreate(m_instanceBuffers[frameIndex]);

        m_indirectBuffers[frameIndex] = g_renderBackend->MakeGpuBuffer(GpuBufferType::INDIRECT_ARGS_BUFFER, drawCommandsBuffer.Size());
        DeferCreate(m_indirectBuffers[frameIndex]);

        m_stagingBuffers[frameIndex] = g_renderBackend->MakeGpuBuffer(GpuBufferType::STAGING_BUFFER, drawCommandsBuffer.Size());
        DeferCreate(m_stagingBuffers[frameIndex]);
    }
}

void IndirectDrawState::PushDrawCall(const DrawCall& drawCall, DrawCommandData& out)
{
    out = {};

    const uint32 drawCommandIndex = m_numDrawCommands++;

    ObjectInstance& instance = m_objectInstances.EmplaceBack();
    instance.entityId = drawCall.entityId.Value();
    instance.drawCommandIndex = drawCommandIndex;
    instance.batchIndex = ~0u;

    out.drawCommandIndex = drawCommandIndex;

    g_renderBackend->PopulateIndirectDrawCommandsBuffer(
        drawCall.mesh->GetVertexBuffer(),
        drawCall.mesh->GetIndexBuffer(),
        drawCommandIndex,
        m_drawCommandsBuffer);

    m_dirtyBits |= 0x3;
}

void IndirectDrawState::PushInstancedDrawCall(const InstancedDrawCall& drawCall, DrawCommandData& out)
{
    out = {};

    const uint32 drawCommandIndex = m_numDrawCommands++;

    for (uint32 index = 0; index < drawCall.count; index++)
    {
        ObjectInstance& instance = m_objectInstances.EmplaceBack();
        instance.entityId = drawCall.entityIds[index].Value();
        instance.drawCommandIndex = drawCommandIndex;
        instance.batchIndex = drawCall.batch->batchIndex;
    }

    out.drawCommandIndex = drawCommandIndex;

    g_renderBackend->PopulateIndirectDrawCommandsBuffer(
        drawCall.mesh->GetVertexBuffer(),
        drawCall.mesh->GetIndexBuffer(),
        drawCommandIndex,
        m_drawCommandsBuffer);

    m_dirtyBits |= 0x3;
}

void IndirectDrawState::ResetDrawState()
{
    Threads::AssertOnThread(g_renderThread | ThreadCategory::THREAD_CATEGORY_TASK);

    m_numDrawCommands = 0;

    m_objectInstances.Clear();

    // use SetSize() to keep the memory allocated
    m_drawCommandsBuffer.SetSize(0);

    m_dirtyBits |= 0x3;
}

void IndirectDrawState::UpdateBufferData(FrameBase* frame, bool* outWasResized)
{
    Threads::AssertOnThread(g_renderThread | ThreadCategory::THREAD_CATEGORY_TASK);

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

    // fill instances buffer with data of the meshes
    {
        Assert(m_stagingBuffers[frameIndex].IsValid());
        Assert(m_stagingBuffers[frameIndex]->Size() >= m_drawCommandsBuffer.Size());

        m_stagingBuffers[frameIndex]->Copy(m_drawCommandsBuffer.Size(), m_drawCommandsBuffer.Data());

        frame->renderQueue << InsertBarrier(m_stagingBuffers[frameIndex], RS_COPY_SRC);
        frame->renderQueue << InsertBarrier(m_indirectBuffers[frameIndex], RS_COPY_DST);

        frame->renderQueue << CopyBuffer(m_stagingBuffers[frameIndex], m_indirectBuffers[frameIndex], m_stagingBuffers[frameIndex]->Size());

        frame->renderQueue << InsertBarrier(m_indirectBuffers[frameIndex], RS_INDIRECT_ARG);
    }

    Assert(m_instanceBuffers[frameIndex]->Size() >= m_objectInstances.Size() * sizeof(ObjectInstance));

    // update data for object instances (cpu - gpu)
    m_instanceBuffers[frameIndex]->Copy(m_objectInstances.Size() * sizeof(ObjectInstance), m_objectInstances.Data());

    m_dirtyBits &= ~(1u << frameIndex);
}

#pragma endregion IndirectDrawState

#pragma region IndirectRenderer

IndirectRenderer::IndirectRenderer()
    : m_cachedCullDataUpdatedBits(0x0),
      m_drawCallCollectionImpl(nullptr)
{
}

IndirectRenderer::~IndirectRenderer()
{
    SafeRelease(std::move(m_objectVisibility));
}

void IndirectRenderer::Create(IDrawCallCollectionImpl* impl)
{
    Assert(impl != nullptr);
    m_drawCallCollectionImpl = impl;

    m_indirectDrawState.Create();

    ShaderRef objectVisibilityShader = g_shaderManager->GetOrCreate(NAME("ObjectVisibility"));
    Assert(objectVisibilityShader.IsValid());

    const DescriptorTableDeclaration& descriptorTableDecl = objectVisibilityShader->GetCompiledShader()->GetDescriptorTableDeclaration();

    DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(&descriptorTableDecl);

    Assert(impl != nullptr);

    GpuBufferHolderBase* entityInstanceBatches = impl->GetGpuBufferHolder();
    const SizeType batchSizeof = impl->GetStructSize();

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = descriptorTable->GetDescriptorSet("ObjectVisibilityDescriptorSet", frameIndex);
        Assert(descriptorSet != nullptr);

        auto* shaderBufferElement = descriptorSet->GetLayout().GetElement(NAME("EntityInstanceBatchesBuffer"));
        Assert(shaderBufferElement != nullptr);

        if (shaderBufferElement->size != ~0u)
        {
            // case 1: the EntityInstanceBatchesBuffer is an array of EntityInstanceBatch structs

            const SizeType shaderBufferSize = shaderBufferElement->size;

            if (shaderBufferSize >= batchSizeof)
            {
                const SizeType sizeMod = shaderBufferSize % batchSizeof;

                Assert(sizeMod == 0, "EntityInstanceBatchesBuffer descriptor has size {} but DrawCallCollection has batch struct size of {}",
                    shaderBufferSize, batchSizeof);
            }
            else
            {
                // case 2: packing the EntityInstanceBatch buffer data into scalar data
                AssertDebug(shaderBufferSize == 16, "Expected EntityInstanceBatchesBuffer descriptor to have size 16 (uvec4), but got {}", shaderBufferSize);
                AssertDebug(batchSizeof % 16 == 0, "Expected batch struct size to be divisible by 16!");
            }
        }

        descriptorSet->SetElement("ObjectInstancesBuffer", m_indirectDrawState.GetInstanceBuffer(frameIndex));
        descriptorSet->SetElement("IndirectDrawCommandsBuffer", m_indirectDrawState.GetIndirectBuffer(frameIndex));
        descriptorSet->SetElement("EntityInstanceBatchesBuffer", entityInstanceBatches->GetBuffer(frameIndex));
    }

    DeferCreate(descriptorTable);

    m_objectVisibility = g_renderBackend->MakeComputePipeline(objectVisibilityShader, descriptorTable);
    DeferCreate(m_objectVisibility);
}

void IndirectRenderer::PushDrawCallsToIndirectState(DrawCallCollection& drawCallCollection)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread | ThreadCategory::THREAD_CATEGORY_TASK);

    for (DrawCall& drawCall : drawCallCollection.drawCalls)
    {
        DrawCommandData drawCommandData;
        m_indirectDrawState.PushDrawCall(drawCall, drawCommandData);

        drawCall.drawCommandIndex = drawCommandData.drawCommandIndex;
    }

    for (InstancedDrawCall& drawCall : drawCallCollection.instancedDrawCalls)
    {
        DrawCommandData drawCommandData;
        m_indirectDrawState.PushInstancedDrawCall(drawCall, drawCommandData);

        drawCall.drawCommandIndex = drawCommandData.drawCommandIndex;
    }
}

void IndirectRenderer::ExecuteCullShaderInBatches(FrameBase* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.IsValid());
    AssertDebug(renderSetup.HasView());
    AssertDebug(renderSetup.passData != nullptr);

    AssertDebug(m_drawCallCollectionImpl != nullptr);

    Assert(renderSetup.passData->cullData.depthPyramidImageView != nullptr);

    const uint32 frameIndex = frame->GetFrameIndex();

    Assert(m_indirectDrawState.GetIndirectBuffer(frameIndex).IsValid());
    Assert(m_indirectDrawState.GetIndirectBuffer(frameIndex)->Size() != 0);

    const SizeType numInstances = m_indirectDrawState.GetInstances().Size();
    const uint32 numBatches = (uint32(numInstances) / IndirectDrawState::batchSize) + 1;

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
        { { "Global", { { "CamerasBuffer", ShaderDataOffset<CameraShaderData>(renderSetup.view->GetCamera()) } } } },
        frameIndex);

    const uint32 viewDescriptorSetIndex = m_objectVisibility->GetDescriptorTable()->GetDescriptorSetIndex("View");

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

    AssertDebug(m_drawCallCollectionImpl->GetStructSize() % 4 == 0);

    DeferredPassData* pd = ObjCast<DeferredPassData>(renderSetup.passData);
    Assert(pd != nullptr);

    pushConstants.depthPyramidDimensions = pd->depthPyramidRenderer->GetExtent();
    pushConstants.batchOffset = 0;
    pushConstants.numInstances = numInstances;
    pushConstants.entityInstanceBatchStride = ByteUtil::AlignAs(m_drawCallCollectionImpl->GetStructSize(), m_drawCallCollectionImpl->GetStructAlignment());

    m_objectVisibility->SetPushConstants(&pushConstants, sizeof(pushConstants));

    frame->renderQueue << BindComputePipeline(m_objectVisibility);

    frame->renderQueue << DispatchCompute(m_objectVisibility, Vec3u { numBatches, 1, 1 });
    frame->renderQueue << InsertBarrier(m_indirectDrawState.GetIndirectBuffer(frameIndex), RS_INDIRECT_ARG);
}

void IndirectRenderer::RebuildDescriptors(FrameBase* frame)
{
    HYP_SCOPE;

    const uint32 frameIndex = frame->GetFrameIndex();

    const DescriptorTableRef& descriptorTable = m_objectVisibility->GetDescriptorTable();

    const DescriptorSetRef& descriptorSet = descriptorTable->GetDescriptorSet("ObjectVisibilityDescriptorSet", frameIndex);
    Assert(descriptorSet != nullptr);

    descriptorSet->SetElement("ObjectInstancesBuffer", m_indirectDrawState.GetInstanceBuffer(frameIndex));
    descriptorSet->SetElement("IndirectDrawCommandsBuffer", m_indirectDrawState.GetIndirectBuffer(frameIndex));

    descriptorSet->Update();
}

#pragma endregion IndirectRenderer

} // namespace hyperion
