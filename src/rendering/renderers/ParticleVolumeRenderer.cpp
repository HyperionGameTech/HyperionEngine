/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/renderers/ParticleVolumeRenderer.hpp>

#include <rendering/RenderBackend.hpp>
#include <rendering/Frame.hpp>
#include <rendering/RenderQueue.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/GraphicsPipeline.hpp>
#include <rendering/ComputePipeline.hpp>
#include <rendering/GraphicsPipelineCache.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/RenderProxyList.hpp>
#include <rendering/RenderCollection.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/Texture.hpp>
#include <rendering/TextureViewCache.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/GBuffer.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <scene/ParticleVolume.hpp>
#include <scene/View.hpp>

#include <scene/camera/Camera.hpp>

#include <util/MeshBuilder.hpp>
#include <util/NoiseFactory.hpp>

#include <core/math/MathUtil.hpp>

#if HYP_VULKAN
#include <rendering/vulkan/VulkanStructs.hpp>
#endif

namespace Hyperion {

// How many frames until we release resources for unused volumes?
static constexpr uint32 DiscardFrames = 60;

ParticleVolumeRenderer::VolumeState::~VolumeState()
{
    SafeDelete(std::move(particleBuffer));
    SafeDelete(std::move(indirectBuffer));
    SafeDelete(std::move(uniformBuffers));
    SafeDelete(std::move(noiseMap));
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
        SafeDelete(std::move(m_staging.quadMesh));
    }

    SafeDelete(std::move(m_staging.zeroIndirectArgs));
}

Handle<PassData> ParticleVolumeRenderer::CreateViewPassData(View* view, PassDataExt&)
{
    Handle<PassData> pd = MakeHandle<PassData>();
    pd->view = MakeWeakRef(view);
    pd->viewport = view->GetViewport();

    return pd;
}

void ParticleVolumeRenderer::EnsureStaging()
{
    if (!m_staging.quadMesh.IsValid())
    {
        m_staging.quadMesh = MeshBuilder::Quad();
        m_staging.quadMesh->SetFlags(MF_VIEW_INDEPENDENT);
        InitObject(m_staging.quadMesh);
    }

    if (!m_staging.zeroIndirectArgs)
    {
        TByteBuffer<RenderAllocator> indirectDrawCommandsBuffer;
        g_renderBackend->PopulateIndirectDrawCommandsBuffer(
            m_staging.quadMesh->GetVertexBuffer(),
            m_staging.quadMesh->GetIndexBuffer(),
            0, indirectDrawCommandsBuffer);

        m_staging.zeroIndirectArgs = g_renderBackend->MakeGpuBuffer(GpuBufferType::STAGING_BUFFER, indirectDrawCommandsBuffer.Size());
        DeferCreate(m_staging.zeroIndirectArgs);

        m_staging.zeroIndirectArgs->Copy(indirectDrawCommandsBuffer.Size(), indirectDrawCommandsBuffer.Data());
    }
}

static void CreateNoiseMap(Handle<Texture>& tex)
{
    static constexpr uint32 Seed = 0xff;

    TextureDesc textureDesc {};
    textureDesc.extent = Vec3u { 128, 128, 1 };
    textureDesc.type = TT_TEX2D;
    textureDesc.format = TF_R8;
    textureDesc.filterModeMin = TFM_LINEAR;
    textureDesc.filterModeMag = TFM_LINEAR;

    TextureData textureData;

    Bitmap_R8 noiseMap = SimplexNoiseGenerator(Seed).CreateBitmap(128, 128, 1024.0f);
    textureData.imageData = ByteBuffer(noiseMap.ToByteView());

    if (tex)
    {
        SafeDelete(std::move(tex));
    }

    tex = MakeHandle<Texture>(textureDesc, textureData);
    InitObject(tex);
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

    state.particleBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::SSBO, state.maxParticles * sizeof(ParticleShaderData));
    DeferCreate(state.particleBuffer);

    state.indirectBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::INDIRECT_ARGS_BUFFER, sizeof(IndirectDrawCommand));
    DeferCreate(state.indirectBuffer);

    CreateNoiseMap(state.noiseMap);

    // compute shader properties
    ShaderProperties properties;
    state.hasPhysics = proxy->particleVolume.GetUnsafe()->GetParams().hasPhysics;
    properties.Set(NAME("HAS_PHYSICS"), state.hasPhysics);
    properties.Set(ShaderProperty(NAME("MAX_PARTICLES"), int(state.maxParticles)));

    // set default particle graphics attributes (translucent)
    MaterialAttributes materialAttributes {};
    materialAttributes.shaderDefinition = ShaderDefinition { NAME("Particle"), properties };
    materialAttributes.bucket = RB_TRANSLUCENT;
    materialAttributes.blendFunction = BlendFunction::AlphaBlending();
    materialAttributes.cullFaces = FCM_FRONT;
    materialAttributes.flags = MAF_DEPTH_TEST; // depth test on, depth write off by default

    MeshAttributes meshAttributes {};
    meshAttributes.vertexAttributes = VertexAttribute::Position | VertexAttribute::Normal | VertexAttribute::TexCoord0;
    meshAttributes.indexBufferElemType = GET_UNSIGNED_INT;
    meshAttributes.topology = TOP_TRIANGLES;
    state.renderableAttributes = RenderableAttributeSet(meshAttributes, materialAttributes);

    return state;
}

