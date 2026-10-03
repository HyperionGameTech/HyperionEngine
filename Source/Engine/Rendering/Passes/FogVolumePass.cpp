/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Passes/FogVolumePass.hpp>
#include <Rendering/Passes/DeferredPass.hpp>
#include <Rendering/Passes/DeferredPassShared.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/GBuffer.hpp>
#include <Rendering/Mesh.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/TextureViewCache.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/RenderTypes.hpp>
#include <Rendering/RenderProxyList.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/GpuBuffer.hpp>
#include <Rendering/CBufferAllocator.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/DepthPyramidRenderer.hpp>
#include <Rendering/CommandRecorder.hpp>
#include <Rendering/RawBufferAllocator.hpp>

#include <Rendering/Shadows/ShadowMapCache.hpp>
#include <Rendering/Shadows/ShadowMap.hpp>

#include <Rendering/Clouds/CloudPass.hpp>

#include <Rendering/Glimmer/GlimmerPass.hpp>
#include <Rendering/Glimmer/GlimmerTechnique.hpp>

#include <Rendering/Util/DeletionQueue.hpp>
#include <Rendering/Util/MeshBuilder.hpp>
#include <Rendering/Util/ShaderPropertyDictionary.hpp>

#include <Scene/FogVolume.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Utilities/DeferredScope.hpp>

#include <Framework/EngineDriver.hpp>
#include <Framework/CVarManager.hpp>
#include <Framework/View.hpp>
#include <Framework/EngineStats.hpp>

