/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <rendering/renderers/ParticleVolumeRenderer.hpp>

#include <rendering/RenderInterface.hpp>
#include <rendering/Frame.hpp>
#include <rendering/CommandRecorder.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/GraphicsPipeline.hpp>
#include <rendering/ComputePipeline.hpp>
#include <rendering/GraphicsPipelineCache.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/RenderProxyList.hpp>
#include <rendering/RendererMain.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/Texture.hpp>
#include <rendering/TextureViewCache.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/CBufferAllocator.hpp>

#include <rendering/util/DeletionQueue.hpp>
#include <rendering/util/ShaderPropertyDictionary.hpp>

#include <scene/ParticleVolume.hpp>
#include <scene/View.hpp>

#include <scene/camera/Camera.hpp>

#include <util/MeshBuilder.hpp>
#include <util/NoiseFactory.hpp>

#include <Core/math/MathUtil.hpp>

// For IndirectDrawCommand
#if HYP_VULKAN
#include <rendering/vulkan/VulkanStructs.hpp>
#elif HYP_DX12
#include <rendering/dx12/DX12Structs.hpp>
#endif

namespace Hyperion {

// How many frames until we release resources for unused volumes?
static constexpr uint32 DiscardFrames = 60;

static const ShaderPropertyId s_propHasPhysics = InternShaderProperty(ShaderProperty(NAME("HAS_PHYSICS")));

ParticleVolumeRenderer::VolumeState::~VolumeState()
{
    EnqueueDeletion(std::move(particleBuffer));
    EnqueueDeletion(std::move(indirectBuffer));
    EnqueueDeletion(std::move(noiseMap));
}

ParticleVolumeRenderer::ParticleVolumeRenderer() = default;
ParticleVolumeRenderer::~ParticleVolumeRenderer() = default;

void ParticleVolumeRenderer::Initialize()
{
    HYP_SCOPE;
}

void ParticleVolumeRenderer::Shutdown()
{
    HYP_SCOPE;

    if (m_staging.quadMesh.IsValid())
    {
        EnqueueDeletion(std::move(m_staging.quadMesh));
    }

    EnqueueDeletion(std::move(m_staging.zeroIndirectArgs));
}

PassData* ParticleVolumeRenderer::CreateViewPassData(View* view, PassDataExt&)
{
    PassData* pd = new PassData();
    pd->view = MakeWeakRef(view);

    return pd;
}

void ParticleVolumeRenderer::EnsureStaging()
{
    if (!m_staging.quadMesh.IsValid())
    {
        m_staging.quadMesh = MeshBuilder::Quad();
        m_staging.quadMesh->SetFlags(MeshFlags::ViewIndependent);
        InitObject(m_staging.quadMesh);
    }

    if (!m_staging.zeroIndirectArgs)
    {
        Array<IndirectDrawCommand, RHIAllocator> indirectDrawCommandsBuffer;
        RI.PopulateIndirectDrawCommandsBuffer(
            m_staging.quadMesh->GetVertexBuffer(),
            m_staging.quadMesh->GetIndexBuffer(),
            0, indirectDrawCommandsBuffer);

        m_staging.zeroIndirectArgs = RI.MakeGpuBuffer(GpuBufferType::StagingBuffer, indirectDrawCommandsBuffer.ByteSize());
        CheckResult(m_staging.zeroIndirectArgs->Create());

        m_staging.zeroIndirectArgs->Copy(indirectDrawCommandsBuffer.ByteSize(), indirectDrawCommandsBuffer.Data());
    }
}

static void CreateNoiseMap(Handle<Texture>& tex)
{
    static constexpr uint32 Seed = 0xff;

    TextureDesc textureDesc {};
    textureDesc.extent = Vec3u { 128, 128, 1 };
    textureDesc.type = TextureType::Texture2D;
    textureDesc.format = TextureFormat::R8;
    textureDesc.filterModeMin = TFM_LINEAR;
    textureDesc.filterModeMag = TFM_LINEAR;

    Bitmap_R8 noiseMap = SimplexNoiseGenerator(Seed).CreateBitmap(128, 128, 1024.0f);

    if (tex.IsValid())
    {
        EnqueueDeletion(std::move(tex));
    }

    tex = MakeHandle<Texture>(textureDesc, noiseMap.ToByteView());
    CheckResult(tex->Create());
}

ParticleVolumeRenderer::VolumeState& ParticleVolumeRenderer::EnsureVolumeState(RenderProxyParticleVolume* proxy)
{
    auto it = m_volumeStates.Find(proxy->particleVolume.Id());

    if (it != m_volumeStates.End())
    {
        return it->second;
    }

    VolumeState& state = m_volumeStates.Emplace(proxy->particleVolume.Id()).first->second;

    state.maxParticles = proxy->bufferData.maxParticles;

    state.particleBuffer = RI.MakeGpuBuffer(GpuBufferType::RWStructuredBuffer, state.maxParticles * sizeof(ParticleShaderData));
    CheckResult(state.particleBuffer->Create());

    state.indirectBuffer = RI.MakeGpuBuffer(GpuBufferType::IndirectArgsBuffer, sizeof(IndirectDrawCommand));
    CheckResult(state.indirectBuffer->Create());

    CreateNoiseMap(state.noiseMap);

    state.hasPhysics = proxy->particleVolume.GetUnsafe()->GetParams().hasPhysics;

    // compute shader properties
    ShaderPropertySet properties;
    properties.Add(InternShaderProperty(ShaderProperty(NAME("MAX_PARTICLES"), int(state.maxParticles))));
    properties.Set(s_propHasPhysics, state.hasPhysics);

    // set default particle graphics attributes (translucent)
    MaterialAttributes materialAttributes {};
    materialAttributes.shaderName = NAME("Particle");
    materialAttributes.shaderProperties = properties;
    materialAttributes.bucket = RenderBucket::Translucent;
    materialAttributes.blendFunction = BlendFunction::AlphaBlending();
    materialAttributes.cullFaces = FCM_BACK;
    materialAttributes.flags = MAF_DEPTH_TEST; // depth test on, depth write off by default

    MeshAttributes meshAttributes {};
    meshAttributes.inputLayout = { VT_Simple };
    meshAttributes.indexBufferElemType = GET_UNSIGNED_INT;
    meshAttributes.topology = TOP_TRIANGLES;
    state.renderableAttributes = RenderableAttributeSet(meshAttributes, materialAttributes);

    return state;
}

void ParticleVolumeRenderer::RenderFrame(Frame* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.world && renderSetup.view && renderSetup.volume);

