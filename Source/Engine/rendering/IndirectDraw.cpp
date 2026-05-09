/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

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
#include <rendering/CBufferAllocator.hpp>

#include <rendering/passes/EnvProbePass.hpp>
#include <rendering/passes/DeferredPass.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <scene/View.hpp>

#include <Core/math/MathUtil.hpp>

namespace Hyperion {

struct alignas(16) ComputeVisibilityConstants
{
    Vec2u depthPyramidDimensions;
    uint32 totalMips;
    uint32 batchOffset;
    uint32 numInstances;
    uint32 entityInstanceBatchStride;
};

static void ZeroizeBuffer(
    CommandRecorderBase& cr,
    GpuBuffer* stagingBuffer,
    GpuBuffer* dstBuffer)
{
    AssertDebug(dstBuffer != nullptr);

    if (dstBuffer->IsCpuAccessible())
    {
        // zeroize buffer, flush
        dstBuffer->Memset(dstBuffer->Size(), 0);
        dstBuffer->Flush(0, dstBuffer->Size());

        return;
    }

    // staging buffer cannot be null if dstBuffer isn't cpu accessible.
    Assert(stagingBuffer != nullptr);

    CheckResult(stagingBuffer->EnsureCapacity(dstBuffer->Size()));

    // upload zeros to the buffer using a staging buffer.
    if (!stagingBuffer->IsCreated())
    {
        CheckResult(stagingBuffer->Create());
    }

    // set all to zero
    stagingBuffer->Memset(stagingBuffer->Size(), 0);

    cr << InsertBarrier(stagingBuffer, RS_COPY_SRC);
    cr << InsertBarrier(dstBuffer, RS_COPY_DST);

    cr << CopyBuffer(stagingBuffer, dstBuffer, dstBuffer->Size());

    cr << InsertBarrier(dstBuffer, RS_INDIRECT_ARG);
}

static inline bool CreateOrResizeBuffer(
    CommandRecorderBase& cr,
    GpuBufferRef& buffer,
    size_t newBufferSize)
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
        buffer = RI.MakeGpuBuffer(prevBufferType, newBufferSize);