namespace Hyperion {

static constexpr uint32 MaxFogLights = 4;

static constexpr uint32 FogMaxSteps = 48;
static constexpr float FogMinStepSize = 0.25f;
static constexpr float FogHistoryWeight = 0.9f;

static EngineStatGpuTimer s_statFogVolumes("Rendering/GPU/FogVolumes");

struct FogVolumeMarchConstants
{
    Vec2i screenDimensions;
    float minStepSize;
    uint32 maxSteps;
    uint32 frameCounter;
    uint32 _pad[3];
};

static_assert(sizeof(FogVolumeMarchConstants) == 32);

// Must match FogVolumeTemporalConstants in Shaders/Deferred/FogVolumeTemporal.hlsl
struct FogVolumeTemporalConstants
{
    Vec4u params; // xy = extent, z = 1 when the history is usable
    Vec4f blend;  // x = history weight
};

static StaticShaderPropertyId s_propUseClusteredLights { ShaderProperty(NAME("CLUSTERED_LIGHTS")) };
static StaticShaderPropertyId s_propFogVolumeUseSDF { ShaderProperty(NAME("FOG_VOLUME_USE_SDF")) };

extern CVar<bool> g_cvFogVolumesClusteredLights;

#pragma region FogVolumePass

FogVolumePass::FogVolumePass(Vec2u extent, GBuffer* gbuffer)
    : FullScreenPass(TextureFormat::RGBA16F, MathUtil::Max(extent / 4, Vec2u::One()), gbuffer)
{
    SetPassName(NAME("FogVolume"));
}

FogVolumePass::~FogVolumePass()
{
    for (Handle<Texture>& historyTexture : m_historyTextures)
    {
        EnqueueDeletion(std::move(historyTexture));
    }
}

void FogVolumePass::Create()
{
    AssertOnThread(g_renderThread);

    m_volumeMesh = MeshBuilder::Cube();
    m_volumeMesh->SetIsTransient(true);
    m_volumeMesh->SetFlags(MeshFlags::ViewIndependent);
    m_volumeMesh->SetName(NAME("FogVolumeMesh"));
    m_volumeMesh->UploadGpuData();

    m_shaderDesc = ShaderDesc(NAME("ApplyFogVolume"));

    FullScreenPass::Create();

    // Upsampling
    for (uint32 i = 0; i < NumUpsamplePasses; i++)
    {
        const bool isLast = i == NumUpsamplePasses - 1;

        Vec2u targetExtent = m_gbuffer->GetExtent();

        if (!isLast)
        {
            targetExtent /= (2 * (NumUpsamplePasses - i - 1));
        }

        targetExtent = MathUtil::Max(targetExtent, Vec2u::One());

        const TextureFormat format = GetFormat();

        m_upsamplePasses[i] = MakeUnique<FullScreenPass>(
            format,
            targetExtent,
            nullptr,
            isLast ? FSP_EXTERNAL_RENDERTARGET : FSP_NONE);

        m_upsamplePasses[i]->SetShaderDesc(ShaderDesc(NAME("Upsample"), ShaderPropertySet {}));
        m_upsamplePasses[i]->Create();
    }

    CreateHistoryTextures();
}

void FogVolumePass::Resize_Internal(Vec2u newSize)
{
    FullScreenPass::Resize_Internal(newSize);

    CreateHistoryTextures();
}

void FogVolumePass::CreateHistoryTextures()
{
    for (Handle<Texture>& historyTexture : m_historyTextures)
    {
        if (historyTexture.IsValid())
        {
            EnqueueDeletion(std::move(historyTexture));
        }

        historyTexture = MakeHandle<Texture>(TextureDesc {
            TextureType::Texture2D,
            TextureFormat::RGBA16F,
            Vec3u(m_extent, 1),
            TextureFilterMode::Linear,
            TextureFilterMode::Linear,
            TextureWrapMode::ClampToEdge,
            1,
            ImageUsage::Storage | ImageUsage::Sampled });

        historyTexture->SetIsTransient(true);
        historyTexture->SetName(NAME("FogVolumeHistory"));
        Check(historyTexture->Create());
    }

    m_historyValid = false;
}

void FogVolumePass::ResolveTemporal(Frame* frame, const RenderSetup& renderSetup)
{
    CommandRecorder& cr = frame->cr;

    const Handle<Texture>& historyTexture = m_historyTextures[m_historyIndex];
    const Handle<Texture>& outTexture = m_historyTextures[m_historyIndex ^ 1u];

    FogVolumeTemporalConstants constants {};
    constants.params = Vec4u(m_extent.x, m_extent.y, m_historyValid ? 1u : 0u, 0u);
    constants.blend = Vec4f(FogHistoryWeight, 0.0f, 0.0f, 0.0f);

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    RI.cbufferAllocator->Write(&constants);
    RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    AttachmentBase* currentTexture = GetAttachment(0);

    cr << InsertBarrier(currentTexture->GetGpuImage(), ResourceState::ShaderResource, ShaderModuleType::Compute);
    cr << InsertBarrier(historyTexture->GetGpuImage(), ResourceState::ShaderResource, ShaderModuleType::Compute);
    cr << InsertBarrier(outTexture->GetGpuImage(), ResourceState::UnorderedAccess, ShaderModuleType::Compute);

    cr << SetCurrentShader(ShaderDesc(NAME("FogVolumeTemporal")));

    uint32 uniformIndex = 0;

    cr << SetShaderUniform(uniformIndex++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
    cr << SetShaderUniform(uniformIndex++, "CurrentTexture"_sh, currentTexture->GetImageView());
    cr << SetShaderUniform(uniformIndex++, "HistoryTexture"_sh, RI.textureViewCache->GetOrCreate(historyTexture));
    cr << SetShaderUniform(uniformIndex++, "VelocityTexture"_sh, m_gbuffer->GetPass(GBufferPass::Opaque).GetAttachment(GBufferTarget::Velocity)->GetImageView());
    cr << SetShaderUniform(uniformIndex++, "SamplerLinear"_sh, RI.placeholderData->GetSamplerLinear());
    cr << SetShaderUniform(uniformIndex++, "OutTexture"_sh, RI.textureViewCache->GetOrCreate(outTexture));

    cr << DispatchCompute(Vec3u { (m_extent.x + 7) / 8, (m_extent.y + 7) / 8, 1 });

    cr << InsertBarrier(outTexture->GetGpuImage(), ResourceState::ShaderResource);

    m_historyIndex ^= 1u;
    m_historyValid = true;
}

void FogVolumePass::Render(Frame* frame, const RenderSetup& renderSetup)
{
    AssertDebug(renderSetup.world && renderSetup.view && renderSetup.framebuffer);

    RenderProxyList& rpl = GetConsumerProxyList(renderSetup.view);
    rpl.BeginRead();

    HYP_DEFER({ rpl.EndRead(); });

    if (rpl.GetFogVolumes().NumCurrent() == 0)
    {
        m_historyValid = false;

        return;
    }

    ENGINE_STAT_GPU_SCOPE(&s_statFogVolumes);

    RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(GetRenderProxy(renderSetup.view->GetCamera()));
    if (!cameraProxy)
    {
        return;
    }

    DeferredPassData* dpd = DynamicCast<DeferredPassData>(renderSetup.passData);
    AssertDebug(dpd != nullptr);

    Attachment* normalsAttachment = m_gbuffer->GetPass(GBufferPass::Opaque).GetAttachment(GBufferTarget::Normals);

    // Directional light
    LightShaderData directionalLightShaderData {};
    DirectionalLightCSMData directionalCSMData {};

    for (Light* light : rpl.GetLights())
    {
        if (light->GetLightType() == LightType::Directional)
        {
            RenderProxyLight* lightProxy = static_cast<RenderProxyLight*>(GetRenderProxy(light));
            AssertDebug(lightProxy != nullptr);

            directionalLightShaderData = lightProxy->bufferData;

            ShadowMap* shadowMaps[MaxShadowMapCascades] {};
            View* shadowMapViewsDynamic[MaxShadowMapCascades] {};
            View* shadowMapViewsStatic[MaxShadowMapCascades] {};

            uint32 numCascades = MathUtil::Clamp(lightProxy->numCascades, 1u, MaxShadowMapCascades);

            for (uint32 cascadeIndex = 0; cascadeIndex < numCascades; cascadeIndex++)
            {
                shadowMaps[cascadeIndex] = RI.shadowMapCache->GetShadowMap(
                    light,
                    renderSetup.view,
                    cascadeIndex,
                    shadowMapViewsDynamic[cascadeIndex],
                    shadowMapViewsStatic[cascadeIndex]);
            }

            DeferredRendererHelpers::FillShadowMapDataCSM(
                &directionalCSMData,
                shadowMapViewsDynamic,
                shadowMapViewsStatic,
                shadowMaps,
                numCascades);

            break;
        }
    }

    CommandRecorder& cr = frame->cr;
    
    cr << SetFillMode(FillMode::Fill);
    cr << SetDepthWrite(false);
    cr << SetDepthTest(false);
    cr << SetStencilTest(false);
    cr << SetCurrentBlendFunction(BlendFunction(BlendModeFactor::One, BlendModeFactor::OneMinusSrcAlpha, BlendModeFactor::One, BlendModeFactor::OneMinusSrcAlpha));

    cr << SetCurrentViewport(Viewport { m_extent, renderSetup.viewport.position });

    cr << SetCurrentFramebuffer(m_framebuffer);

    cr << SetTopology(m_volumeMesh->GetMeshAttributes().topology);
    cr << SetInputLayout(m_volumeMesh->GetMeshAttributes().inputLayout);

    cr << SetFaceCullMode(FaceCullMode::Front);

    const bool useClusteredLights = g_cvFogVolumesClusteredLights.Get();

    {
        ShaderPropertySet fogShaderProperties;

        // fogShaderProperties.Add(s_propFogVolumeUseSDF);

        if (useClusteredLights)
        {
            fogShaderProperties.Add(s_propUseClusteredLights);
        }

        cr << SetCurrentShader(ShaderDesc(NAME("ApplyFogVolume"), fogShaderProperties));
    }

    cr << SetShaderUniform(0, "SamplerLinear"_sh, RI.placeholderData->GetSamplerLinearMipmap());
    cr << SetShaderUniform(1, "SamplerNearest"_sh, RI.placeholderData->GetSamplerNearest());

    cr << SetShaderUniform(2, "CamerasBuffer"_sh, RI.namedBuffers[NamedBuffer::Cameras], Resources::GetBinding(renderSetup.view->GetCamera()));

    cr << SetShaderUniform(3, "ShadowMapsTextureArray"_sh, RI.shadowMapCache->GetAtlasImageView());
    cr << SetShaderUniform(4, "PointLightShadowMapsTextureArray"_sh, RI.shadowMapCache->GetPointLightShadowMapImageView());

    cr << SetShaderUniform(5, "DepthTexture"_sh, RI.textureViewCache->GetOrCreate(dpd->depthPyramidRenderer->GetHZBTexture(), 2, 1));

    cr << SetShaderUniform(6, "BlueNoiseBuffer"_sh, RI.blueNoiseBuffer);

    cr << SetShaderUniform(7, "LightsBuffer"_sh, RI.namedBuffers[NamedBuffer::Lights]);
    cr << SetShaderUniform(8, "EnvProbesBuffer"_sh, RI.namedBuffers[NamedBuffer::EnvProbes]);

    cr << SetShaderUniform(9, "ClusterGridBuffer"_sh, *dpd->gridTilesBuffer);
    cr << SetShaderUniform(10, "ClusterIndexBuffer"_sh, *dpd->gridIndexBuffer);

    if (dpd->clusteredShadowMapIndexBuffer == nullptr)
    {
        // We need this here because if path tracing is active, the deferred pass doesn't set it
        dpd->clusteredShadowMapIndexBuffer = &RI.bufferAllocator->AcquireByteAddressBuffer(sizeof(uint32));
    }

    cr << SetShaderUniform(11, "ShadowMapIndexBuffer"_sh, *dpd->clusteredShadowMapIndexBuffer);

    cr << SetShaderUniform(15, "CloudWeatherMapTexture"_sh, dpd->cloudPass->GetWeatherMapView());
    cr << SetShaderUniform(16, "CloudShadowMapTexture"_sh, dpd->cloudPass->GetShadowMapView());

    GlimmerPass* glimmerPass = StaticCast<GlimmerPass>(RI.namedPasses[NamedPass::Glimmer][0]);
    const uint32 worldsBufferIndex = glimmerPass->BindApplyResources(cr, 17, renderSetup.world);
    cr << SetShaderUniform(worldsBufferIndex, "WorldsBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);

    RenderProxyEnvProbe* skyProbeProxy = renderSetup.envProbe != nullptr
        ? static_cast<RenderProxyEnvProbe*>(GetRenderProxy(renderSetup.envProbe))
        : nullptr;

    LightShaderData fogLightData[MaxFogLights] {};
    ShadowMapData fogShadowMapData[MaxFogLights] {};
    uint32 numFogLights = 0;

    if (!useClusteredLights)
    {
        for (Light* light : rpl.GetLights())
        {
            const LightType lightType = light->GetLightType();

            if (lightType == LightType::Directional)
            {
                continue;
            }

            if (lightType != LightType::Point && lightType != LightType::Spot)
            {
                continue;
            }

            if (numFogLights >= MaxFogLights)
            {
                break;
            }

            RenderProxyLight* lightProxy = static_cast<RenderProxyLight*>(GetRenderProxy(light));
            AssertDebug(lightProxy != nullptr);

            fogLightData[numFogLights] = lightProxy->bufferData;

            View* shadowMapViewDynamic;
            View* shadowMapViewStatic;

            ShadowMap* shadowMap = RI.shadowMapCache->GetShadowMap(
                light,
                renderSetup.view,
                0,
                shadowMapViewDynamic,
                shadowMapViewStatic);

            if (shadowMap != nullptr)
            {
                DeferredRendererHelpers::FillShadowMapData(
                    fogShadowMapData[numFogLights],
                    *shadowMap,
                    0,
                    shadowMapViewDynamic,
                    shadowMapViewStatic);
            }

            ++numFogLights;
        }
    }

    for (FogVolume* volume : rpl.GetFogVolumes())
    {
        RenderProxyFogVolume* proxy = static_cast<RenderProxyFogVolume*>(GetRenderProxy(volume));
        Assert(proxy != nullptr);

        FogVolumePassData& data = GetFogVolumePassData(volume);
        data.noiseTexture = proxy->noiseTexture;
        data.volumeTexture = proxy->volumeTexture;

        {
            if (data.volumeTexture)
            {
                cr << SetShaderUniform(12, "DataMap"_sh, RI.textureViewCache->GetOrCreate(data.volumeTexture));
            }

            if (data.noiseTexture)
            {
                cr << SetShaderUniform(13, "NoiseMap"_sh, RI.textureViewCache->GetOrCreate(data.noiseTexture));
            }

            FogVolumeShaderData shaderData = proxy->bufferData;

            if (!useClusteredLights)
            {
                shaderData.numBoundLights = numFogLights;
            }

            GpuBuffer* cbuffer = nullptr;
            size_t cbufferOffset = 0;
            size_t cbufferSize = 0;

            RI.cbufferAllocator->Write(&shaderData);
            RI.cbufferAllocator->Write(&directionalLightShaderData);
            RI.cbufferAllocator->Write(&directionalCSMData);
            dpd->cloudPass->WriteShaderData(*RI.cbufferAllocator);

            if (useClusteredLights)
            {
                for (uint32 i = 0; i < MaxClusteredShadowMaps; i++)
                {
                    if (i < dpd->numClusteredShadowMaps)
                    {
                        RI.cbufferAllocator->Write(&dpd->clusteredShadowMaps[i]);
                    }
                    else
                    {
                        ShadowMapData dummy {};
                        RI.cbufferAllocator->Write(&dummy);
                    }
                }
            }
            else
            {
                for (uint32 i = 0; i < MaxFogLights; i++)
                {
                    RI.cbufferAllocator->Write(&fogLightData[i]);
                }

                for (uint32 i = 0; i < MaxFogLights; i++)
                {
                    RI.cbufferAllocator->Write(&fogShadowMapData[i]);
                }
            }

            FogVolumeMarchConstants marchConstants {};
            marchConstants.screenDimensions = Vec2i(m_extent);
            marchConstants.minStepSize = FogMinStepSize;
            marchConstants.maxSteps = FogMaxSteps;
            marchConstants.frameCounter = GetFrameCounter();
            RI.cbufferAllocator->Write(&marchConstants);

            if (skyProbeProxy != nullptr)
            {
                RI.cbufferAllocator->Write(&skyProbeProxy->bufferData);
            }
            else
            {
                // default constructed, so textureIndices is ~0u
                static const EnvProbeShaderData s_noSkyProbeData {};
                RI.cbufferAllocator->Write(&s_noSkyProbeData);
            }

            glimmerPass->WriteApplyShaderData(*RI.cbufferAllocator, renderSetup.world);

            RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

            cr << SetShaderUniform(14, "FogVolumeConstants"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

            cr << CommitDrawState();

            cr << BindVertexBuffer(m_volumeMesh->GetVertexBuffer(0));
            cr << BindIndexBuffer(m_volumeMesh->GetIndexBuffer(0));
            cr << DrawIndexed(36); // draw cube
        }
    }

    cr << SetFaceCullMode(FaceCullMode::None);
    cr << SetCurrentBlendFunction(BlendFunction::None());

    ResolveTemporal(frame, renderSetup);

    const Handle<Texture>& resolvedTexture = m_historyTextures[m_historyIndex];

    // Now upsampling passes
    for (uint32 i = 0; i < NumUpsamplePasses; i++)
    {
        const bool isFirst = i == 0;
        const bool isLast = i == NumUpsamplePasses - 1;

        FullScreenPass* pass = m_upsamplePasses[i].Get();

        const Vec2f sourceResolution = MathUtil::Max(Vec2f(pass->GetExtent()) / 2, Vec2f::One());

        // Need new cbuffer
        GpuBuffer* cbuffer = nullptr;
        size_t cbufferSize = 0;
        size_t cbufferOffset = 0;

        { // Update constant buffer
            struct UpsampleConstants
            {
                CameraShaderData camera;

                Vec2f texelSize;
                Vec2f uvScale;
                float depthThreshold;
                float normalThreshold;
            };

            UpsampleConstants upsampleConstants {};
            upsampleConstants.camera = cameraProxy->bufferData;
            upsampleConstants.texelSize = Vec2f::One() / sourceResolution;
            upsampleConstants.uvScale = Vec2f::One();
            upsampleConstants.depthThreshold = 0.1f;
            upsampleConstants.normalThreshold = 1.0f;

            RI.cbufferAllocator->Write(&upsampleConstants);
            RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);
        }

        cr << SetCurrentShader(pass->GetShaderDesc());

        // if last - direct to framebuffer, over the scene
        // what's behind the fog shows through by its transmittance
        if (isLast)
        {
            cr << SetCurrentBlendFunction(BlendFunction(BlendModeFactor::One, BlendModeFactor::OneMinusSrcAlpha, BlendModeFactor::Zero, BlendModeFactor::One));
            cr << SetCurrentFramebuffer(renderSetup.framebuffer);
            cr << SetCurrentViewport(renderSetup.viewport);

        }
        else
        {
            cr << SetCurrentFramebuffer(pass->GetFramebuffer());
            cr << SetCurrentViewport(Viewport { pass->GetExtent() });
        }

        uint32 numShaderUniforms = 0;

        // Samplers
        cr << SetShaderUniform(numShaderUniforms++, "SamplerLinear"_sh, RI.placeholderData->GetSamplerLinear());
        cr << SetShaderUniform(numShaderUniforms++, "SamplerNearest"_sh, RI.placeholderData->GetSamplerNearest());

        // GBuffer textures
        cr << SetShaderUniform(numShaderUniforms++, "NormalsTexture"_sh, normalsAttachment->GetImageView());

        cr << SetShaderUniform(
            numShaderUniforms++,
            "DepthTexture"_sh,
            RI.textureViewCache->GetOrCreate(dpd->depthPyramidRenderer->GetHZBTexture(), NumUpsamplePasses - i - 1, 1));

        cr << SetShaderUniform(
            numShaderUniforms++,
            "PrevPassTexture"_sh,
            isFirst ? RI.textureViewCache->GetOrCreate(resolvedTexture) : m_upsamplePasses[i - 1]->GetAttachment(0)->GetImageView());

        cr << SetShaderUniform(numShaderUniforms++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

        cr << CommitDrawState();

        // Draw quad
        pass->RenderFullScreenQuad(frame, renderSetup);
    }

    // reset states
    cr << SetCurrentBlendFunction(BlendFunction::None());
    cr << SetDepthTest(true);
    cr << SetDepthWrite(true);

    m_isFirstFrame = false;
}

#pragma endregion FogVolumePass

} // namespace Hyperion
