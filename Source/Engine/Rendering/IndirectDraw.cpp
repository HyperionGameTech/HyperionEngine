/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <Rendering/IndirectDraw.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/DrawCall.hpp>
#include <Rendering/RenderInterface.hpp>
#include <Rendering/DepthPyramidRenderer.hpp>
#include <Rendering/Frame.hpp>
#include <Rendering/ComputePipeline.hpp>
#include <Rendering/DescriptorSet.hpp>
#include <Rendering/GpuBuffer.hpp>
#include <Rendering/RenderHelpers.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/ShaderInstance.hpp>
#include <Rendering/Mesh.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/CBufferAllocator.hpp>
#include <Rendering/TextureViewCache.hpp>

#include <Rendering/Passes/EnvProbePass.hpp>
#include <Rendering/Passes/DeferredPass.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Scene/View.hpp>

#include <Core/Math/MathUtil.hpp>

namespace Hyperion {

struct alignas(16) ComputeVisibilityConstants
{
    Vec2u depthPyramidDimensions;
    uint32 totalMips;
    uint32 batchOffset;
    uint32 numInstances;
    uint32 entityInstanceBatchStride;
};

static void ZeroizeBuffer(CommandRecorder& cr, GpuBuffer* dstBuffer)
{
    AssertDebug(dstBuffer != nullptr);

    const size_t bufferSize = dstBuffer->Size();

    if (dstBuffer->IsCpuAccessible())
    {
        // zeroize buffer, flush
        dstBuffer->Memset(bufferSize, 0);
        dstBuffer->Flush(0, bufferSize);

        return;
    }

    // staging buffer cannot be null if dstBuffer isn't cpu accessible.
    GpuBuffer* stagingBuffer = RI.stagingBufferPool->AcquireStagingBuffer(bufferSize);
    Assert(stagingBuffer != nullptr);

    // set all to zero
    stagingBuffer->Memset(bufferSize, 0);

    cr << InsertBarrier(stagingBuffer, RS_COPY_SRC);
    cr << InsertBarrier(dstBuffer, RS_COPY_DST);

    cr << CopyBuffer(stagingBuffer, dstBuffer, bufferSize);

    cr << InsertBarrier(dstBuffer, RS_INDIRECT_ARG);
}

static inline bool CreateOrResizeBuffer(
    CommandRecorder& cr,
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

        Check(buffer->Create());

        return true;
    }

    if (!buffer->IsCreated())
    {
        Check(buffer->Create());

        return true;
    }

    return false;
}

static bool ResizeIndirectDrawCommandsBuffer(
    CommandRecorder& cr,
    const Span<IndirectDrawCommand>& drawCommandsBuffer,
    GpuBufferRef& indirectBuffer)
{
    const size_t requiredSize = drawCommandsBuffer.Size() * sizeof(IndirectDrawCommand);

    const bool wasCreatedOrResized = CreateOrResizeBuffer(cr, indirectBuffer, requiredSize);

    if (!wasCreatedOrResized)
    {
        return false;
    }

    ZeroizeBuffer(cr, indirectBuffer);

    return true;
}

static bool ResizeInstancesBuffer(
    CommandRecorder& cr,
    uint32 numObjectInstances,
    GpuBufferRef& instanceBuffer)
{
    const bool wasCreatedOrResized = CreateOrResizeBuffer(
        cr,
        instanceBuffer,
        numObjectInstances * sizeof(ObjectInstance));

    if (wasCreatedOrResized)
    {
        ZeroizeBuffer(cr, instanceBuffer);
    }

    return wasCreatedOrResized;
}