    ParticleVolume* particleVolume = DynamicCast<ParticleVolume>(renderSetup.volume);
    AssertDebug(particleVolume != nullptr);

    EnsureStaging();

    View* view = renderSetup.view;
    RenderProxyList& rpl = GetConsumerProxyList(view);

    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    // Reset zero staging buffer state
    frame->preRenderCommands << InsertBarrier(m_staging.zeroIndirectArgs, RS_COPY_SRC);

    RenderProxyParticleVolume* proxy = static_cast<RenderProxyParticleVolume*>(GetRenderProxy(particleVolume));
    AssertDebug(proxy != nullptr);

    VolumeState& state = EnsureVolumeState(proxy);

    // zero indirect arguments (instance count)
    Assert(state.indirectBuffer->Size() == sizeof(IndirectDrawCommand));

    { // zero out indirect buffer (ahead of frame compute + rendering)
        CommandRecorder& cr = frame->preRenderCommands;

        cr << InsertBarrier(state.indirectBuffer, RS_COPY_DST);
        cr << CopyBuffer(m_staging.zeroIndirectArgs, state.indirectBuffer, sizeof(IndirectDrawCommand));
        cr << InsertBarrier(state.indirectBuffer, RS_INDIRECT_ARG);
    }

    // bind and dispatch compute
    struct ComputeShaderConstants
    {
        Vec4f origin;

        float spawnRadius;
        float randomness;
        float avgLifespan;
        uint32 maxParticles;

        float maxParticlesSqrt;
        float deltaTime;
        uint32 globalCounter;
        uint32 _pad;
    };

    ComputeShaderConstants csConstants {};
    csConstants.origin = proxy->bufferData.originStartSize;
    csConstants.spawnRadius = proxy->bufferData.spawnRadius;
    csConstants.randomness = proxy->bufferData.randomness;
    csConstants.avgLifespan = proxy->bufferData.avgLifespan;
    csConstants.maxParticles = proxy->bufferData.maxParticles;
    csConstants.maxParticlesSqrt = proxy->bufferData.maxParticlesSqrt;
    csConstants.deltaTime = 0.016f; // TODO: real render delta
    csConstants.globalCounter = m_counter++;

    RI.cbufferAllocator->Write(&csConstants);

    RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(GetRenderProxy(view->GetCamera()));
    AssertDebug(cameraProxy != nullptr);

    RI.cbufferAllocator->Write(&cameraProxy->bufferData);

    GpuBuffer* cbuffer;
    size_t cbufferOffset;
    size_t cbufferSize;
    RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    // this is rendered from translucent pass in DeferredRenderer
    Framebuffer* framebuffer = view->GetOutputTarget().GetFramebuffer(RenderBucket::Translucent);
    Assert(framebuffer != nullptr);

