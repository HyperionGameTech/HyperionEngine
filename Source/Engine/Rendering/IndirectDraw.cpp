/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <Rendering/IndirectDraw.hpp>
#include <Rendering/InstanceDataPool.hpp>
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

#include <Framework/CVarManager.hpp>
#include <Framework/EngineStats.hpp>
#include <Framework/View.hpp>

#include <Core/Math/MathUtil.hpp>

namespace Hyperion {

static EngineStatTimer s_statWriteInstances("Rendering/CPU/IndirectDraw/WriteInstances");
static EngineStatTimer s_statUpdateBuffers("Rendering/CPU/IndirectDraw/UpdateBuffers");
static EngineStatGpuTimer s_statCull("Rendering/GPU/IndirectDraw/Cull");

struct ComputeVisibilityConstants
{
    Mat4f viewProj;

    Vec2u depthPyramidDimensions;
    uint32 totalMips;
    uint32 batchOffset;
    uint32 numInstances;
    uint32 entityInstanceBatchStride;
};

#pragma region IndirectDrawState

IndirectDrawState::IndirectDrawState()
    : m_instanceData(nullptr),
      m_instanceOffset(0),
      m_numReservedInstances(0),
      m_numDrawCommands(0)
{
}

IndirectDrawState::~IndirectDrawState()
{
    m_indirectBuffer.Shutdown();
    m_instances.Shutdown();
}

void IndirectDrawState::Create()
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    m_instances = StructuredBuffer(1, sizeof(ObjectInstance));
    m_indirectBuffer = IndirectArgsBuffer(1, sizeof(IndirectDrawCommand));
}

void IndirectDrawState::ReserveInstances(size_t count)
{
    const size_t requiredInstances = m_instanceOffset + count;

    if (m_instances.TotalSizeBytes() / sizeof(ObjectInstance) < requiredInstances)
    {
        m_instances.Resize(MathUtil::NextPowerOf2(requiredInstances) * sizeof(ObjectInstance));

        m_instances.ClearDirty();
        m_instances.MarkDirty(0, m_instanceOffset * sizeof(ObjectInstance));
    }

    if (!m_instances.IsInitialized())
    {
        m_instances.Initialize();
    }

    m_instanceData = m_instances.GetWritePtr<ObjectInstance>(m_instanceOffset, count) - m_instanceOffset;
    m_numReservedInstances = requiredInstances;
}

ObjectInstance& IndirectDrawState::NextInstance()
{
    AssertDebug(m_instanceOffset < m_numReservedInstances);

    return m_instanceData[m_instanceOffset++];
}

void IndirectDrawState::PushDrawCall(size_t drawCallIndex, const DrawCallStorage& drawCalls, DrawCommandData& out)
{
    HYP_SCOPE;

    out = {};

    const uint32 drawCommandIndex = m_numDrawCommands++;

    ObjectInstance& instance = NextInstance();
    instance.entityBindingIndex = drawCalls.entityBindingIndices[drawCallIndex];
    instance.drawCommandIndex = drawCommandIndex;
    instance.batchIndex = ~0u;
    instance.instanceIndex = 0;
    instance.instanceSlot = IdentityInstanceSlot;

    out.drawCommandIndex = drawCommandIndex;

    const RenderProxyMesh* meshProxy = drawCalls.meshProxies[drawCallIndex];

    // the LOD was picked for this view when the draw call was built
    const uint8 lodIndex = uint8(drawCalls.ids[drawCallIndex].lodIndex);

    RI.PopulateIndirectDrawCommandsBuffer(
        meshProxy->mesh->GetVertexBuffer(lodIndex),
        meshProxy->mesh->GetIndexBuffer(lodIndex),
        meshProxy->mesh->NumIndices(lodIndex),
        drawCommandIndex,
        m_drawCommandsBuffer);
}