static bool ResizeIfNeeded(
    CommandRecorder& cr,
    FixedArray<GpuBufferRef, NumFramesInFlight>& indirectBuffers,
    FixedArray<GpuBufferRef, NumFramesInFlight>& instanceBuffers,
    uint32 numObjectInstances,
    const Span<IndirectDrawCommand>& drawCommandsBuffer,
    uint8 dirtyBits)
{
    bool resizeHappened = false;

    const uint32 frameIndex = GetFrameCounter() % NumFramesInFlight;

    GpuBufferRef& indirectBuffer = indirectBuffers[frameIndex];
    GpuBufferRef& instanceBuffer = instanceBuffers[frameIndex];

    if ((dirtyBits & (1u << frameIndex)) || !indirectBuffer)
    {
        resizeHappened |= ResizeIndirectDrawCommandsBuffer(cr, drawCommandsBuffer, indirectBuffer);
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
        Check(m_instanceBuffers[frameIndex]->Create());

        m_indirectBuffers[frameIndex] = RI.MakeGpuBuffer(GpuBufferType::IndirectArgsBuffer, drawCommandsBuffer.ByteSize());
#if HYP_DEBUG_MODE
        m_indirectBuffers[frameIndex]->SetDebugName(NAME_FMT("IndirectDraw_IndirectBuffer_Frame{}", frameIndex));
#endif

        Check(m_indirectBuffers[frameIndex]->Create());
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

void IndirectDrawState::UpdateBufferData(CommandRecorder& cr, bool* outWasResized)
{
    const uint32 frameIndex = GetFrameCounter() % NumFramesInFlight;

    if ((*outWasResized = ResizeIfNeeded(
             cr,
             m_indirectBuffers,
             m_instanceBuffers,
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
    if (needsStaging)
    {
        const size_t drawCommandsBufferSize = m_drawCommandsBuffer.ByteSize();

        GpuBuffer* stagingBuffer = RI.stagingBufferPool->AcquireStagingBuffer(drawCommandsBufferSize);
        Assert(stagingBuffer != nullptr);

        stagingBuffer->Copy(drawCommandsBufferSize, m_drawCommandsBuffer.Data());
        stagingBuffer->Flush(0, drawCommandsBufferSize);

        cr << InsertBarrier(stagingBuffer, RS_COPY_SRC);
        cr << InsertBarrier(indirectBuffer, RS_COPY_DST);

        cr << CopyBuffer(stagingBuffer, indirectBuffer, drawCommandsBufferSize);

        cr << InsertBarrier(indirectBuffer, RS_INDIRECT_ARG);
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
    : m_batchAllocator(nullptr)
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

void IndirectRenderer::PushDrawCallsToIndirectState(CommandRecorder& cr, DrawCallCollection& drawCallCollection)
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

void IndirectRenderer::PrepareDrawCommands(CommandRecorder& cr)
{
    bool wasBufferResized = false;
    m_indirectDrawState.UpdateBufferData(cr, &wasBufferResized);
}

void IndirectRenderer::ExecuteCullShaderInBatches(CommandRecorder& cr, const RenderSetup& renderSetup)
{
    AssertDebug(renderSetup.world && renderSetup.view);
    AssertDebug(renderSetup.passData != nullptr);

    AssertDebug(m_batchAllocator != nullptr);

    const uint32 frameIndex = GetFrameCounter() % NumFramesInFlight;

    AssertDebug(m_indirectDrawState.GetIndirectBuffer(frameIndex).IsValid());
    AssertDebug(m_indirectDrawState.GetIndirectBuffer(frameIndex)->Size() != 0);

    const size_t numInstances = m_indirectDrawState.GetInstances().Size();
    const uint32 numBatches = (uint32(numInstances) + IndirectDrawState::BatchSize - 1) / IndirectDrawState::BatchSize;

    if (numInstances == 0)
    {
        return;
    }

    PrepareDrawCommands(cr);

    DeferredPassData* pd = DynamicCast<DeferredPassData>(renderSetup.passData);
    AssertDebug(pd != nullptr);

    uint32 numShaderUniforms = 0;

    cr << SetCurrentShader(ShaderDesc(NAME("ComputeVisibility")));

    cr << SetShaderUniform(numShaderUniforms++, "CamerasBuffer"_sh, RI.namedBuffers[NamedBuffer::Cameras], Resources::GetBinding(renderSetup.view->GetCamera()));
    cr << SetShaderUniform(numShaderUniforms++, "EntitiesBuffer"_sh, RI.namedBuffers[NamedBuffer::Entities]);
    cr << SetShaderUniform(numShaderUniforms++, "WorldsBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);

    cr << SetShaderUniform(numShaderUniforms++, "SamplerNearest"_sh, RI.placeholderData->GetSamplerNearest());
    cr << SetShaderUniform(numShaderUniforms++, "DepthPyramidResult"_sh, RI.textureViewCache->GetOrCreate(pd->depthPyramidRenderer->GetHZBTexture()));

    cr << SetShaderUniform(numShaderUniforms++, "ObjectInstancesBuffer"_sh, m_indirectDrawState.GetInstanceBuffer(frameIndex), ShaderDataOffset(0, sizeof(ObjectInstance)));
    cr << SetShaderUniform(numShaderUniforms++, "IndirectDrawCommandsBuffer"_sh, m_indirectDrawState.GetIndirectBuffer(frameIndex), ShaderDataOffset(0, sizeof(IndirectDrawCommand)));

    GpuBuffer* entityInstanceBatchesBuffer = m_batchAllocator->GetStructuredBuffer().gpuBuffer;

    // For ComputeVisibility we use RWByteAddressBuffer -- that's why stride is passed as 0.
    cr << SetShaderUniform(numShaderUniforms++, "EntityInstanceBatchesBuffer"_sh,
        entityInstanceBatchesBuffer,
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

    cr << InsertBarrier(m_indirectDrawState.GetIndirectBuffer(frameIndex), RS_UNORDERED_ACCESS, ShaderModuleType::Compute);
    cr << InsertBarrier(entityInstanceBatchesBuffer, RS_UNORDERED_ACCESS, ShaderModuleType::Compute);

    cr << DispatchCompute(Vec3u { numBatches, 1, 1 });

    cr << InsertBarrier(m_indirectDrawState.GetIndirectBuffer(frameIndex), RS_INDIRECT_ARG);
    cr << InsertBarrier(entityInstanceBatchesBuffer, RS_SHADER_RESOURCE, ShaderModuleType::Vertex);
}

#pragma endregion IndirectRenderer

} // namespace Hyperion