    { // update gpu particles pass (compute, done before frame is rendered)
        CommandRecorder& cr = frame->preRenderCommands;

        ShaderPropertySet properties;
        properties.Add(InternShaderProperty(ShaderProperty(NAME("MAX_PARTICLES"), int(state.maxParticles))));
        properties.Set(s_propHasPhysics, state.hasPhysics);

        cr << SetCurrentShader(ShaderDesc(NAME("UpdateParticles"), properties));

        cr << SetShaderUniform(0, "ParticlesBuffer"_sh, state.particleBuffer, ShaderDataOffset(0, sizeof(ParticleShaderData)));
        cr << SetShaderUniform(1, "IndirectDrawCommandsBuffer"_sh, state.indirectBuffer, ShaderDataOffset(0, sizeof(IndirectDrawCommand)));
        cr << SetShaderUniform(2, "NoiseMap"_sh, RI.textureViewCache->GetOrCreate(state.noiseMap));

        cr << SetShaderUniform(3, "SamplerNearest"_sh, RI.placeholderData->GetSamplerNearest());
        cr << SetShaderUniform(4, "SamplerLinear"_sh, RI.placeholderData->GetSamplerLinear());

        cr << SetShaderUniform(5, "GBufferAlbedoTexture"_sh, framebuffer->GetAttachment(GTN_ALBEDO)->GetImageView());
        cr << SetShaderUniform(6, "GBufferNormalsTexture"_sh, framebuffer->GetAttachment(GTN_NORMALS)->GetImageView());
        cr << SetShaderUniform(7, "GBufferMaterialTexture"_sh, framebuffer->GetAttachment(GTN_MATERIAL)->GetImageView());
        cr << SetShaderUniform(8, "GBufferVelocityTexture"_sh, framebuffer->GetAttachment(GTN_VELOCITY)->GetImageView());
        cr << SetShaderUniform(9, "GBufferDepthTexture"_sh, framebuffer->GetAttachment(GTN_DEPTH)->GetImageView());

        cr << SetShaderUniform(10, "WorldsBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);

        cr << SetShaderUniform(11, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

        const size_t maxParticles = proxy->bufferData.maxParticles;
        cr << DispatchCompute(Vec3u { uint32((maxParticles + 255) / 256), 1, 1 });

        cr << InsertBarrier(state.indirectBuffer, RS_INDIRECT_ARG);
    }

    state.fc = GetFrameCounter();

    { // draw particles pass
        CommandRecorder& cr = frame->cr;

        cr << SetInputLayout(state.renderableAttributes.GetMeshAttributes().inputLayout);
        cr << SetTopology(state.renderableAttributes.GetMeshAttributes().topology);

        cr << SetCurrentShader(ShaderDesc(
            state.renderableAttributes.GetMaterialAttributes().shaderName,
            state.renderableAttributes.GetMaterialAttributes().shaderProperties));

        cr << SetCurrentBlendFunction(state.renderableAttributes.GetMaterialAttributes().blendFunction);
        cr << SetFaceCullMode(state.renderableAttributes.GetMaterialAttributes().cullFaces);
        cr << SetFillMode(state.renderableAttributes.GetMaterialAttributes().fillMode);
        cr << SetDepthTest(bool(state.renderableAttributes.GetMaterialAttributes().flags & MAF_DEPTH_TEST));
        cr << SetDepthWrite(bool(state.renderableAttributes.GetMaterialAttributes().flags & MAF_DEPTH_WRITE));
        cr << SetStencilTest(bool(state.renderableAttributes.GetMaterialAttributes().flags & MAF_STENCIL_TEST));
        cr << SetStencilFunction(state.renderableAttributes.GetMaterialAttributes().stencilFunction);

        cr << SetShaderUniform(0, "ParticlesBuffer"_sh, state.particleBuffer, ShaderDataOffset(0, sizeof(ParticleShaderData)));

        if (proxy->particleTexture)
        {
            cr << SetShaderUniform(1, "ParticleTexture"_sh, RI.textureViewCache->GetOrCreate(proxy->particleTexture));
        }
        else
        {
            cr << SetShaderUniform(1, "ParticleTexture"_sh, RI.placeholderData->GetImageView2D1x1R8());
        }

        cr << SetShaderUniform(2, "SamplerLinear"_sh, RI.placeholderData->GetSamplerLinearMipmap());
        cr << SetShaderUniform(3, "WorldsBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);
        cr << SetShaderUniform(4, "CamerasBuffer"_sh, RI.namedBuffers[NamedBuffer::Cameras], Resources::GetBinding(view->GetCamera()));

        cr << CommitDrawState();

        cr << BindVertexBuffer(m_staging.quadMesh->GetVertexBuffer());
        cr << BindIndexBuffer(m_staging.quadMesh->GetIndexBuffer());
        cr << DrawIndexedIndirect(state.indirectBuffer, 0);

        // reset states
        cr << SetCurrentBlendFunction(BlendFunction::None());
        cr << SetDepthTest(true);
        cr << SetDepthWrite(true);
        cr << SetStencilTest(false);
        cr << SetFillMode(FM_FILL);
        cr << SetFaceCullMode(FCM_BACK);
    }
}

int ParticleVolumeRenderer::RunCleanupCycle(int maxIter)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    const uint32 currFrame = GetFrameCounter();

    int numCycles = 0;

    for (auto it = m_volumeStates.Begin(); it != m_volumeStates.End() && numCycles < maxIter;)
    {
        VolumeState& state = it->second;

        const int64 frameDiff = int64(currFrame) - int64(state.fc);

        if (frameDiff >= DiscardFrames)
        {
            it = m_volumeStates.Erase(it);

            ++numCycles;

            continue;
        }

        ++it;
    }

    return numCycles;
}

} // namespace Hyperion