void IndirectDrawState::PushInstancedDrawCall(size_t drawCallIndex, const InstancedDrawCallStorage& drawCalls, DrawCommandData& out)
{
    out = {};

    const uint32 drawCommandIndex = m_numDrawCommands++;

    const uint32 count = drawCalls.counts[drawCallIndex];

    const EntityInstanceBatch* batch = drawCalls.batches[drawCallIndex];

    for (uint32 index = 0; index < count; index++)
    {
        ObjectInstance& instance = NextInstance();
        instance.entityBindingIndex = (batch->indices[index] & 0xFFFFFFu);
        instance.drawCommandIndex = drawCommandIndex;
        instance.batchIndex = batch->batchIndex;
        instance.instanceIndex = index;
        instance.instanceSlot = batch->instanceSlots[index];
    }

    out.drawCommandIndex = drawCommandIndex;

    const RenderProxyMesh* meshProxy = drawCalls.meshProxies[drawCallIndex];

    // the LOD was picked for this view when the draw call was built
    const uint8 lodIndex = uint8(drawCalls.ids[drawCallIndex].lodIndex);

    RI.PopulateIndirectDrawCommandsBuffer(
        meshProxy->mesh->GetVertexBuffer(lodIndex),
        meshProxy->mesh->GetIndexBuffer(lodIndex),
        meshProxy->mesh->NumIndices(lodIndex),
        drawCommandIndex,
        m_drawCommandsBuffer);
}

void IndirectDrawState::ResetDrawState()
{
    m_numDrawCommands = 0;

    m_instanceOffset = 0;
    m_numReservedInstances = 0;
    m_instanceData = nullptr;

    // use Resize() to keep the memory allocated
    m_drawCommandsBuffer.Resize(0);
}