        if (prevWasCpuAccessible)
        {
            buffer->SetIsCpuAccessible(true);
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
    CommandRecorderBase& cr,
    const Span<IndirectDrawCommand>& drawCommandsBuffer,
    GpuBufferRef& indirectBuffer,
    GpuBuffer* stagingBuffer)
{
    const size_t requiredSize = drawCommandsBuffer.Size() * sizeof(IndirectDrawCommand);

    const bool wasCreatedOrResized = CreateOrResizeBuffer(cr, indirectBuffer, requiredSize);

    if (!wasCreatedOrResized)
    {
        return false;
    }

    ZeroizeBuffer(cr, stagingBuffer, indirectBuffer);

    return true;
}

static bool ResizeInstancesBuffer(
    CommandRecorderBase& cr,
    uint32 numObjectInstances,
    GpuBufferRef& instanceBuffer)
{
    const bool wasCreatedOrResized = CreateOrResizeBuffer(
        cr,
        instanceBuffer,
        numObjectInstances * sizeof(ObjectInstance));

    if (wasCreatedOrResized)
    {
        ZeroizeBuffer(cr, nullptr, instanceBuffer);
    }

    return wasCreatedOrResized;
}

static bool ResizeIfNeeded(
    CommandRecorderBase& cr,
    FixedArray<GpuBufferRef, NumFramesInFlight>& indirectBuffers,
    FixedArray<GpuBufferRef, NumFramesInFlight>& instanceBuffers,
    FixedArray<GpuBufferRef, NumFramesInFlight>& stagingBuffers,
    uint32 numObjectInstances,
    const Span<IndirectDrawCommand>& drawCommandsBuffer,
    uint8 dirtyBits)
{
    bool resizeHappened = false;

    const uint32 frameIndex = GetFrameCounter() % NumFramesInFlight;

    GpuBufferRef& indirectBuffer = indirectBuffers[frameIndex];
    GpuBufferRef& instanceBuffer = instanceBuffers[frameIndex];
    GpuBufferRef& stagingBuffer = stagingBuffers[frameIndex];

    if ((dirtyBits & (1u << frameIndex)) || !indirectBuffer)
    {
        resizeHappened |= ResizeIndirectDrawCommandsBuffer(cr, drawCommandsBuffer, indirectBuffer, stagingBuffer);
    }

    if ((dirtyBits & (1u << frameIndex)) || !instanceBuffer)
    {
        resizeHappened |= ResizeInstancesBuffer(cr, numObjectInstances, instanceBuffer);
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

    Array<IndirectDrawCommand, RHIAllocator> drawCommandsBuffer;
    RI.PopulateIndirectDrawCommandsBuffer(GpuBufferRef::Null(), GpuBufferRef::Null(), 0, drawCommandsBuffer);

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
    {
        m_instanceBuffers[frameIndex] = RI.MakeGpuBuffer(GpuBufferType::StructuredBuffer, sizeof(ObjectInstance));
        m_instanceBuffers[frameIndex]->SetIsCpuAccessible(true);
#if HYP_DEBUG_MODE
        m_instanceBuffers[frameIndex]->SetDebugName(NAME_FMT("IndirectDraw_InstancesBuffer_Frame{}", frameIndex));
#endif
        CheckResult(m_instanceBuffers[frameIndex]->Create());

        m_indirectBuffers[frameIndex] = RI.MakeGpuBuffer(GpuBufferType::IndirectArgsBuffer, drawCommandsBuffer.ByteSize());
#if HYP_DEBUG_MODE
        m_indirectBuffers[frameIndex]->SetDebugName(NAME_FMT("IndirectDraw_IndirectBuffer_Frame{}", frameIndex));
#endif

        CheckResult(m_indirectBuffers[frameIndex]->Create());

        if (!m_indirectBuffers[frameIndex]->IsCpuAccessible())
        {
            m_stagingBuffers[frameIndex] = RI.MakeGpuBuffer(GpuBufferType::StagingBuffer, drawCommandsBuffer.ByteSize());
#if HYP_DEBUG_MODE
            m_stagingBuffers[frameIndex]->SetDebugName(NAME_FMT("IndirectDraw_StagingBuffer_Frame{}", frameIndex));
#endif

            CheckResult(m_stagingBuffers[frameIndex]->Create());
        }
    }
}

void IndirectDrawState::PushDrawCall(size_t drawCallIndex, const DrawCallStorage& drawCalls, DrawCommandData& out)
{
    HYP_SCOPE;

    out = {};

    const uint32 drawCommandIndex = m_numDrawCommands++;

    ObjectInstance& instance = m_objectInstances.EmplaceBack();
    instance.entityBindingIndex = drawCalls.entityBindingIndices[drawCallIndex];
    instance.drawCommandIndex = drawCommandIndex;
    instance.batchIndex = ~0u;

    out.drawCommandIndex = drawCommandIndex;

    RI.PopulateIndirectDrawCommandsBuffer(
        drawCalls.meshProxies[drawCallIndex]->mesh->GetVertexBuffer(),
        drawCalls.meshProxies[drawCallIndex]->mesh->GetIndexBuffer(),
        drawCommandIndex,
        m_drawCommandsBuffer);

    m_dirtyBits = AllBitsDirty;
}

void IndirectDrawState::PushInstancedDrawCall(size_t drawCallIndex, const InstancedDrawCallStorage& drawCalls, DrawCommandData& out)
{
    out = {};

    const uint32 drawCommandIndex = m_numDrawCommands++;

    const uint32 count = drawCalls.counts[drawCallIndex];
    EntityInstanceBatch* batch = drawCalls.batches[drawCallIndex];

    for (uint32 index = 0; index < count; index++)
    {
        ObjectInstance& instance = m_objectInstances.EmplaceBack();
        instance.entityBindingIndex = batch->indices[index];
        instance.drawCommandIndex = drawCommandIndex;
        instance.batchIndex = batch->batchIndex;
    }

    out.drawCommandIndex = drawCommandIndex;

    RI.PopulateIndirectDrawCommandsBuffer(
        drawCalls.meshProxies[drawCallIndex]->mesh->GetVertexBuffer(),
        drawCalls.meshProxies[drawCallIndex]->mesh->GetIndexBuffer(),
        drawCommandIndex,
        m_drawCommandsBuffer);

    m_dirtyBits = AllBitsDirty;
}

void IndirectDrawState::ResetDrawState()
{
    m_numDrawCommands = 0;

    m_objectInstances.Clear();

    // use Resize() to keep the memory allocated
    m_drawCommandsBuffer.Resize(0);

    m_dirtyBits = AllBitsDirty;
}

void IndirectDrawState::UpdateBufferData(CommandRecorderBase& cr, bool* outWasResized)
{
    const uint32 frameIndex = GetFrameCounter() % NumFramesInFlight;

    if ((*outWasResized = ResizeIfNeeded(
             cr,
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

    const bool needsStaging = !indirectBuffer->IsCpuAccessible();

    // fill instances buffer with data of the meshes
    // @TODO Rework to use StructuredBuffer instead of staging buffers manually
    if (needsStaging)
    {
        GpuBuffer* stagingBuffer = m_stagingBuffers[frameIndex];

        Assert(stagingBuffer != nullptr);
        Assert(stagingBuffer->Size() >= m_drawCommandsBuffer.ByteSize());

        stagingBuffer->Copy(m_drawCommandsBuffer.ByteSize(), m_drawCommandsBuffer.Data());

        //CommandRecorder& cr = RI.commandRecorderAllocator.GetCommandRecorder();

        cr << InsertBarrier(stagingBuffer, RS_COPY_SRC);
        cr << InsertBarrier(indirectBuffer, RS_COPY_DST);

        cr << CopyBuffer(stagingBuffer, indirectBuffer, stagingBuffer->Size());

        cr << InsertBarrier(indirectBuffer, RS_INDIRECT_ARG);

        //cr.Done();
    }

    Assert(instanceBuffer->Size() >= m_objectInstances.Size() * sizeof(ObjectInstance));

    // update data for object instances (cpu - gpu)
    instanceBuffer->Copy(m_objectInstances.Size() * sizeof(ObjectInstance), m_objectInstances.Data());
    instanceBuffer->Flush(0, m_objectInstances.Size() * sizeof(ObjectInstance));

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
}

void IndirectRenderer::Create(EntityBatchAllocatorBase* batchAllocator)
{
    Assert(batchAllocator != nullptr);
    m_batchAllocator = batchAllocator;

    m_indirectDrawState.Create();
}

void IndirectRenderer::PushDrawCallsToIndirectState(CommandRecorderBase& cr, DrawCallCollection& drawCallCollection)
{
    for (size_t i = 0; i < drawCallCollection.drawCalls.Size(); i++)
    {
        DrawCommandData drawCommandData;
        m_indirectDrawState.PushDrawCall(i, drawCallCollection.drawCalls, drawCommandData);

        drawCallCollection.drawCalls.drawCommandIndices[i] = drawCommandData.drawCommandIndex;
    }

    for (size_t i = 0; i < drawCallCollection.instancedDrawCalls.Size(); i++)
    {
        DrawCommandData drawCommandData;
        m_indirectDrawState.PushInstancedDrawCall(i, drawCallCollection.instancedDrawCalls, drawCommandData);

        drawCallCollection.instancedDrawCalls.drawCommandIndices[i] = drawCommandData.drawCommandIndex;
    }
}

void IndirectRenderer::PrepareDrawCommands(CommandRecorderBase& cr)
{
    bool wasBufferResized = false;
    m_indirectDrawState.UpdateBufferData(cr, &wasBufferResized);
}

void IndirectRenderer::ExecuteCullShaderInBatches(CommandRecorderBase& cr, const RenderSetup& renderSetup)
{
    AssertDebug(renderSetup.world && renderSetup.view);
    AssertDebug(renderSetup.passData != nullptr);

    AssertDebug(m_batchAllocator != nullptr);

    AssertDebug(renderSetup.passData->cullData.depthPyramidImageView != nullptr);

    const uint32 frameIndex = GetFrameCounter() % NumFramesInFlight;

    AssertDebug(m_indirectDrawState.GetIndirectBuffer(frameIndex).IsValid());
    AssertDebug(m_indirectDrawState.GetIndirectBuffer(frameIndex)->Size() != 0);

    const size_t numInstances = m_indirectDrawState.GetInstances().Size();
    const uint32 numBatches = (uint32(numInstances) / IndirectDrawState::BatchSize) + 1;

    if (numInstances == 0)
    {
        return;
    }

    PrepareDrawCommands(cr);

    if (m_cachedCullData != renderSetup.passData->cullData)
    {
        m_cachedCullData = renderSetup.passData->cullData;
        m_cachedCullDataUpdatedBits = 0xFF;
    }

    DeferredPassData* pd = DynamicCast<DeferredPassData>(renderSetup.passData);
    AssertDebug(pd != nullptr);

    uint32 numShaderUniforms = 0;

    cr << SetCurrentShader(ShaderDesc(NAME("ComputeVisibility")));

    cr << SetShaderUniform(numShaderUniforms++, "CamerasBuffer"_sh, RI.namedBuffers[NamedBuffer::Cameras], Resources::GetBinding(renderSetup.view->GetCamera()));
    cr << SetShaderUniform(numShaderUniforms++, "EntitiesBuffer"_sh, RI.namedBuffers[NamedBuffer::Entities]);
    cr << SetShaderUniform(numShaderUniforms++, "WorldsBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);

    cr << SetShaderUniform(numShaderUniforms++, "SamplerNearest"_sh, RI.placeholderData->GetSamplerNearest());
    cr << SetShaderUniform(numShaderUniforms++, "DepthPyramidResult"_sh, renderSetup.passData->cullData.depthPyramidImageView);

    cr << SetShaderUniform(numShaderUniforms++, "ObjectInstancesBuffer"_sh, m_indirectDrawState.GetInstanceBuffer(frameIndex), ShaderDataOffset(0, sizeof(ObjectInstance)));
    cr << SetShaderUniform(numShaderUniforms++, "IndirectDrawCommandsBuffer"_sh, m_indirectDrawState.GetIndirectBuffer(frameIndex), ShaderDataOffset(0, sizeof(IndirectDrawCommand)));

    // For ComputeVisibility we use RWByteAddressBuffer -- that's why stride is passed as 0.
    cr << SetShaderUniform(numShaderUniforms++, "EntityInstanceBatchesBuffer"_sh,
        m_batchAllocator->GetStructuredBuffer().gpuBuffer,
        ShaderDataOffset(0, 0));

    ComputeVisibilityConstants constants {};
    constants.depthPyramidDimensions = pd->depthPyramidRenderer->GetExtent();
    constants.totalMips = pd->depthPyramidRenderer->GetTotalMips();
    constants.batchOffset = 0;
    constants.numInstances = numInstances;
    constants.entityInstanceBatchStride = ByteUtil::AlignAs(m_batchAllocator->GetStructSize(), m_batchAllocator->GetStructAlignment());

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferSize = 0;
    size_t cbufferOffset = 0;

    RI.cbufferAllocator->Write(&constants);
    RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    cr << SetShaderUniform(numShaderUniforms++, "ComputeVisibilityConstants"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

    cr << InsertBarrier(m_indirectDrawState.GetIndirectBuffer(frameIndex), RS_INDIRECT_ARG);

    cr << DispatchCompute(Vec3u { (numBatches + IndirectDrawState::BatchSize - 1) / IndirectDrawState::BatchSize, 1, 1 });

    cr << InsertBarrier(m_indirectDrawState.GetIndirectBuffer(frameIndex), RS_INDIRECT_ARG);
}

#pragma endregion IndirectRenderer

} // namespace Hyperion