void ParticleVolumeRenderer::RenderFrame(Frame* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    HYP_NAMED_SCOPE("Render particle volume");

    AssertDebug(renderSetup.world && renderSetup.view && renderSetup.volume);

    ParticleVolume* particleVolume = ObjCast<ParticleVolume>(renderSetup.volume);
    AssertDebug(particleVolume != nullptr);

    EnsureStaging();

    View* view = renderSetup.view;
    RenderProxyList& rpl = RenderApi::GetConsumerProxyList(view);

    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    // Reset zero staging buffer state
    frame->preRenderQueue << InsertBarrier(m_staging.zeroIndirectArgs, RS_COPY_SRC);

    RenderProxyParticleVolume* proxy = static_cast<RenderProxyParticleVolume*>(RenderApi::GetRenderProxy(particleVolume));
    AssertDebug(proxy != nullptr);

    VolumeState& state = EnsureVolumeState(proxy);

    // zero indirect arguments (instance count)
    Assert(state.indirectBuffer->Size() == sizeof(IndirectDrawCommand));

    { // zero out indirect buffer (ahead of frame compute + rendering)
        RenderQueue& rq = frame->preRenderQueue;

        rq << InsertBarrier(state.indirectBuffer, RS_COPY_DST);
        rq << CopyBuffer(m_staging.zeroIndirectArgs, state.indirectBuffer, sizeof(IndirectDrawCommand));
        rq << InsertBarrier(state.indirectBuffer, RS_INDIRECT_ARG);
    }

    // bind and dispatch compute
    struct ParticleSpawnerUniforms
    {
        Vec4f origin;
        float spawnRadius;
        float randomness;
        float avgLifespan;
        uint32 maxParticles;
        float maxParticlesSqrt;
        float deltaTime;
        uint32 globalCounter;
    };

    ParticleSpawnerUniforms uniforms {};
    uniforms.origin = proxy->bufferData.originStartSize;
    uniforms.spawnRadius = proxy->bufferData.spawnRadius;
    uniforms.randomness = proxy->bufferData.randomness;
    uniforms.avgLifespan = proxy->bufferData.avgLifespan;
    uniforms.maxParticles = proxy->bufferData.maxParticles;
    uniforms.maxParticlesSqrt = proxy->bufferData.maxParticlesSqrt;
    uniforms.deltaTime = 0.016f; // TODO: real render delta
    uniforms.globalCounter = m_counter++;

    GpuBufferRef& uniformBuffer = state.uniformBuffers[frame->GetFrameIndex()];
    if (!uniformBuffer)
    {
        uniformBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::CBUFF, sizeof(uniforms));
        CheckResult(uniformBuffer->Create());
    }

    uniformBuffer->Copy(sizeof(uniforms), &uniforms);
    uniformBuffer->Flush(0, sizeof(uniforms));

    // this is rendered from translucent pass in DeferredRenderer
    Framebuffer* framebuffer = view->GetOutputTarget().GetFramebuffer(RB_TRANSLUCENT);
    Assert(framebuffer != nullptr);

    { // update gpu particles pass (compute, done before frame is rendered)
        RenderQueue& rq = frame->preRenderQueue;

        ShaderProperties properties;
        properties.Set(NAME("HAS_PHYSICS"), state.hasPhysics);
        properties.Set(ShaderProperty(NAME("MAX_PARTICLES"), int(state.maxParticles)));

        rq << SetCurrentShader(ShaderDesc(ShaderDefinition(NAME("UpdateParticles"), properties)));

        rq << SetShaderUniform(0, "ParticlesBuffer"_sh, state.particleBuffer); 
        rq << SetShaderUniform(1, "IndirectDrawCommandsBuffer"_sh, state.indirectBuffer);
        rq << SetShaderUniform(2, "NoiseMap"_sh, g_renderInterface->textureViewCache->GetOrCreate(state.noiseMap));

        rq << SetShaderUniform(3, "SamplerNearest"_sh, g_renderInterface->placeholderData->GetSamplerNearest());
        rq << SetShaderUniform(4, "SamplerLinear"_sh, g_renderInterface->placeholderData->GetSamplerLinear());

        rq << SetShaderUniform(5, "GBufferAlbedoTexture"_sh, framebuffer->GetAttachment(GTN_ALBEDO)->GetImageView());
        rq << SetShaderUniform(6, "GBufferNormalsTexture"_sh, framebuffer->GetAttachment(GTN_NORMALS)->GetImageView());
        rq << SetShaderUniform(7, "GBufferMaterialTexture"_sh, framebuffer->GetAttachment(GTN_MATERIAL)->GetImageView());
        rq << SetShaderUniform(8, "GBufferVelocityTexture"_sh, framebuffer->GetAttachment(GTN_VELOCITY)->GetImageView());
        rq << SetShaderUniform(9, "GBufferDepthTexture"_sh, framebuffer->GetAttachment(GTN_DEPTH)->GetImageView());
        
        rq << SetShaderUniform(10, "WorldsBuffer"_sh, g_renderInterface->gpuBuffers[GRB_WORLDS]->GetBuffer(frame->GetFrameIndex()));
        
        rq << SetShaderUniform(11, "CamerasBuffer"_sh, g_renderInterface->gpuBuffers[GRB_CAMERAS]->GetBuffer(frame->GetFrameIndex()), ShaderDataOffset<CameraShaderData>(view->GetCamera()));
        
        rq << SetShaderUniform(12, "ParticleSpawnerData"_sh, uniformBuffer);

        const SizeType maxParticles = proxy->bufferData.maxParticles;
        rq << DispatchCompute(Vec3u { uint32((maxParticles + 255) / 256), 1, 1 });

        rq << InsertBarrier(state.indirectBuffer, RS_INDIRECT_ARG);
    }

    state.fc = RenderApi::GetFrameCounter();

    { // draw particles pass
        RenderQueue& rq = frame->renderQueue;

        rq << SetVertexAttributes(state.renderableAttributes.GetMeshAttributes().vertexAttributes);
        rq << SetTopology(state.renderableAttributes.GetMeshAttributes().topology);

        rq << SetCurrentShader(ShaderDesc(state.renderableAttributes.GetMaterialAttributes().shaderDefinition));

        rq << SetCurrentBlendFunction(state.renderableAttributes.GetMaterialAttributes().blendFunction);
        rq << SetFaceCullMode(state.renderableAttributes.GetMaterialAttributes().cullFaces);
        rq << SetFillMode(state.renderableAttributes.GetMaterialAttributes().fillMode);
        rq << SetDepthTest(bool(state.renderableAttributes.GetMaterialAttributes().flags & MAF_DEPTH_TEST));
        rq << SetDepthWrite(bool(state.renderableAttributes.GetMaterialAttributes().flags & MAF_DEPTH_WRITE));
        rq << SetStencilTest(bool(state.renderableAttributes.GetMaterialAttributes().flags & MAF_STENCIL_TEST));
        rq << SetStencilFunction(state.renderableAttributes.GetMaterialAttributes().stencilFunction);

        rq << SetShaderUniform(0, "ParticlesBuffer"_sh, state.particleBuffer);

        if (proxy->particleTexture)
        {
            rq << SetShaderUniform(1, "ParticleTexture"_sh, g_renderInterface->textureViewCache->GetOrCreate(proxy->particleTexture));
        }
        else
        {
            rq << SetShaderUniform(1, "ParticleTexture"_sh, g_renderInterface->placeholderData->GetImageView2D1x1R8());
        }

        rq << SetShaderUniform(2, "SamplerLinear"_sh, g_renderInterface->placeholderData->GetSamplerLinearMipmap());
        rq << SetShaderUniform(3, "WorldsBuffer"_sh, g_renderInterface->gpuBuffers[GRB_WORLDS]->GetBuffer(frame->GetFrameIndex()));
        rq << SetShaderUniform(4, "CamerasBuffer"_sh, g_renderInterface->gpuBuffers[GRB_CAMERAS]->GetBuffer(frame->GetFrameIndex()), ShaderDataOffset<CameraShaderData>(view->GetCamera()));

        rq << CommitDrawState();

        rq << BindVertexBuffer(m_staging.quadMesh->GetVertexBuffer());
        rq << BindIndexBuffer(m_staging.quadMesh->GetIndexBuffer());
        rq << DrawIndexedIndirect(state.indirectBuffer, 0);

        // reset states
        rq << SetCurrentBlendFunction(BlendFunction::None());
        rq << SetDepthTest(true);
        rq << SetDepthWrite(true);
        rq << SetStencilTest(false);
        rq << SetFillMode(FM_FILL);
        rq << SetFaceCullMode(FCM_BACK);
    }
}

int ParticleVolumeRenderer::RunCleanupCycle(int maxIter)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    const uint32 currFrame = RenderApi::GetFrameCounter();

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