void IndirectDrawState::UpdateBufferData(CommandRecorder& cr, bool* outWasResized)
{
    const size_t drawCommandsBufferSize = m_drawCommandsBuffer.ByteSize();

    *outWasResized = false;

    if (m_indirectBuffer.TotalSizeBytes() < drawCommandsBufferSize)
    {
        size_t newSize = drawCommandsBufferSize;

        if constexpr (UseNextPow2Size)
        {
            newSize = MathUtil::NextPowerOf2(newSize);
        }

        *outWasResized = m_indirectBuffer.Resize(newSize);

        // all draw commands are rewritten below, so skip uploading the whole new buffer
        m_indirectBuffer.ClearDirty();
    }

    if (!m_indirectBuffer.IsInitialized())
    {
        m_indirectBuffer.Initialize();
    }

    m_indirectBuffer.Write(0, drawCommandsBufferSize, m_drawCommandsBuffer.Data());

    // Defer flush of both buffers to the top of the frame.
    m_indirectBuffer.FlushBatched();
    m_instances.FlushBatched();
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

void IndirectRenderer::Create(EntityBatchAllocator* batchAllocator)
{
    Assert(batchAllocator != nullptr);
    m_batchAllocator = batchAllocator;

    m_indirectDrawState.Create();
}

void IndirectRenderer::PushDrawCallsToIndirectState(CommandRecorder& cr, DrawCallCollection& drawCallCollection)
{
    ENGINE_STAT_SCOPE(&s_statWriteInstances);

    size_t numInstances = drawCallCollection.drawCalls.Size();

    for (size_t i = 0; i < drawCallCollection.instancedDrawCalls.Size(); i++)
    {
        numInstances += drawCallCollection.instancedDrawCalls.counts[i];
    }

    m_indirectDrawState.ReserveInstances(numInstances);

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
    ENGINE_STAT_SCOPE(&s_statUpdateBuffers);

    bool wasBufferResized = false;
    m_indirectDrawState.UpdateBufferData(cr, &wasBufferResized);
}

void IndirectRenderer::ExecuteCullShaderInBatches(CommandRecorder& cr, const RenderSetup& renderSetup)
{
    AssertDebug(renderSetup.world && renderSetup.view);
    AssertDebug(renderSetup.passData != nullptr);

    AssertDebug(m_batchAllocator != nullptr);

    AssertDebug(m_indirectDrawState.GetIndirectBuffer().gpuBuffer != nullptr);

    const size_t numInstances = m_indirectDrawState.GetNumInstances();
    const uint32 numBatches = (uint32(numInstances) + IndirectDrawState::BatchSize - 1) / IndirectDrawState::BatchSize;

    if (numInstances == 0)
    {
        return;
    }

    PrepareDrawCommands(cr);

    // fetch after PrepareDrawCommands(), it may have resized the buffer
    GpuBuffer* indirectBuffer = m_indirectDrawState.GetIndirectBuffer().gpuBuffer;

    DeferredPassData* pd = TryCast<DeferredPassData>(renderSetup.passData);
    AssertDebug(pd != nullptr);

    RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(GetRenderProxy(renderSetup.view->GetCamera()));
    AssertDebug(cameraProxy != nullptr);

    uint32 numShaderUniforms = 0;

    cr << SetCurrentShader(ShaderDesc(NAME("ComputeVisibility")));
    cr << SetShaderUniform(numShaderUniforms++, "EntitiesBuffer"_sh, RI.namedBuffers[NamedBuffer::Entities]);
    cr << SetShaderUniform(numShaderUniforms++, "InstanceDataBuffer"_sh, RI.namedBuffers[NamedBuffer::InstanceData]);
    cr << SetShaderUniform(numShaderUniforms++, "WorldsBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);

    cr << SetShaderUniform(numShaderUniforms++, "SamplerNearest"_sh, RI.placeholderData->GetSamplerNearest());
    cr << SetShaderUniform(numShaderUniforms++, "DepthPyramidResult"_sh, RI.textureViewCache->GetOrCreate(pd->depthPyramidRenderer->GetHZBTexture()));

    cr << SetShaderUniform(numShaderUniforms++, "ObjectInstancesBuffer"_sh, m_indirectDrawState.GetInstanceBuffer());
    cr << SetShaderUniform(numShaderUniforms++, "IndirectDrawCommandsBuffer"_sh, indirectBuffer, ShaderDataOffset(0, sizeof(IndirectDrawCommand)));

    GpuBuffer* entityInstanceBatchesBuffer = m_batchAllocator->GetStructuredBuffer().gpuBuffer;

    // For ComputeVisibility we use RWByteAddressBuffer -- that's why stride is passed as 0.
    cr << SetShaderUniform(numShaderUniforms++, "EntityInstanceBatchesBuffer"_sh,
        entityInstanceBatchesBuffer,
        ShaderDataOffset(0, 0));

    ComputeVisibilityConstants constants {};
    constants.viewProj = cameraProxy->bufferData.viewProjMat;
    constants.depthPyramidDimensions = pd->depthPyramidRenderer->GetExtent();
    constants.totalMips = pd->depthPyramidRenderer->GetTotalMips();
    constants.batchOffset = 0;
    constants.numInstances = numInstances;
    constants.entityInstanceBatchStride = sizeof(EntityInstanceBatch);

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferSize = 0;
    size_t cbufferOffset = 0;

    RI.cbufferAllocator->Write(&constants);
    RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    cr << SetShaderUniform(numShaderUniforms++, "ComputeVisibilityConstants"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

    cr << InsertBarrier(indirectBuffer, ResourceState::UnorderedAccess, ShaderModuleType::Compute);
    cr << InsertBarrier(entityInstanceBatchesBuffer, ResourceState::UnorderedAccess, ShaderModuleType::Compute);

    {
        ENGINE_STAT_GPU_SCOPE(&s_statCull, &cr);

        cr << DispatchCompute(Vec3u { numBatches, 1, 1 });
    }

    cr << InsertBarrier(indirectBuffer, ResourceState::IndirectArg);
    cr << InsertBarrier(entityInstanceBatchesBuffer, ResourceState::ShaderResource, ShaderModuleType::Vertex);
}

#pragma endregion IndirectRenderer

} // namespace Hyperion
