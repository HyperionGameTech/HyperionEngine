/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <rendering/renderers/DeferredRenderer.hpp>
#include <rendering/renderers/EnvProbeRenderer.hpp>
#include <rendering/renderers/UIRenderer.hpp>

#include <rendering/RenderGroup.hpp>
#include <rendering/MaterialTextureCache.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/DepthPyramidRenderer.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/GraphicsPipelineCache.hpp>
#include <rendering/passes/SSRPass.hpp>
#include <rendering/SSGI.hpp>
#include <rendering/passes/HBAOPass.hpp>
#include <rendering/DepthOfField.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/MaterialInstance.hpp>
#include <rendering/Texture.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/GpuBuffer.hpp>
#include <rendering/Device.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/GraphicsPipeline.hpp>
#include <rendering/Swapchain.hpp>
#include <rendering/RenderCollection.hpp>
#include <rendering/RenderProxyList.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/TextureViewCache.hpp>
#include <rendering/SamplerCache.hpp>
#include <rendering/BLASCache.hpp>
#include <rendering/AccelerationStructure.hpp>
#include <rendering/RayTracingPipeline.hpp>
#include <rendering/MeshBlasBuilder.hpp>
#include <rendering/RayTracingReflections.hpp>
#include <rendering/DDGI.hpp>
#include <rendering/CBufferAllocator.hpp>
#include <rendering/StructuredBufferAllocator.hpp>

#include <rendering/shadows/ShadowMapAllocator.hpp>
#include <rendering/shadows/ShadowMapCache.hpp>
#include <rendering/shadows/ShadowMap.hpp>

#include <rendering/util/ShaderCompiler.hpp>
#include <rendering/util/DeletionQueue.hpp>

#include <rendering/DebugDrawer.hpp>

#include <scene/World.hpp>
#include <scene/View.hpp>
#include <scene/EnvGrid.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/FogVolume.hpp>
#include <scene/ParticleVolume.hpp>
#include <scene/LightmapVolume.hpp>

#include <engine/CVarManager.hpp>

#include <engine/config/EngineConfig.hpp>

#include <Core/config/Config.hpp>

#include <Core/filesystem/FsUtil.hpp>

#include <Core/utilities/DeferredScope.hpp>
#include <Core/utilities/Float16.hpp>

#include <system/AppContext.hpp>

#include <util/MeshBuilder.hpp>

#include <engine/EngineDriver.hpp>
#include <engine/EngineStats.hpp>

#include <DeferredRenderer.generated.inl>

namespace Hyperion {

static constexpr float CameraJitterScale = 0.25f;

static constexpr uint32 TileSize = 32;
static constexpr uint32 TileZBins = 16;

static const Float16 s_ltcMatrix[] = {
#include <rendering/inl/LTCMatrix.inl>
};

static_assert(sizeof(s_ltcMatrix) == 64 * 64 * 4 * 2, "Invalid LTC matrix size");

static const Float16 s_ltcBrdf[] = {
#include <rendering/inl/LTCBRDF.inl>
};

static_assert(sizeof(s_ltcBrdf) == 64 * 64 * 4 * 2, "Invalid LTC BRDF size");

// Maps individual light types to per-light specific properties.
static const FixedArray<ShaderPropertySet, NumLightTypes> s_deferredLightTypeProperties {
    ShaderPropertySet { { InternShaderProperty(ShaderProperty(NAME("LIGHT_TYPE"), NAME("DIRECTIONAL"))) } },
    ShaderPropertySet { { InternShaderProperty(ShaderProperty(NAME("LIGHT_TYPE"), NAME("POINT"))) } },
    ShaderPropertySet { { InternShaderProperty(ShaderProperty(NAME("LIGHT_TYPE"), NAME("SPOT"))) } },
    ShaderPropertySet { { InternShaderProperty(ShaderProperty(NAME("LIGHT_TYPE"), NAME("AREA_RECT"))) } }
};

static const ShaderPropertyId s_propHasDiffuseMap = InternShaderProperty(ShaderProperty(NAME("HAS_DIFFUSE_MAP")));

static const ShaderPropertyId s_propHBAOEnabled = InternShaderProperty(ShaderProperty(NAME("HBAO_ENABLED")));
static const ShaderPropertyId s_propSSAOEnabled = InternShaderProperty(ShaderProperty(NAME("SSAO_ENABLED")));
static const ShaderPropertyId s_propSSGIEnabled = InternShaderProperty(ShaderProperty(NAME("SSGI_ENABLED")));
static const ShaderPropertyId s_propSSREnabled = InternShaderProperty(ShaderProperty(NAME("SSR_ENABLED")));

static const ShaderPropertyId s_propRayTracingReflections = InternShaderProperty(ShaderProperty(NAME("RT_REFLECTIONS")));
static const ShaderPropertyId s_propRayTracingGlobalIllumination = InternShaderProperty(ShaderProperty(NAME("RT_GI")));
static const ShaderPropertyId s_propPathTracer = InternShaderProperty(ShaderProperty(NAME("PATHTRACER")));

static const ShaderPropertyId s_propDebugReflections = InternShaderProperty(ShaderProperty(NAME("DEBUG_REFLECTIONS")));
static const ShaderPropertyId s_propDebugIrradiance = InternShaderProperty(ShaderProperty(NAME("DEBUG_IRRADIANCE")));

static const ShaderPropertyId s_propOutputSDR = InternShaderProperty(ShaderProperty(NAME("OUTPUT"), NAME("SDR")));

static const ShaderPropertyId s_propLightTypeClustered = InternShaderProperty(ShaderProperty(NAME("LIGHT_TYPE"), NAME("CLUSTERED")));
static const ShaderPropertyId s_propMaxClusteredShadowMaps = InternShaderProperty(ShaderProperty(NAME("MAX_CLUSTERED_SHADOW_MAPS"), int(MaxClusteredShadowMaps)));

ShaderPropertyId propTileSize = InternShaderProperty(ShaderProperty(NAME("TILE_SIZE"), int(TileSize)));
ShaderPropertyId propTileZBins = InternShaderProperty(ShaderProperty(NAME("TILE_Z_BINS"), int(TileZBins)));

static constexpr StringHash GBufferTextureNames[GTN_MAX] = {
    "GBufferAlbedoTexture"_sh,
    "GBufferNormalsTexture"_sh,
    "GBufferMaterialTexture"_sh,
    "GBufferVelocityTexture"_sh,
    "GBufferDepthTexture"_sh
};

static EngineStatTimer s_statDeferredPass("Rendering/Deferred/DeferredPass");

// Global stat counter instances
EngineStatCounter<uint32> g_statDrawCalls("Rendering/DrawCalls");
EngineStatCounter<uint32> g_statInstancedDrawCalls("Rendering/InstancedDrawCalls");
EngineStatCounter<uint32> g_statTriangles("Rendering/Triangles");
EngineStatCounter<uint32> g_statRenderGroups("Rendering/RenderGroups");
EngineStatCounter<uint32> g_statViews("Rendering/Views");
EngineStatCounter<uint32> g_statTextures("Rendering/Textures");
EngineStatCounter<uint32> g_statMaterials("Rendering/Materials");
EngineStatCounter<uint32> g_statLights("Rendering/Lights");
EngineStatCounter<uint32> g_statLightmapVolumes("Rendering/LightmapVolumes");
EngineStatCounter<uint32> g_statParticleVolumes("Rendering/ParticleVolumes");
EngineStatCounter<uint32> g_statEnvProbes("Rendering/EnvProbes");
EngineStatCounter<uint32> g_statEnvGrids("Rendering/EnvGrids");
EngineStatCounter<uint32> g_statDebugDraws("Rendering/DebugDraws");

CVar<int> cvDeferredDebugVis { "Rendering.Deferred.DebugVis", 0 };

CVar<bool> cvRayTracingEnabled { "Rendering.RayTracedEnabled", true };
CVar<bool> cvRayTracedGI { "Rendering.RayTracedGI", false };
CVar<bool> cvRayTracedReflections { "Rendering.RayTracing.RayTracedReflections", false };
CVar<bool> cvPathTracing { "Rendering.PathTracing", false };
CVar<bool> cvSSGI { "Rendering.SSGI", true };
CVar<bool> cvSSR { "Rendering.SSR", true, "Rendering.SSR.Enabled" };
CVar<bool> cvTAA { "Rendering.TAA", true };
CVar<bool> cvHBAO { "Rendering.HBAO", true, "Rendering.HBAO.Enabled" };
CVar<bool> cvEnableLightmapVolumes { "Rendering.LightmapVolumes", true };
CVar<bool> cvClusteredShading { "Rendering.ClusteredShading", true };
CVar<float> cvTonemapExposure { "Rendering.Tonemap.Exposure", 1.8f };
CVar<bool> cvBypassDrawing { "Rendering.BypassDrawing", false };

namespace DeferredRendererHelpers {

static HYP_FORCE_INLINE bool CanClusterLight(LightType lightType)
{
    return lightType == LightType::Point
        || lightType == LightType::Spot;
}

void GetDeferredShaderProperties(
    DeferredPassMode mode,
    ShaderPropertySet& outShaderProperties,
    const RenderProxyList* rpl = nullptr,
    LightType lightType = InvalidLightType,
    bool clustered = false)
{
    const EngineConfig& cfg = GetEngineConfig();

    static const IRenderConfig& s_renderConfig = g_renderInterface->GetRenderConfig();

    MergeGlobalShaderProperties(outShaderProperties);

    outShaderProperties.Add(propTileSize);
    outShaderProperties.Add(propTileZBins);

    if (cvHBAO.Get())
    {
        outShaderProperties.Add(s_propHBAOEnabled);
    }

    if (mode == DPM_INDIRECT_LIGHTING)
    {
        outShaderProperties.Set(s_propRayTracingReflections, s_renderConfig.rayTracing && cvRayTracedReflections.Get());

        if (s_renderConfig.rayTracing && cvRayTracedGI.Get())
        {
            outShaderProperties.Add(s_propRayTracingGlobalIllumination);
        }

        outShaderProperties.Set(s_propSSGIEnabled, cvSSGI.Get());
        outShaderProperties.Set(s_propSSREnabled, cvSSR.Get());
    }
    else
    {
        outShaderProperties.Add(s_propMaxClusteredShadowMaps);

        if (clustered)
        {
            outShaderProperties.Add(s_propLightTypeClustered);
        }
    }

    if (s_renderConfig.rayTracing && cvPathTracing.Get())
    {
        outShaderProperties.Add(s_propPathTracer);
    }
    else
    {
        switch (cvDeferredDebugVis.Get())
        {
        case 1: // reflections
            outShaderProperties.Add(s_propDebugReflections);
            break;
        case 2: // irradiance
            outShaderProperties.Add(s_propDebugIrradiance);
            break;
        default:
            break;
        }
    }

    if (!clustered && lightType != InvalidLightType)
    {
        outShaderProperties = outShaderProperties | s_deferredLightTypeProperties[uint32(lightType)];
    }
}

void FillShadowMapData(
    ShadowMapData& outShadowMapData,
    const ShadowMap& inShadowMap,
    View* shadowMapViewDynamic,
    View* shadowMapViewStatic)
{
    ShadowMapAtlasElement* atlasElement = inShadowMap.GetAtlasElement();
    AssertDebug(atlasElement != nullptr);

    if (!atlasElement)
        return;

    AssertDebug(shadowMapViewDynamic != nullptr && shadowMapViewDynamic->GetCamera() != nullptr);

    RenderProxyCamera* shadowCameraProxy = static_cast<RenderProxyCamera*>(GetRenderProxy(shadowMapViewDynamic->GetCamera()));
    AssertDebug(shadowCameraProxy != nullptr);

    const Mat4f& viewProjMat = shadowCameraProxy->bufferData.viewProjMat;

    BoundingBox shadowBoundsNDC;
    shadowBoundsNDC.min = Vec3f(-1.0f);
    shadowBoundsNDC.max = Vec3f(1.0f);

    BoundingBox shadowBoundsWS = viewProjMat.Inverse() * shadowBoundsNDC;
        
    outShadowMapData = {};

    outShadowMapData.layerIndex = atlasElement->layerIndex;

    outShadowMapData.viewProjMat = viewProjMat;
    outShadowMapData.invProjMat = shadowCameraProxy->bufferData.inverseProjMat;

    outShadowMapData.aabbMin.x = shadowBoundsWS.min.x;
    outShadowMapData.aabbMin.y = shadowBoundsWS.min.y;
    outShadowMapData.aabbMin.z = shadowBoundsWS.min.z;
    outShadowMapData.aabbMin.w = atlasElement->offsetUV.x;

    outShadowMapData.aabbMax.x = shadowBoundsWS.max.x;
    outShadowMapData.aabbMax.y = shadowBoundsWS.max.y;
    outShadowMapData.aabbMax.z = shadowBoundsWS.max.z;
    outShadowMapData.aabbMax.w = atlasElement->offsetUV.y;

    outShadowMapData.dimensionsScale = Vec4f(Vec2f(atlasElement->dimensions), atlasElement->scale);

    outShadowMapData.splitDistance = 0.0f; // @TODO
}

} // namespace DeferredRendererHelpers

static const TypeId s_envProbeTypeToTypeId[EPT_MAX] = {
    TypeId::ForType<SkyProbe>(),        // EPT_SKY
    TypeId::ForType<ReflectionProbe>(), // EPT_REFLECTION
    TypeId::ForType<EnvProbe>()         // EPT_AMBIENT (fixme when derived class)
};

#pragma region DeferredPass

DeferredPass::DeferredPass(DeferredPassMode mode, Vec2u extent, GBuffer* gbuffer, const FramebufferRef& framebuffer)
    : FullScreenPass(ShaderDesc(), framebuffer, TextureFormat::RGBA16F, extent, gbuffer, FSP_EXTERNAL_RENDERTARGET),
      m_mode(mode),
      m_ltcSampler(nullptr)
{
    Assert(m_framebuffer.IsValid());

    SetBlendFunction(BlendFunction(BMF_ONE, BMF_ONE, BMF_ONE, BMF_ONE));
}

DeferredPass::~DeferredPass()
{
    m_ltcSampler = nullptr;
}

void DeferredPass::Create()
{
    AssertOnThread(g_renderThread);

    FullScreenPass::Create();

    // linear transform cosines texture data
    if (m_mode == DPM_DIRECT_LIGHTING && !m_ltcSampler)
    {
        m_ltcSampler = g_renderInterface->samplerCache->GetOrCreate(SamplerDesc {
            TFM_NEAREST,
            TFM_LINEAR,
            TWM_CLAMP_TO_EDGE
        });

        ByteBuffer ltcMatrixData(sizeof(s_ltcMatrix), s_ltcMatrix);

        m_ltcMatrixTexture = MakeHandle<Texture>(
            TextureDesc {
                TextureType::Texture2D,
                TextureFormat::RGBA16F,
                Vec3u { 64, 64, 1 },
                TFM_LINEAR,
                TFM_LINEAR,
                TWM_CLAMP_TO_EDGE
            },
            ltcMatrixData.ToByteView());

        m_ltcMatrixTexture->SetName(NAME("LTC_Matrix"));
        CheckResult(m_ltcMatrixTexture->Create());

        ByteBuffer ltcBrdfData(sizeof(s_ltcBrdf), s_ltcBrdf);

        m_ltcBrdfTexture = MakeHandle<Texture>(
            TextureDesc {
                TextureType::Texture2D,
                TextureFormat::RGBA16F,
                Vec3u { 64, 64, 1 },
                TFM_LINEAR,
                TFM_LINEAR,
                TWM_CLAMP_TO_EDGE
            },
            ltcBrdfData.ToByteView());

        m_ltcBrdfTexture->SetName(NAME("LTC_BRDF"));
        CheckResult(m_ltcBrdfTexture->Create());
    }
}

void DeferredPass::Resize_Internal(Vec2u newSize)
{
    FullScreenPass::Resize_Internal(newSize);
}

void DeferredPass::RenderToFramebuffer_Internal(Frame* frame, const RenderSetup& rs, Framebuffer* framebuffer)
{
    HYP_SCOPE;

    AssertDebug(rs.world && rs.view);
    AssertDebug(rs.passData != nullptr);

    const uint32 frameIndex = frame->GetFrameIndex();

    RenderProxyList& rpl = GetConsumerProxyList(rs.view);
    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });
    
    RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(GetRenderProxy(rs.view->GetCamera()));
    Assert(cameraProxy != nullptr);

    const Vec4f& cameraPosition = cameraProxy->bufferData.cameraPosition;

    DeferredRendererPassData* dpd = ObjCast<DeferredRendererPassData>(rs.passData);
    AssertDebug(dpd != nullptr);

    Framebuffer* opaquePassFramebuffer = dpd->view.GetUnsafe()->GetOutputTarget().GetFramebuffer(RenderBucket::Opaque);

    CommandRecorder& cr = frame->cr;

    cr << SetCurrentViewport(rs.viewport);

    cr << SetInputLayout(StaticVertexInputLayout<VT_Simple>);
    cr << SetTopology(TOP_TRIANGLES);
    cr << SetFillMode(FM_FILL);
    cr << SetCurrentBlendFunction(m_blendFunction);
    cr << SetDepthWrite(false);
    cr << SetDepthTest(false);
    
    //if (m_mode == DPM_INDIRECT_LIGHTING)
    //{
    //    cr << SetStencilTest(true);
    //    cr << SetStencilFunction(StencilFunction {
    //        .passOp = SO_KEEP,
    //        .failOp = SO_KEEP,
    //        .depthFailOp = SO_KEEP,
    //        .compareOp = SCO_EQUAL });

    //    // stencil state: only render where stencil == 0 (non-lightmapped geometry)
    //    cr << SetStencilState(0, LightmapStencilMask, 0x0);
    //}

    HYP_DEFER({
        // reset states
        cr << SetCurrentBlendFunction(BlendFunction::None());
        cr << SetStencilState(0, 0xFF, 0x0);
        cr << SetDepthWrite(true);
        cr << SetDepthTest(true);
        cr << SetStencilTest(false);
    });

    uint32 numShaderUniforms = 0;

    Sampler* shadowSampler = g_renderInterface->samplerCache->GetOrCreate(SamplerDesc {
        TFM_LINEAR,
        TFM_LINEAR,
        TWM_CLAMP_TO_EDGE,
        SamplerCompareOp::LessEq
    });

    cr << SetShaderUniform(numShaderUniforms++, "SamplerLinear"_sh, g_renderInterface->placeholderData->GetSamplerLinearMipmap());
    cr << SetShaderUniform(numShaderUniforms++, "SamplerNearest"_sh, g_renderInterface->placeholderData->GetSamplerNearest());
    cr << SetShaderUniform(numShaderUniforms++, "SamplerShadow"_sh, shadowSampler);

    cr << SetShaderUniform(numShaderUniforms++, "WorldsBuffer"_sh, g_renderInterface->namedBuffers[NamedBuffer::Worlds].gpuBuffer);
    
    cr << SetShaderUniform(numShaderUniforms++, "LightsBuffer"_sh, g_renderInterface->namedBuffers[NamedBuffer::Lights].gpuBuffer);
    cr << SetShaderUniform(numShaderUniforms++, "EnvProbesBuffer"_sh, g_renderInterface->namedBuffers[NamedBuffer::EnvProbes].gpuBuffer);
    
    // use the same index for the CBuffer uniform across shaders
    const uint32 cbufferUniformIndex = numShaderUniforms++;

    cr << SetShaderUniform(numShaderUniforms++, "ShadowMapsTextureArray"_sh, g_renderInterface->shadowMapCache->GetAtlasImageView());
    cr << SetShaderUniform(numShaderUniforms++, "PointLightShadowMapsTextureArray"_sh, g_renderInterface->shadowMapCache->GetPointLightShadowMapImageView());
        
    cr << SetShaderUniform(numShaderUniforms++, "EnvProbesTexture"_sh, g_renderInterface->textureViewCache->GetOrCreate(g_renderInterface->envProbesTexture));

    for (uint32 attachmentIndex = 0; attachmentIndex < GTN_MAX; attachmentIndex++)
    {
        cr << SetShaderUniform(numShaderUniforms++, GBufferTextureNames[attachmentIndex], opaquePassFramebuffer->GetAttachment(attachmentIndex)->GetImageView());
    }

    cr << SetShaderUniform(numShaderUniforms++, "GBufferMipChain"_sh, g_renderInterface->textureViewCache->GetOrCreate(dpd->mipChain));

    if (dpd->hbao != nullptr)
        cr << SetShaderUniform(numShaderUniforms++, "SSAOResultTexture"_sh, dpd->hbao->GetFinalImageView());
    
    if (dpd->reflectionsPass != nullptr && dpd->reflectionsPass->ssrPass != nullptr)
        cr << SetShaderUniform(numShaderUniforms++, "SSRResultTexture"_sh, g_renderInterface->textureViewCache->GetOrCreate(dpd->reflectionsPass->ssrPass->GetFinalResultTexture()));

    const bool useClusteredShading = cvClusteredShading.Get();

    if (useClusteredShading || m_mode == DPM_INDIRECT_LIGHTING)
    {
        AssertDebug(dpd->gridTilesBuffer != nullptr && dpd->gridIndexBuffer != nullptr);

        // Indirect pass uses clusters for EnvProbes.
        cr << SetShaderUniform(numShaderUniforms++, "ClusterGridBuffer"_sh, dpd->gridTilesBuffer->gpuBuffer);
        cr << SetShaderUniform(numShaderUniforms++, "ClusterIndexBuffer"_sh, dpd->gridIndexBuffer->gpuBuffer);
    }

    if (m_mode == DPM_INDIRECT_LIGHTING)
    {
        if (dpd->ssgi != nullptr)
            cr << SetShaderUniform(numShaderUniforms++, "SSGIResultTexture"_sh, g_renderInterface->textureViewCache->GetOrCreate(dpd->ssgi->GetFinalResultTexture()));

        if (dpd->rayTracingReflections != nullptr)
            cr << SetShaderUniform(numShaderUniforms++, "RTRadianceResultTexture"_sh, dpd->rayTracingReflections->GetFinalImageView());

        if (dpd->ddgi)
        {
            cr << SetShaderUniform(numShaderUniforms++, "DDGIConstants"_sh, dpd->ddgi->GetConstantBuffer(frameIndex));
            cr << SetShaderUniform(numShaderUniforms++, "DDGIIrradianceTexture"_sh, dpd->ddgi->GetIrradianceImageView());
            cr << SetShaderUniform(numShaderUniforms++, "DDGIDepthTexture"_sh, dpd->ddgi->GetDepthImageView());
        }

        { // build indirect lighting constants
            GpuBuffer* cbuffer = nullptr;
            size_t cbufferOffset = 0;
            size_t cbufferSize = 0;

            // write camera data
            g_renderInterface->cbufferAllocator->Write(&cameraProxy->bufferData);

            g_renderInterface->cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

            cr << SetShaderUniform(cbufferUniformIndex, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
        }

        ShaderPropertySet shaderProperties;
        DeferredRendererHelpers::GetDeferredShaderProperties(DPM_INDIRECT_LIGHTING, shaderProperties, &rpl);

        cr << SetCurrentShader(ShaderDesc(NAME("DeferredIndirect"), shaderProperties));
        
        cr << CommitDrawState();

        cr << BindVertexBuffer(m_fullScreenQuad->GetVertexBuffer());
        cr << BindIndexBuffer(m_fullScreenQuad->GetIndexBuffer());
        cr << DrawIndexed(6);

        return;
    }

    if (useClusteredShading)
    {
        ShaderPropertySet shaderProperties;
        DeferredRendererHelpers::GetDeferredShaderProperties(DPM_DIRECT_LIGHTING, shaderProperties, &rpl, InvalidLightType, /* clustered */ true);

        cr << SetCurrentShader(ShaderDesc(NAME("DeferredDirect"), shaderProperties));

        uint32 localNumShaderUniforms = numShaderUniforms;

        // Write out MAX_SHADOW_MAPS (8?) ShadowMaps for the View, indexed by light idx (GetBinding())
        {// Build constants
            GpuBuffer* cbuffer = nullptr;
            size_t cbufferOffset = 0;
            size_t cbufferSize = 0;
            
            // write camera
            g_renderInterface->cbufferAllocator->Write(&cameraProxy->bufferData);

            uint32 maxLightBinding = 0;

            Array<Pair<Light*, uint32>, RenderTempAllocator> shadowCasterLightsInView;
            shadowCasterLightsInView.Reserve(MaxClusteredShadowMaps);

            for (Light* light : rpl.GetLights())
            {
                if (!(light->GetLightFlags() & LightFlags::ShadowCaster)
                    || !DeferredRendererHelpers::CanClusterLight(light->GetLightType()))
                {
                    continue;
                }

                if (shadowCasterLightsInView.Size() == MaxClusteredShadowMaps)
                {
                    break;
                }

                const uint32 binding = Resources::GetBinding(light);
                Assert(binding != ~0u);

                maxLightBinding = MathUtil::Max(binding + 1, maxLightBinding);

                shadowCasterLightsInView.EmplaceBack(light, binding);
            }

            Array<ShadowMapData, RenderTempAllocator> shadowMapData;
            shadowMapData.Resize(MaxClusteredShadowMaps);

            if (maxLightBinding == 0)
                maxLightBinding = 1;

            StructuredBuffer& shadowMapIndexBuffer = g_renderInterface->sbufferAllocator->AcquireBuffer(maxLightBinding, sizeof(uint32));

            uint32 shadowMapIndex = 0;

            for (const Pair<Light*, uint32>& lightAndLightBinding : shadowCasterLightsInView)
            {
                if (shadowMapIndex == MaxClusteredShadowMaps)
                {
                    break;
                }

                Light* light = lightAndLightBinding.first;

                uint32 lightBinding = lightAndLightBinding.second;
                AssertDebug(lightBinding < maxLightBinding);

                ShadowMapData& currShadowMapData = shadowMapData[shadowMapIndex];

                shadowMapIndexBuffer.Write(lightBinding * sizeof(uint32), sizeof(uint32), &shadowMapIndex);

                View* shadowMapViewDynamic;
                View* shadowMapViewStatic;

                ShadowMap* shadowMap = g_renderInterface->shadowMapCache->GetShadowMap(
                    light,
                    rs.view,
                    0,
                    shadowMapViewDynamic,
                    shadowMapViewStatic);

                if (shadowMap != nullptr)
                {
                    DeferredRendererHelpers::FillShadowMapData(
                        currShadowMapData,
                        *shadowMap,
                        shadowMapViewDynamic,
                        shadowMapViewStatic);
                }

                ++shadowMapIndex;
            }

            shadowMapIndexBuffer.Flush();
            
            cr << SetShaderUniform(localNumShaderUniforms++, "ShadowMapIndexBuffer"_sh, shadowMapIndexBuffer.gpuBuffer);

            g_renderInterface->cbufferAllocator->Write(&shadowMapData[0], shadowMapData.ByteSize(), alignof(ShadowMapData));
            g_renderInterface->cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

            cr << SetShaderUniform(cbufferUniformIndex, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
        }

        cr << CommitDrawState();

        cr << BindVertexBuffer(m_fullScreenQuad->GetVertexBuffer());
        cr << BindIndexBuffer(m_fullScreenQuad->GetIndexBuffer());
        cr << DrawIndexed(6);
    }

    // last LightType we rendered
    LightType prevLightType = InvalidLightType;

    // render with each light
    for (uint32 lightTypeIndex = 0; lightTypeIndex < NumLightTypes; lightTypeIndex++)
    {
        const LightType lightType = LightType(lightTypeIndex);

        if (useClusteredShading)
        {
            if (DeferredRendererHelpers::CanClusterLight(lightType))
            {
                continue; // skip; would've been rendered with clustering.
            }
        }

        for (Light* light : rpl.GetLights())
        {
            if (light->GetLightType() != lightType)
            {
                continue;
            }

            RenderProxyLight* lightProxy = static_cast<RenderProxyLight*>(GetRenderProxy(light));
            AssertDebug(lightProxy != nullptr);

            if (lightType != prevLightType)
            {
                ShaderPropertySet shaderProperties;
                DeferredRendererHelpers::GetDeferredShaderProperties(DPM_DIRECT_LIGHTING, shaderProperties, &rpl, lightType);

                cr << SetCurrentShader(ShaderDesc(NAME("DeferredDirect"), shaderProperties));
            }
                
            uint32 localNumShaderUniforms = numShaderUniforms;

            { // Build constants
                GpuBuffer* cbuffer = nullptr;
                size_t cbufferOffset = 0;
                size_t cbufferSize = 0;

                // write camera
                g_renderInterface->cbufferAllocator->Write(&cameraProxy->bufferData);

                // write current light
                g_renderInterface->cbufferAllocator->Write(&lightProxy->bufferData);

                ShadowMapData shadowMapData[MaxShadowMapCascades];

                const uint32 numCascadesToWrite = (lightType == LightType::Directional) ? MaxShadowMapCascades : 1;

                for (uint32 cascadeIndex = 0; cascadeIndex < numCascadesToWrite; cascadeIndex++)
                {
                    ShadowMapData& currShadowMapData = shadowMapData[cascadeIndex];
                        
                    View* shadowMapViewDynamic;
                    View* shadowMapViewStatic;

                    ShadowMap* shadowMap = g_renderInterface->shadowMapCache->GetShadowMap(
                        light,
                        rs.view,
                        cascadeIndex,
                        shadowMapViewDynamic,
                        shadowMapViewStatic);

                    if (shadowMap != nullptr)
                    {
                        DeferredRendererHelpers::FillShadowMapData(
                            shadowMapData[cascadeIndex],
                            *shadowMap,
                            shadowMapViewDynamic,
                            shadowMapViewStatic);
                    }

                    g_renderInterface->cbufferAllocator->Write(&shadowMapData[cascadeIndex]);
                }
                    
                g_renderInterface->cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

                cr << SetShaderUniform(cbufferUniformIndex, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
            }

            cr << SetShaderUniform(localNumShaderUniforms++, "CurrentLight"_sh, g_renderInterface->namedBuffers[NamedBuffer::Lights].gpuBuffer, TShaderDataOffset<LightShaderData>(light));

            if (lightType == LightType::AreaRect)
            {
                if (lightProxy != nullptr && lightProxy->lightMaterial != nullptr)
                {
                    RenderProxyMaterial* materialProxy = static_cast<RenderProxyMaterial*>(GetRenderProxy(lightProxy->lightMaterial));
                    AssertDebug(materialProxy != nullptr);

                    if (materialProxy->bufferData.textureUsage & uint32(MaterialTextureKey::Diffuse))
                    {
                        const uint32 materialBoundIndex = Resources::GetBinding(lightProxy->lightMaterial);
                        AssertDebug(materialBoundIndex != ~0u);

                        Span<const GpuImageViewRef> imageViews = g_renderInterface->materialTextureCache->imageViews.Get(materialBoundIndex);
                        AssertDebug(imageViews.Size() >= materialProxy->boundTextures.Size());

                        cr << SetShaderUniform(localNumShaderUniforms++, "DiffuseMap"_sh, imageViews[materialProxy->boundTextureIndices[0]]);
                    }

                    cr << SetShaderUniform(localNumShaderUniforms++, "CurrentMaterial"_sh, g_renderInterface->namedBuffers[NamedBuffer::Materials].gpuBuffer, TShaderDataOffset<MaterialShaderData>(lightProxy->lightMaterial));
                }
                else
                {
                    cr << SetShaderUniform(localNumShaderUniforms++, "CurrentMaterial"_sh, g_renderInterface->namedBuffers[NamedBuffer::Materials].gpuBuffer, TShaderDataOffset<MaterialShaderData>(0));
                }

                cr << SetShaderUniform(localNumShaderUniforms++, "LTCSampler"_sh, m_ltcSampler);

                if (m_ltcMatrixTexture != nullptr)
                    cr << SetShaderUniform(localNumShaderUniforms++, "LTCMatrixTexture"_sh, g_renderInterface->textureViewCache->GetOrCreate(m_ltcMatrixTexture));

                if (m_ltcBrdfTexture != nullptr)
                    cr << SetShaderUniform(localNumShaderUniforms++, "LTCBRDFTexture"_sh, g_renderInterface->textureViewCache->GetOrCreate(m_ltcBrdfTexture));
            }
                
            cr << CommitDrawState();

            cr << BindVertexBuffer(m_fullScreenQuad->GetVertexBuffer());
            cr << BindIndexBuffer(m_fullScreenQuad->GetIndexBuffer());
            cr << DrawIndexed(6);

            prevLightType = lightType;
        }
    }
}

#pragma endregion DeferredPass

#pragma region TonemapPass

TonemapPass::TonemapPass(Vec2u extent, GBuffer* gbuffer)
    : FullScreenPass(TextureFormat::R11G11B10F, extent, gbuffer)
{
}

TonemapPass::~TonemapPass()
{
}

void TonemapPass::Resize_Internal(Vec2u newSize)
{
    FullScreenPass::Resize_Internal(newSize);
}

void TonemapPass::Render(Frame* frame, const RenderSetup& rs)
{
    ShaderPropertySet shaderProperties;
    shaderProperties.Add(s_propOutputSDR);
    shaderProperties.Add(InternShaderProperty(ShaderProperty(NAME("EXPOSURE"), cvTonemapExposure.Get())));
    m_shaderDesc = ShaderDesc(NAME("Tonemap"), shaderProperties);

    Begin(frame, rs);

    CommandRecorder& cr = frame->cr;

    DeferredRendererPassData* dpd = ObjCast<DeferredRendererPassData>(rs.passData);
    AssertDebug(dpd != nullptr);

    const uint32 frameIndex = frame->GetFrameIndex();
    const FramebufferRef& inputsFramebuffer = dpd->view.GetUnsafe()->GetOutputTarget().GetFramebuffer(RenderBucket::Opaque);

    uint32 numShaderUniforms = 0;

    cr << SetShaderUniform(numShaderUniforms++, "GBufferAlbedoTexture"_sh, inputsFramebuffer->GetAttachment(GTN_ALBEDO)->GetImageView());
    cr << SetShaderUniform(numShaderUniforms++, "GBufferNormalsTexture"_sh, inputsFramebuffer->GetAttachment(GTN_NORMALS)->GetImageView());
    cr << SetShaderUniform(numShaderUniforms++, "GBufferMaterialTexture"_sh, inputsFramebuffer->GetAttachment(GTN_MATERIAL)->GetImageView());
    cr << SetShaderUniform(numShaderUniforms++, "GBufferVelocityTexture"_sh, inputsFramebuffer->GetAttachment(GTN_VELOCITY)->GetImageView());
    cr << SetShaderUniform(numShaderUniforms++, "GBufferDepthTexture"_sh, inputsFramebuffer->GetAttachment(GTN_DEPTH)->GetImageView());

    Framebuffer* translucentPassFramebuffer = dpd->view.GetUnsafe()->GetOutputTarget().GetFramebuffer(RenderBucket::Translucent);
    AssertDebug(translucentPassFramebuffer != nullptr);

    cr << SetShaderUniform(numShaderUniforms++, "DeferredResult"_sh, translucentPassFramebuffer->GetAttachment(GTN_ALBEDO)->GetImageView());

    cr << SetShaderUniform(numShaderUniforms++, "ShadowMapsTextureArray"_sh, g_renderInterface->shadowMapCache->GetAtlasImageView());

    cr << SetShaderUniform(numShaderUniforms++, "GBufferMipChain"_sh, g_renderInterface->textureViewCache->GetOrCreate(dpd->mipChain));

    cr << SetShaderUniform(numShaderUniforms++, "SamplerNearest"_sh, g_renderInterface->placeholderData->GetSamplerNearest());
    cr << SetShaderUniform(numShaderUniforms++, "SamplerLinear"_sh, g_renderInterface->placeholderData->GetSamplerLinear());

    if (dpd->rayTracingReflections)
    {
        cr << SetShaderUniform(numShaderUniforms++, "RTRadianceResultTexture"_sh, dpd->rayTracingReflections->GetFinalImageView());
    }
    else
    {
        cr << SetShaderUniform(numShaderUniforms++, "RTRadianceResultTexture"_sh, g_renderInterface->placeholderData->GetImageView2D1x1R8());
    }

    if (dpd->ssgi)
    {
        cr << SetShaderUniform(numShaderUniforms++, "SSGIResultTexture"_sh, g_renderInterface->textureViewCache->GetOrCreate(dpd->ssgi->GetFinalResultTexture()));
    }
    else
    {
        cr << SetShaderUniform(numShaderUniforms++, "SSGIResultTexture"_sh, g_renderInterface->placeholderData->GetImageView2D1x1R8());
    }

    if (dpd->taaPass)
    {
        cr << SetShaderUniform(numShaderUniforms++, "TAAResultTexture"_sh, g_renderInterface->textureViewCache->GetOrCreate(dpd->taaPass->GetResultTexture()));
    }
    else
    {
        cr << SetShaderUniform(numShaderUniforms++, "TAAResultTexture"_sh, g_renderInterface->placeholderData->GetImageView2D1x1R8());
    }

    cr << SetShaderUniform(numShaderUniforms++, "DeferredIndirectResultTexture"_sh, dpd->deferredShadingFramebuffer->GetAttachment(0)->GetImageView());

    cr << SetShaderUniform(numShaderUniforms++, "PostProcessingUniforms"_sh, dpd->postProcessing->GetUniformBuffer());

    cr << SetShaderUniform(numShaderUniforms++, "CamerasBuffer"_sh, g_renderInterface->namedBuffers[NamedBuffer::Cameras].gpuBuffer, TShaderDataOffset<CameraShaderData>(rs.view->GetCamera()));
    cr << SetShaderUniform(numShaderUniforms++, "WorldsBuffer"_sh, g_renderInterface->namedBuffers[NamedBuffer::Worlds].gpuBuffer);

    RenderFullScreenQuad(frame, rs);

    End(frame, rs);
}

#pragma endregion TonemapPass

#pragma region LightmapPass

struct LightmapVolumeUniforms
{
    float irradianceWeight;
    float radianceWeight;

    uint32 numAtlases;
};

LightmapPass::LightmapPass()
    : FullScreenPass(TextureFormat::RGBA16F, nullptr, FSP_EXTERNAL_RENDERTARGET)
{
    m_shaderDesc = ShaderDesc(NAME("ApplyLightmap"));
}

LightmapPass::~LightmapPass()
{
    for (auto& data : m_lightmapVolumePassData)
    {
        EnqueueDeletion(std::move(data.uniformBuffers));
    }
}

void LightmapPass::Create()
{
    AssertOnThread(g_renderThread);

    FullScreenPass::Create();
}

void LightmapPass::Resize_Internal(Vec2u newSize)
{
    FullScreenPass::Resize_Internal(newSize);
}

void LightmapPass::RenderToFramebuffer_Internal(Frame* frame, const RenderSetup& renderSetup, Framebuffer* framebuffer)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.world && renderSetup.volume && renderSetup.view);

    const uint32 frameIndex = frame->GetFrameIndex();

    LightmapVolume* volume = ObjCast<LightmapVolume>(renderSetup.volume);
    AssertDebug(volume != nullptr);

    RenderProxyLightmapVolume* proxy = static_cast<RenderProxyLightmapVolume*>(GetRenderProxy(volume));
    Assert(proxy != nullptr);

    if (proxy->numAtlases == 0)
    {
        return; // nothing to do
    }

    DeferredRendererPassData* dpd = ObjCast<DeferredRendererPassData>(renderSetup.passData);
    AssertDebug(dpd != nullptr);

    Framebuffer* viewFramebuffer = dpd->view.GetUnsafe()->GetOutputTarget().GetFramebuffer(RenderBucket::Opaque);
    AssertDebug(viewFramebuffer != nullptr);

    CommandRecorder& cr = frame->cr;

    cr << SetCurrentShader(m_shaderDesc);

    cr << SetCurrentViewport(renderSetup.viewport);

    cr << SetInputLayout(StaticVertexInputLayout<VT_Simple>);

    cr << SetFaceCullMode(FCM_BACK);
    cr << SetFillMode(FM_FILL);
    cr << SetTopology(TOP_TRIANGLES);

    cr << SetDepthTest(false);
    cr << SetDepthWrite(false);
    
    cr << SetCurrentBlendFunction(BlendFunction::Additive());

    cr << SetStencilTest(true);
    cr << SetStencilFunction(StencilFunction {
        .passOp = SO_KEEP,
        .failOp = SO_KEEP,
        .depthFailOp = SO_KEEP,
        .compareOp = SCO_EQUAL // match values with equal atlas index when we render
    });

    HYP_DEFER({
        // reset states
        cr << SetCurrentBlendFunction(BlendFunction::None());
        cr << SetStencilState(0, 0xFF, 0x0);
        cr << SetDepthWrite(true);
        cr << SetDepthTest(true);
        cr << SetStencilTest(false);
    });

    LightmapVolumePassData& data = GetLightmapVolumePassData(volume);

    uint32 numShaderUniforms = 0;

    // GBuffer textures
    cr << SetShaderUniform(numShaderUniforms++, "GBufferAlbedoTexture"_sh, viewFramebuffer->GetAttachment(GTN_ALBEDO)->GetImageView());
    cr << SetShaderUniform(numShaderUniforms++, "GBufferNormalsTexture"_sh, viewFramebuffer->GetAttachment(GTN_NORMALS)->GetImageView());
    cr << SetShaderUniform(numShaderUniforms++, "GBufferMaterialTexture"_sh, viewFramebuffer->GetAttachment(GTN_MATERIAL)->GetImageView());
    cr << SetShaderUniform(numShaderUniforms++, "GBufferDepthTexture"_sh, viewFramebuffer->GetAttachment(GTN_DEPTH)->GetImageView());
    cr << SetShaderUniform(numShaderUniforms++, "GBufferVelocityTexture"_sh, viewFramebuffer->GetAttachment(GTN_VELOCITY)->GetImageView());
    cr << SetShaderUniform(numShaderUniforms++, "GBufferMipChain"_sh, g_renderInterface->textureViewCache->GetOrCreate(dpd->mipChain));

    // Samplers
    cr << SetShaderUniform(numShaderUniforms++, "SamplerNearest"_sh, g_renderInterface->placeholderData->GetSamplerNearest());
    cr << SetShaderUniform(numShaderUniforms++, "SamplerLinear"_sh, g_renderInterface->placeholderData->GetSamplerLinear());

    // Shadows
    cr << SetShaderUniform(numShaderUniforms++, "ShadowMapsTextureArray"_sh, g_renderInterface->shadowMapCache->GetAtlasImageView());
    cr << SetShaderUniform(numShaderUniforms++, "PointLightShadowMapsTextureArray"_sh, g_renderInterface->shadowMapCache->GetPointLightShadowMapImageView());

    // Cameras and Worlds buffers
    cr << SetShaderUniform(numShaderUniforms++, "CamerasBuffer"_sh, g_renderInterface->namedBuffers[NamedBuffer::Cameras].gpuBuffer, TShaderDataOffset<CameraShaderData>(renderSetup.view->GetCamera()));
    cr << SetShaderUniform(numShaderUniforms++, "WorldsBuffer"_sh, g_renderInterface->namedBuffers[NamedBuffer::Worlds].gpuBuffer);

    // Env probes
    cr << SetShaderUniform(numShaderUniforms++, "EnvProbesTexture"_sh, g_renderInterface->textureViewCache->GetOrCreate(g_renderInterface->envProbesTexture));
    cr << SetShaderUniform(numShaderUniforms++, "EnvProbesBuffer"_sh, g_renderInterface->namedBuffers[NamedBuffer::EnvProbes].gpuBuffer);

    if (renderSetup.envProbe != nullptr)
        cr << SetShaderUniform(numShaderUniforms++, "CurrentEnvProbe"_sh, g_renderInterface->namedBuffers[NamedBuffer::EnvProbes].gpuBuffer, TShaderDataOffset<EnvProbeShaderData>(renderSetup.envProbe));
    else
        cr << SetShaderUniform(numShaderUniforms++, "CurrentEnvProbe"_sh, g_renderInterface->namedBuffers[NamedBuffer::EnvProbes].gpuBuffer, TShaderDataOffset<EnvProbeShaderData>(0));

    if (dpd->hbao != nullptr)
        cr << SetShaderUniform(numShaderUniforms++, "SSAOResultTexture"_sh, dpd->hbao->GetFinalImageView());

    if (data.uniformBuffers.Size() < proxy->numAtlases)
    {
        data.uniformBuffers.Resize(proxy->numAtlases);
    }

    for (uint32 atlasIndex = 0; atlasIndex < proxy->numAtlases; atlasIndex++)
    {
        Texture* irradianceTexture = proxy->atlasIrradianceTextures[atlasIndex];
        Texture* radianceTexture = proxy->atlasRadianceTextures[atlasIndex];

        LightmapVolumeUniforms uniforms {};
        uniforms.numAtlases = proxy->numAtlases;
        uniforms.irradianceWeight = irradianceTexture ? 1.0f : 0.0f;
        uniforms.radianceWeight = radianceTexture ? 1.0f : 0.0f;

        GpuBufferRef& uniformBuffer = data.uniformBuffers[atlasIndex];

        if (!uniformBuffer)
        {
            uniformBuffer = g_renderInterface->MakeGpuBuffer(GpuBufferType::CONSTANT_BUFFER, sizeof(LightmapVolumeUniforms));
            CheckResult(uniformBuffer->Create());
        }

        uniformBuffer->Copy(sizeof(uniforms), &uniforms);

        // only draw elems in the volume with a stencil reference of the atlas index (+1)
        cr << SetStencilState(atlasIndex + 1, LightmapStencilMask, 0x0);

        uint32 localNumShaderUniforms = numShaderUniforms;

        cr << SetShaderUniform(localNumShaderUniforms++, "IrradianceTexture"_sh, g_renderInterface->textureViewCache->GetOrCreate(irradianceTexture != nullptr ? irradianceTexture : g_renderInterface->placeholderData->defaultTexture2d));
        cr << SetShaderUniform(localNumShaderUniforms++, "RadianceTexture"_sh, g_renderInterface->textureViewCache->GetOrCreate(radianceTexture != nullptr ? radianceTexture : g_renderInterface->placeholderData->defaultTexture2d));
        cr << SetShaderUniform(localNumShaderUniforms++, "LightmapSampler"_sh, g_renderInterface->placeholderData->GetSamplerLinear());
        cr << SetShaderUniform(localNumShaderUniforms++, "LightmapVolumeUniforms"_sh, uniformBuffer);

        RenderFullScreenQuad(frame, renderSetup);
    }

    // reset stencil state back to default
    cr << SetStencilState(0, 0xFF, 0x0);

    m_isFirstFrame = false;
}

#pragma endregion LightmapPass

#pragma region FogVolumePass

static constexpr uint32 MaxBoundLightsPerFogVolume = 4;

FogVolumePass::FogVolumePass()
    : FullScreenPass(TextureFormat::RGBA16F, nullptr, FSP_EXTERNAL_RENDERTARGET)
{
}

FogVolumePass::~FogVolumePass()
{
}

void FogVolumePass::Create()
{
    AssertOnThread(g_renderThread);

    m_volumeMesh = MeshBuilder::Cube(true);
    m_volumeMesh->SetFlags(MeshFlags::ViewIndependent);
    m_volumeMesh->SetName(NAME("FogVolumeMesh"));
    InitObject(m_volumeMesh);

    ShaderPropertySet shaderProperties;
    shaderProperties.Add(InternShaderProperty(ShaderProperty(NAME("MAX_LIGHTS"), int(MaxBoundLightsPerFogVolume))));

    m_shaderDesc = ShaderDesc(NAME("ApplyFogVolume"), shaderProperties);

    FullScreenPass::Create();
}

void FogVolumePass::Resize_Internal(Vec2u newSize)
{
    FullScreenPass::Resize_Internal(newSize);
}

void FogVolumePass::RenderToFramebuffer_Internal(Frame* frame, const RenderSetup& renderSetup, Framebuffer* framebuffer)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.world && renderSetup.volume && renderSetup.view);

    DeferredRendererPassData* dpd = ObjCast<DeferredRendererPassData>(renderSetup.passData);
    AssertDebug(dpd != nullptr);

    FogVolume* volume = ObjCast<FogVolume>(renderSetup.volume);
    AssertDebug(volume != nullptr);

    RenderProxyFogVolume* proxy = static_cast<RenderProxyFogVolume*>(GetRenderProxy(volume));
    Assert(proxy != nullptr);

    FogVolumePassData& data = GetFogVolumePassData(volume);
    data.noiseTexture = proxy->noiseTexture;
    data.volumeTexture = proxy->volumeTexture;

    CommandRecorder& cr = frame->cr;

    cr << SetCurrentViewport(renderSetup.viewport);

    cr << SetTopology(m_volumeMesh->GetMeshAttributes().topology);
    cr << SetInputLayout(m_volumeMesh->GetMeshAttributes().inputLayout);

    cr << SetFillMode(FM_FILL);
    cr << SetDepthWrite(false);
    cr << SetDepthTest(false);
    cr << SetStencilTest(false);
    cr << SetFaceCullMode(FCM_FRONT); // cull front faces to render inside of the volume
    cr << SetCurrentBlendFunction(BlendFunction::Additive());

    cr << SetCurrentShader(m_shaderDesc);

    cr << SetShaderUniform(0, "SamplerLinear"_sh, g_renderInterface->placeholderData->GetSamplerLinearMipmap());
    cr << SetShaderUniform(1, "SamplerNearest"_sh, g_renderInterface->placeholderData->GetSamplerNearest());

    cr << SetShaderUniform(2, "CamerasBuffer"_sh, g_renderInterface->namedBuffers[NamedBuffer::Cameras].gpuBuffer, TShaderDataOffset<CameraShaderData>(renderSetup.view->GetCamera()));

    cr << SetShaderUniform(3, "ShadowMapsTextureArray"_sh, g_renderInterface->shadowMapCache->GetAtlasImageView());
    cr << SetShaderUniform(4, "PointLightShadowMapsTextureArray"_sh, g_renderInterface->shadowMapCache->GetPointLightShadowMapImageView());

    if (data.volumeTexture)
        cr << SetShaderUniform(5, "DataMap"_sh, g_renderInterface->textureViewCache->GetOrCreate(data.volumeTexture));

    if (data.noiseTexture)
        cr << SetShaderUniform(6, "NoiseMap"_sh, g_renderInterface->textureViewCache->GetOrCreate(data.noiseTexture));

    cr << SetShaderUniform(7, "DepthPyramidTexture"_sh, dpd->depthPyramidRenderer->GetResultImageView());

    // Set constants
    FogVolumeShaderData shaderData = proxy->bufferData;

    uint32& numBoundLights = shaderData.numBoundLights;
    numBoundLights = 0;

    uint32* lightIndicesU32 = reinterpret_cast<uint32*>(shaderData.lightIndices);

    RenderProxyList& rpl = GetConsumerProxyList(renderSetup.view);

    Array<Pair<Light*, LightShaderData*>, RenderAllocator> tempLightsArray;

    for (Light* light : rpl.GetLights())
    {
        const LightType lightType = light->GetLightType();

        if (lightType != LightType::Directional && lightType != LightType::Point)
        {
            continue;
        }

        if (numBoundLights >= MaxBoundLightsPerFogVolume)
        {
            break;
        }

        RenderProxyLight* lightProxy = static_cast<RenderProxyLight*>(GetRenderProxy(light));
        Assert(lightProxy != nullptr);

        tempLightsArray.EmplaceBack(light, &lightProxy->bufferData);

        lightIndicesU32[numBoundLights++] = Resources::GetBinding(light);
    }

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;
    
    g_renderInterface->cbufferAllocator->Write(&shaderData);

    for (uint32 i = 0; i < MaxBoundLightsPerFogVolume; i++)
    {
        if (i < uint32(tempLightsArray.Size()))
        {
            g_renderInterface->cbufferAllocator->Write(tempLightsArray[i].second);
            continue;
        }
        
        LightShaderData dummy {};
        g_renderInterface->cbufferAllocator->Write(&dummy);
    }

    for (uint32 i = 0; i < MaxBoundLightsPerFogVolume; i++)
    {
        ShadowMapData shadowMapData {};

        if (i < uint32(tempLightsArray.Size()))
        {
            View* shadowMapViewDynamic;
            View* shadowMapViewStatic;

            Light* light = tempLightsArray[i].first;

            ShadowMap* shadowMap = g_renderInterface->shadowMapCache->GetShadowMap(
                light,
                renderSetup.view,
                /* cascadeIndex */ 0,
                shadowMapViewDynamic,
                shadowMapViewStatic);

            if (shadowMap != nullptr)
            {
                DeferredRendererHelpers::FillShadowMapData(
                    shadowMapData,
                    *shadowMap,
                    shadowMapViewDynamic,
                    shadowMapViewStatic);
            }
        }

        g_renderInterface->cbufferAllocator->Write(&shadowMapData);
    }
            
    g_renderInterface->cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    cr << SetShaderUniform(8, "FogVolumeConstants"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

    cr << CommitDrawState();

    cr << BindVertexBuffer(m_volumeMesh->GetVertexBuffer());
    cr << BindIndexBuffer(m_volumeMesh->GetIndexBuffer());
    cr << DrawIndexed(36); // draw cube

    // reset states
    cr << SetCurrentBlendFunction(BlendFunction::None());
    cr << SetDepthTest(true);
    cr << SetDepthWrite(true);

    m_isFirstFrame = false;
}

#pragma endregion FogVolumePass

#pragma region ReflectionsPass

// Sky renders first
constexpr FixedArray<EnvProbeType, CMT_MAX> EnvProbeTypes {
    EPT_SKY,
    EPT_REFLECTION
};

constexpr FixedArray<CubemapType, CMT_MAX> CubemapTypes {
    CMT_DEFAULT,           // EPT_SKY
    CMT_PARALLAX_CORRECTED // EPT_REFLECTION
};

static const FixedArray<Pair<CubemapType, ShaderPropertySet>, CMT_MAX> s_cubemapPasses = {
    Pair<CubemapType, ShaderPropertySet> { CMT_DEFAULT, ShaderPropertySet {} },
    Pair<CubemapType, ShaderPropertySet> { CMT_PARALLAX_CORRECTED, ShaderPropertySet { { InternShaderProperty(ShaderProperty(NAME("ENV_PROBE_PARALLAX_CORRECTED"))) } } }
};

ReflectionsPass::ReflectionsPass(Vec2u extent, GBuffer* gbuffer, const GpuImageViewRef& mipChainImageView)
    : FullScreenPass(TextureFormat::RGBA16F, extent, gbuffer),
      m_mipChainImageView(mipChainImageView),
      m_isFirstFrame(true)
{
    m_shaderDesc = ShaderDesc(NAME("ApplyReflectionProbe"));

    SetBlendFunction(BlendFunction(
        BMF_SRC_ALPHA, BMF_ONE_MINUS_SRC_ALPHA,
        BMF_ONE, BMF_ONE_MINUS_SRC_ALPHA));
}

ReflectionsPass::~ReflectionsPass()
{
    EnqueueDeletion(std::move(m_mipChainImageView));

    ssrPass.Reset();
}

void ReflectionsPass::Create()
{
    HYP_SCOPE;

    FullScreenPass::Create();

    CreateSSRPass();
}

bool ReflectionsPass::ShouldRenderSSR() const
{
    return cvSSR.Get() && !cvRayTracedReflections.Get();
}

void ReflectionsPass::CreateSSRPass()
{
    ssrPass = MakeUnique<SSRPass>(m_gbuffer, m_mipChainImageView);
    ssrPass->Create();
}

void ReflectionsPass::Resize_Internal(Vec2u newSize)
{
    HYP_SCOPE;

    FullScreenPass::Resize_Internal(newSize);
}

void ReflectionsPass::Render(Frame* frame, const RenderSetup& rs)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(rs.world && rs.view);
    AssertDebug(rs.passData != nullptr);

    const uint32 frameIndex = frame->GetFrameIndex();

    RenderProxyList& rpl = GetConsumerProxyList(rs.view);
    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    Viewport viewport = rs.viewport;

    if (ShouldRenderCheckerboarded())
    {
        const Vec2i viewportOffset = (Vec2i(m_framebuffer->GetExtent().x, 0) / 2) * (GetWorldBufferData()->frameCounter & 1);
        const Vec2u viewportExtent = Vec2u(m_framebuffer->GetExtent().x / 2, m_framebuffer->GetExtent().y);

        viewport = Viewport { viewportExtent, viewportOffset };
    }

    CommandRecorder& cr = frame->cr;

    if (ShouldRenderSSR())
    {
        ssrPass->Render(frame, rs);
    }

    cr << SetTopology(TOP_TRIANGLES);
    cr << SetInputLayout(StaticVertexInputLayout<VT_Simple>);

    cr << SetCurrentViewport(viewport);

    cr << SetCurrentShader(m_shaderDesc);

    cr << SetDepthTest(false);
    cr << SetDepthWrite(false);
    cr << SetStencilTest(false);
    cr << SetCurrentBlendFunction(BlendFunction(
        BMF_SRC_ALPHA, BMF_ONE_MINUS_SRC_ALPHA,
        BMF_ONE, BMF_ONE_MINUS_SRC_ALPHA));
    cr << SetFillMode(FM_FILL);
    cr << SetFaceCullMode(FCM_BACK);

    HYP_DEFER({
        cr << SetCurrentBlendFunction(BlendFunction::None());
        cr << SetDepthTest(true);
        cr << SetDepthWrite(true);
    });

    FixedArray<Array<EnvProbe*, RenderTempAllocator>, CMT_MAX> probesPerCubemapType;

    for (uint32 cubemapType = 0; cubemapType < CMT_MAX; cubemapType++)
    {
        const EnvProbeType envProbeType = EnvProbeTypes[cubemapType];

        for (EnvProbe* envProbe : rpl.GetEnvProbes().GetElements(s_envProbeTypeToTypeId[envProbeType]))
        {
            probesPerCubemapType[cubemapType].PushBack(envProbe);
        }
    }

    cr << SetCurrentFramebuffer(GetFramebuffer());

    // render previous frame's result to screen if doing temporal blending (and not checkerboarded)
    if (!m_isFirstFrame && UsesTemporalBlending() && !ShouldRenderCheckerboarded())
    {
        DrawHistoryTexture(frame, rs);
    }

    cr << SetShaderUniform(0, "SamplerLinear"_sh, g_renderInterface->placeholderData->GetSamplerLinearMipmap());
    cr << SetShaderUniform(1, "SamplerNearest"_sh, g_renderInterface->placeholderData->GetSamplerNearest());

    DeferredRendererPassData* dpd = ObjCast<DeferredRendererPassData>(rs.passData);
    AssertDebug(dpd != nullptr);

    const FramebufferRef& opaquePassFramebuffer = dpd->view.GetUnsafe()->GetOutputTarget().GetFramebuffer(RenderBucket::Opaque);

    for (uint32 attachmentIndex = 0; attachmentIndex < GTN_MAX; attachmentIndex++)
    {
        cr << SetShaderUniform(2 + attachmentIndex, GBufferTextureNames[attachmentIndex], opaquePassFramebuffer->GetAttachment(attachmentIndex)->GetImageView());
    }

    cr << SetShaderUniform(2 + GTN_MAX, "CamerasBuffer"_sh, g_renderInterface->namedBuffers[NamedBuffer::Cameras].gpuBuffer, TShaderDataOffset<CameraShaderData>(rs.view->GetCamera()));
    cr << SetShaderUniform(3 + GTN_MAX, "WorldsBuffer"_sh, g_renderInterface->namedBuffers[NamedBuffer::Worlds].gpuBuffer);
    cr << SetShaderUniform(4 + GTN_MAX, "EnvProbesBuffer"_sh, g_renderInterface->namedBuffers[NamedBuffer::EnvProbes].gpuBuffer);

    cr << SetShaderUniform(10 + GTN_MAX, "BlueNoiseBuffer"_sh, g_renderInterface->blueNoiseBuffer);
    cr << SetShaderUniform(11 + GTN_MAX, "SphereSamplesBuffer"_sh, g_renderInterface->sphereSamplesBuffer);

    cr << SetShaderUniform(12 + GTN_MAX, "GBufferMipChain"_sh, g_renderInterface->textureViewCache->GetOrCreate(dpd->mipChain));

    cr << SetShaderUniform(13 + GTN_MAX, "EnvProbesTexture"_sh, g_renderInterface->textureViewCache->GetOrCreate(g_renderInterface->envProbesTexture));

    uint32 numRenderedEnvProbes = 0;

    for (uint32 envProbeTypeIndex = 0; envProbeTypeIndex < ArraySize(EnvProbeTypes); envProbeTypeIndex++)
    {
        const EnvProbeType envProbeType = EnvProbeTypes[envProbeTypeIndex];
        const CubemapType cubemapType = CubemapTypes[envProbeTypeIndex];

        const Array<EnvProbe*, RenderTempAllocator>& probes = probesPerCubemapType[cubemapType];

        if (probes.Empty())
        {
            continue;
        }

        for (EnvProbe* envProbe : probes)
        {
            if (numRenderedEnvProbes >= MaxBoundReflectionProbes)
            {
                HYP_LOG(Rendering, Warning, "Attempting to render too many reflection probes.");

                break;
            }

            cr << SetShaderUniform(5 + GTN_MAX, "CurrentEnvProbe"_sh, g_renderInterface->namedBuffers[NamedBuffer::EnvProbes].gpuBuffer, TShaderDataOffset<EnvProbeShaderData>(envProbe));

            RenderFullScreenQuad(frame, rs);

            ++numRenderedEnvProbes;
        }
    }

    if (ShouldRenderSSR())
    {
        const Handle<Texture>& ssrTexture = ssrPass->GetFinalResultTexture();

        // render SSR to screen
        FramebufferDesc framebufferDesc = rs.view->GetOutputTarget().GetFramebuffer()->GetFramebufferDesc();
        framebufferDesc.attachments[0].loadOp = LoadOperation::LOAD;
        framebufferDesc.attachments[0].blendFunction = BlendFunction(BMF_SRC_ALPHA, BMF_ONE_MINUS_SRC_ALPHA, BMF_ONE, BMF_ONE_MINUS_SRC_ALPHA);

        cr << SetCurrentViewport(rs.viewport);

        cr << SetCurrentShader(ShaderDesc(NAME("BlitTexture")));

        // reset
        cr << SetDepthTest(false);
        cr << SetDepthWrite(false);
        cr << SetCurrentBlendFunction(BlendFunction(BMF_SRC_ALPHA, BMF_ONE_MINUS_SRC_ALPHA, BMF_ONE, BMF_ONE_MINUS_SRC_ALPHA));

        cr << SetShaderUniform(0, "SamplerLinear"_sh, g_renderInterface->placeholderData->GetSamplerLinear());
        cr << SetShaderUniform(1, "WorldsBuffer"_sh, g_renderInterface->namedBuffers[NamedBuffer::Worlds].gpuBuffer);
        cr << SetShaderUniform(2, "InTexture"_sh, g_renderInterface->textureViewCache->GetOrCreate(ssrTexture));

        RenderFullScreenQuad(frame, rs);

        cr << SetDepthTest(true);
        cr << SetDepthWrite(true);
        cr << SetCurrentBlendFunction(BlendFunction::None());
    }

    cr << SetCurrentFramebuffer(nullptr);

    if (ShouldRenderCheckerboarded())
    {
        MergeCheckerboard(frame, rs);
    }

    if (UsesTemporalBlending())
    {
        if (!ShouldRenderCheckerboarded())
        {
            CopyResultToPreviousTexture(frame, rs);
        }

        m_temporalBlending->Render(frame, rs);
    }

    m_isFirstFrame = false;
}

#pragma endregion ReflectionsPass

#pragma region DeferredRendererPassData

DeferredRendererPassData::~DeferredRendererPassData()
{
    depthPyramidRenderer.Reset();

    hbao.Reset();

    taaPass.Reset();

    // m_dofBlur->Destroy();

    ssgi.Reset();

    postProcessing->Destroy();
    postProcessing.Reset();

    combinePass.Reset();

    reflectionsPass.Reset();

    lightmapPass.Reset();
    tonemapPass.Reset();
    mipChain.Reset();
    indirectPass.Reset();
    directPass.Reset();

    rayTracingReflections.Reset();
    ddgi.Reset();
}

#pragma endregion DeferredPassData

#pragma region RayTracingPassData

RayTracingPassData::~RayTracingPassData()
{
    EnqueueDeletion(std::move(rayTracingTlases));
}

#pragma endregion RayTracingPassData

#pragma region DeferredRenderer

static FramebufferRef CreateDeferredShadingFramebuffer(GBuffer* gbuffer)
{
    FramebufferDesc framebufferDesc {};
    framebufferDesc.extent = gbuffer->GetExtent();

    FramebufferRef framebuffer = g_renderInterface->MakeFramebuffer(framebufferDesc);

#if HYP_DEBUG_MODE
    framebuffer->SetDebugName(NAME("DeferredShadingFramebuffer"));
#endif

    Attachment* colorAttachment = framebuffer->AddAttachment(
        0,
        AttachmentDesc {
            TextureType::Texture2D,
            TextureFormat::RGBA16F,
            LoadOperation::CLEAR,
            StoreOperation::STORE
        });

    // depth for stencil testing
    const GpuImageViewRef& depthImageView = gbuffer->GetBucket(RenderBucket::Opaque).GetGBufferAttachment(GTN_DEPTH)->GetImageView();
    Assert(depthImageView.IsValid());

    AttachmentDesc depthAttachmentDesc {};
    depthAttachmentDesc.imageType = TextureType::Texture2D;
    depthAttachmentDesc.format = depthImageView->GetImage()->GetTextureFormat();
    depthAttachmentDesc.loadOp = LoadOperation::LOAD;
    depthAttachmentDesc.storeOp = StoreOperation::NONE;
    depthAttachmentDesc.onlyStencil = true;

    Attachment* depthAttachment = framebuffer->AddAttachment(
        1,
        depthAttachmentDesc,
        depthImageView);

    CheckResult(framebuffer->Create());

#if HYP_DEBUG_MODE
    colorAttachment->GetGpuImage()->SetDebugName(NAME("DeferredShadingTarget_Color"));
#endif

    return framebuffer;
}

class TileProcessor
{
public:
    static constexpr uint32 MaxEnvProbesPerTile = 8;
    static constexpr uint32 MaxLightsPerTile = 16;

    struct TileGridData
    {
        uint32 indexOffset;
        uint16 numLights;
        uint16 numEnvProbes;
    };

    struct Tile
    {
        uint16 numEnvProbes;
        uint16 numLights;

        uint16 envProbeIndices[MaxEnvProbesPerTile];
        uint16 lightIndices[MaxLightsPerTile];
    };

    struct TileDataAllocation
    {
        size_t gridBufferSize = 0;
        size_t indexBufferSize = 0;
        uint32 lastUsedFrame = UINT32_MAX;
    };

    Array<TileDataAllocation, RenderAllocator> tileDataPerView;

    TileProcessor()
    {

    }

    TileProcessor(const TileProcessor& other) = delete;
    TileProcessor& operator=(const TileProcessor& other) = delete;

    ~TileProcessor() = default;

    void ProcessView(const Viewport& viewport, View* view, StructuredBuffer*& outGridBuffer, StructuredBuffer*& outIndexBuffer)
    {
        Assert(view != nullptr);

        outGridBuffer = nullptr;
        outIndexBuffer = nullptr;

        // @TODO VP offset
        const Vec2u& extent = viewport.extent;

        const uint32 numTilesX = (extent.x + TileSize - 1) / TileSize;
        const uint32 numTilesY = (extent.y + TileSize - 1) / TileSize;
        const uint32 totalTiles = numTilesX * numTilesY * TileZBins;

        if (tileDataPerView.Size() <= view->Id().ToIndex())
        {
            tileDataPerView.Resize(view->Id().ToIndex() + 1);
        }

        Array<Tile, RenderAllocator> tempTiles;
        tempTiles.ResizeZeroed(totalTiles);

        RenderProxyList& rpl = GetConsumerProxyList(view);
        rpl.BeginRead();
        HYP_DEFER({ rpl.EndRead(); });

        RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(GetRenderProxy(view->GetCamera()));
        Assert(cameraProxy != nullptr);

        const Mat4f& cameraVP = cameraProxy->bufferData.viewProjMat;

        const float cameraNear = cameraProxy->bufferData.cameraNear;
        const float cameraFar = cameraProxy->bufferData.cameraFar;
        const float logFarOverNear = std::log2(cameraFar / cameraNear);

        const float scale = float(TileZBins) / logFarOverNear;
        const float bias = -(float(TileZBins) * std::log2(cameraNear)) / logFarOverNear;

        const Mat4f& viewMatrix = cameraProxy->bufferData.viewMat;
        const Mat4f& projMatrix = cameraProxy->bufferData.projMat;

        auto CalculateZBin = [scale, bias](float viewSpaceZ) -> int32
        {
            const float z = MathUtil::Max(viewSpaceZ, 0.0001f);
            const int32 zBin = int32(std::log2(z) * scale + bias);

            return MathUtil::Clamp(zBin, 0, int32(TileZBins) - 1);
        };

        auto ProjectSphereToScreenAABB = [&projMatrix, &extent, cameraNear, numTilesX, numTilesY](
            const Vec3f& centerVS, float radius,
            uint32& outMinX, uint32& outMinY, uint32& outMaxX, uint32& outMaxY) -> bool
        {
            const float dist = centerVS.z;

            if (dist + radius < cameraNear)
            {
                return false;
            }

            const float projScaleX = projMatrix[0][0];
            const float projScaleY = projMatrix[1][1];

            const float effectiveZ = MathUtil::Max(dist, cameraNear);
            const float invZ = 1.0f / effectiveZ;

            const float ndcCenterX = centerVS.x * projScaleX * invZ;
            const float ndcCenterY = centerVS.y * projScaleY * invZ;

            const float nearestZ = MathUtil::Max(dist - radius, cameraNear);
            const float invNearestZ = 1.0f / nearestZ;
            const float ndcRadiusX = radius * std::abs(projScaleX) * invNearestZ;
            const float ndcRadiusY = radius * std::abs(projScaleY) * invNearestZ;

            const float halfW = float(extent.x) * 0.5f;
            const float halfH = float(extent.y) * 0.5f;

            const float pixMinX = (ndcCenterX - ndcRadiusX) * halfW + halfW;
            const float pixMaxX = (ndcCenterX + ndcRadiusX) * halfW + halfW;
            const float pixMinY = (ndcCenterY - ndcRadiusY) * halfH + halfH;
            const float pixMaxY = (ndcCenterY + ndcRadiusY) * halfH + halfH;

            outMinX = uint32(MathUtil::Max(int32(pixMinX) / int32(TileSize), 0));
            outMinY = uint32(MathUtil::Max(int32(pixMinY) / int32(TileSize), 0));
            outMaxX = MathUtil::Min(uint32(MathUtil::Max(pixMaxX, 0.0f)) / TileSize, numTilesX - 1);
            outMaxY = MathUtil::Min(uint32(MathUtil::Max(pixMaxY, 0.0f)) / TileSize, numTilesY - 1);

            return outMinX <= outMaxX && outMinY <= outMaxY;
        };

        auto ProjectAABBToScreenTiles = [&viewMatrix, &projMatrix, &extent, cameraNear, numTilesX, numTilesY](
            const Vec3f& aabbMinWS, const Vec3f& aabbMaxWS,
            uint32& outMinX, uint32& outMinY, uint32& outMaxX, uint32& outMaxY,
            float& outMinVSZ, float& outMaxVSZ) -> bool
        {
            const Vec3f corners[8] = {
                { aabbMinWS.x, aabbMinWS.y, aabbMinWS.z },
                { aabbMaxWS.x, aabbMinWS.y, aabbMinWS.z },
                { aabbMinWS.x, aabbMaxWS.y, aabbMinWS.z },
                { aabbMaxWS.x, aabbMaxWS.y, aabbMinWS.z },
                { aabbMinWS.x, aabbMinWS.y, aabbMaxWS.z },
                { aabbMaxWS.x, aabbMinWS.y, aabbMaxWS.z },
                { aabbMinWS.x, aabbMaxWS.y, aabbMaxWS.z },
                { aabbMaxWS.x, aabbMaxWS.y, aabbMaxWS.z },
            };

            const float projScaleX = projMatrix[0][0];
            const float projScaleY = projMatrix[1][1];
            const float halfW = float(extent.x) * 0.5f;
            const float halfH = float(extent.y) * 0.5f;

            float ndcMinX = MathUtil::MaxSafeValue<float>();
            float ndcMinY = MathUtil::MaxSafeValue<float>();
            float ndcMaxX = MathUtil::MinSafeValue<float>();
            float ndcMaxY = MathUtil::MinSafeValue<float>();

            outMinVSZ = MathUtil::MaxSafeValue<float>();
            outMaxVSZ = MathUtil::MinSafeValue<float>();

            bool anyInFront = false;
            bool anyBehind = false;

            for (const Vec3f& corner : corners)
            {
                const Vec3f cornerVS = viewMatrix * corner;

                outMinVSZ = MathUtil::Min(outMinVSZ, cornerVS.z);
                outMaxVSZ = MathUtil::Max(outMaxVSZ, cornerVS.z);

                if (cornerVS.z < cameraNear)
                {
                    anyBehind = true;
                    continue;
                }

                anyInFront = true;

                const float invZ = 1.0f / cornerVS.z;
                const float ndcX = cornerVS.x * projScaleX * invZ;
                const float ndcY = cornerVS.y * projScaleY * invZ;

                ndcMinX = MathUtil::Min(ndcMinX, ndcX);
                ndcMinY = MathUtil::Min(ndcMinY, ndcY);
                ndcMaxX = MathUtil::Max(ndcMaxX, ndcX);
                ndcMaxY = MathUtil::Max(ndcMaxY, ndcY);
            }

            if (!anyInFront)
            {
                return false;
            }

            // If the AABB straddles the near plane, conservatively cover the full screen
            if (anyBehind)
            {
                ndcMinX = -1.0f;
                ndcMinY = -1.0f;
                ndcMaxX = 1.0f;
                ndcMaxY = 1.0f;
            }

            const float pixMinX = ndcMinX * halfW + halfW;
            const float pixMaxX = ndcMaxX * halfW + halfW;
            const float pixMinY = ndcMinY * halfH + halfH;
            const float pixMaxY = ndcMaxY * halfH + halfH;

            outMinX = uint32(MathUtil::Max(int32(pixMinX) / int32(TileSize), 0));
            outMinY = uint32(MathUtil::Max(int32(pixMinY) / int32(TileSize), 0));
            outMaxX = MathUtil::Min(uint32(MathUtil::Max(pixMaxX, 0.0f)) / TileSize, numTilesX - 1);
            outMaxY = MathUtil::Min(uint32(MathUtil::Max(pixMaxY, 0.0f)) / TileSize, numTilesY - 1);

            return outMinX <= outMaxX && outMinY <= outMaxY;
        };

        for (Light* light : rpl.GetLights())
        {
            const LightType lightType = light->GetLightType();

            if (!DeferredRendererHelpers::CanClusterLight(lightType))
            {
                continue;
            }

            const uint32 lightBindingIndex = Resources::GetBinding(light);

            if (lightBindingIndex == ~0u)
            {
                continue;
            }

            RenderProxyLight* lightProxy = static_cast<RenderProxyLight*>(GetRenderProxy(light));
            AssertDebug(lightProxy != nullptr);

            const Vec3f lightPosWS = lightProxy->bufferData.positionIntensity.GetXYZ();
            const Vec3f lightPosVS = viewMatrix * lightPosWS;
            const float lightRadius = float(Float16::FromRaw(uint16(lightProxy->bufferData.radiusFalloffPacked & 0xFFFFu)));

            uint32 tileMinX;
            uint32 tileMinY;
            uint32 tileMaxX;
            uint32 tileMaxY;

            if (!ProjectSphereToScreenAABB(lightPosVS, lightRadius, tileMinX, tileMinY, tileMaxX, tileMaxY))
            {
                continue;
            }

            const float lightDistVS = lightPosVS.z;
            const int32 zBinMin = CalculateZBin(MathUtil::Max(lightDistVS - lightRadius, cameraNear));
            const int32 zBinMax = CalculateZBin(MathUtil::Min(lightDistVS + lightRadius, cameraFar));

            for (int32 z = zBinMin; z <= zBinMax; z++)
            {
                for (uint32 y = tileMinY; y <= tileMaxY; y++)
                {
                    for (uint32 x = tileMinX; x <= tileMaxX; x++)
                    {
                        const uint32 clusterIndex = (uint32(z) * numTilesY + y) * numTilesX + x;

                        Tile& tile = tempTiles[clusterIndex];

                        if (tile.numLights < MaxLightsPerTile)
                        {
                            tile.lightIndices[tile.numLights++] = uint16(lightBindingIndex);
                        }
                    }
                }
            }
        }

        Array<Tuple<EnvProbe*, EnvProbeShaderData*, uint32>, RenderTempAllocator> envProbes;
        envProbes.Reserve(rpl.GetEnvProbes().NumCurrent());
        
        for (EnvProbe* envProbe : rpl.GetEnvProbes())
        {
            const uint32 envProbeBindingIndex = Resources::GetBinding(envProbe);

            if (envProbeBindingIndex == ~0u)
            {
                continue;
            }

            RenderProxyEnvProbe* envProbeProxy = static_cast<RenderProxyEnvProbe*>(GetRenderProxy(envProbe));
            AssertDebug(envProbeProxy != nullptr);

            envProbes.EmplaceBack(envProbe, &envProbeProxy->bufferData, envProbeBindingIndex);
        }

        // Sort env probes, we want sky first
        Vec3f cameraPosition = cameraProxy->bufferData.cameraPosition.GetXYZ();

        std::sort(envProbes.Begin(), envProbes.End(),
            [&cameraPosition](const Tuple<EnvProbe*, EnvProbeShaderData*, uint32>& a, const Tuple<EnvProbe*, EnvProbeShaderData*, uint32>& b)
            {
                const bool aIsSky = a.GetElement<0>()->IsA(SkyProbe::StaticClass());
                const bool bIsSky = b.GetElement<0>()->IsA(SkyProbe::StaticClass());

                if (aIsSky && !bIsSky)
                {
                    return false;
                }

                if (!aIsSky && bIsSky)
                {
                    return true;
                }

                if (aIsSky && bIsSky)
                {
                    return false;
                }

                // both are reflection probes, sort by distance to camera
                const Vec3f aProbePosition = a.GetElement<1>()->worldPosition.GetXYZ();
                const Vec3f bProbePosition = b.GetElement<1>()->worldPosition.GetXYZ();

                const float aDistSq = (aProbePosition - cameraPosition).LengthSquared();
                const float bDistSq = (bProbePosition - cameraPosition).LengthSquared();

                return aDistSq < bDistSq;
            });

        for (const Tuple<EnvProbe*, EnvProbeShaderData*, uint32>& tup : envProbes)
        {
            const EnvProbe& envProbe = *tup.GetElement<0>();
            const EnvProbeShaderData& envProbeData = *tup.GetElement<1>();
            const uint32 envProbeBindingIndex = tup.GetElement<2>();

            const Vec3f aabbMinWS = envProbeData.aabbMin.GetXYZ();
            const Vec3f aabbMaxWS = envProbeData.aabbMax.GetXYZ();

            const bool isSky = envProbe.GetEnvProbeType() == EPT_SKY;

            if (isSky)
            {
                for (Tile& tile : tempTiles)
                {
                    if (tile.numEnvProbes < MaxEnvProbesPerTile)
                    {
                        tile.envProbeIndices[tile.numEnvProbes++] = uint16(envProbeBindingIndex);
                    }
                }
            }
            else
            {
                uint32 tileMinX;
                uint32 tileMinY;
                uint32 tileMaxX;
                uint32 tileMaxY;
                float probeVSZMin;
                float probeVSZMax;

                if (!ProjectAABBToScreenTiles(aabbMinWS, aabbMaxWS, tileMinX, tileMinY, tileMaxX, tileMaxY, probeVSZMin, probeVSZMax))
                {
                    continue;
                }

                const int32 zBinMin = CalculateZBin(MathUtil::Max(probeVSZMin, cameraNear));
                const int32 zBinMax = CalculateZBin(MathUtil::Min(probeVSZMax, cameraFar));

                for (int32 z = zBinMin; z <= zBinMax; z++)
                {
                    for (uint32 y = tileMinY; y <= tileMaxY; y++)
                    {
                        for (uint32 x = tileMinX; x <= tileMaxX; x++)
                        {
                            const uint32 clusterIndex = (uint32(z) * numTilesY + y) * numTilesX + x;

                            Tile& tile = tempTiles[clusterIndex];

                            if (tile.numEnvProbes < MaxEnvProbesPerTile)
                            {
                                tile.envProbeIndices[tile.numEnvProbes++] = uint16(envProbeBindingIndex);
                            }
                        }
                    }
                }
            }
        }

        Array<TileGridData, RenderAllocator> gridData;
        gridData.Resize(totalTiles);
        
        Array<uint16, RenderAllocator> flatIndexData;
        flatIndexData.Reserve(totalTiles * 4);

        uint32 offset = 0;

        for (uint32 i = 0; i < totalTiles; ++i)
        {
            const Tile& tile = tempTiles[i];
            
            gridData[i].indexOffset = uint32(flatIndexData.Size());
            gridData[i].numLights = tile.numLights;
            gridData[i].numEnvProbes = tile.numEnvProbes;

            flatIndexData.Resize(offset + tile.numLights + tile.numEnvProbes);

            for (uint16 j = 0; j < tile.numLights; j++)
            {
                flatIndexData[offset + j] = tile.lightIndices[j];
            }

            offset += tile.numLights;

            for (uint16 j = 0; j < tile.numEnvProbes; j++)
            {
                flatIndexData[offset + j] = tile.envProbeIndices[j];
            }

            offset += tile.numEnvProbes;
        }

        if (flatIndexData.Empty())
        {
            flatIndexData.Resize(1);
        }

        TileDataAllocation& allocation = tileDataPerView[view->Id().ToIndex()];
        allocation.lastUsedFrame = GetFrameCounter();

        StructuredBuffer& gridBuffer = g_renderInterface->sbufferAllocator->AcquireBuffer(gridData.Size(), sizeof(TileGridData));
        StructuredBuffer& indexBuffer = g_renderInterface->sbufferAllocator->AcquireBuffer(flatIndexData.Size(), sizeof(uint16));
        
        allocation.gridBufferSize = gridBuffer.gpuBuffer->Size();
        allocation.indexBufferSize = indexBuffer.gpuBuffer->Size();

        gridBuffer.Write(0, gridData.Size() * sizeof(TileGridData), gridData.Data());
        gridBuffer.Flush();

        indexBuffer.Write(0, flatIndexData.Size() * sizeof(uint16), flatIndexData.Data());
        indexBuffer.Flush();

        outGridBuffer = &gridBuffer;
        outIndexBuffer = &indexBuffer;
    }
};

DeferredRenderer::DeferredRenderer()
    : m_tileProcessor(MakeUnique<TileProcessor>())
{
}

DeferredRenderer::~DeferredRenderer()
{
}

void DeferredRenderer::Initialize()
{
}

void DeferredRenderer::Shutdown()
{
    m_quadMesh.Reset();
}

PassData* DeferredRenderer::CreateViewPassData(View* view, PassDataExt&)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    Assert(view != nullptr);

    if (view->GetFlags() & ViewFlags::GBUFFER)
    {
        DeferredRendererPassData* pd = new DeferredRendererPassData();
        DeferredRendererPassData& passData = *pd;

        passData.view = MakeWeakRef(view);

        GBuffer* gbuffer = view->GetOutputTarget().GetGBuffer();
        Assert(gbuffer != nullptr);

        gbuffer->Create();

        AssertDebug(gbuffer->IsCreated());

        HYP_LOG(Rendering, Verbose, "Creating renderer for view '{}' with GBuffer '{}'", view->Id(), gbuffer->GetExtent());

        const FramebufferRef& opaquePassFramebuffer = view->GetOutputTarget().GetFramebuffer(RenderBucket::Opaque);
        const FramebufferRef& lightmapPassFramebuffer = view->GetOutputTarget().GetFramebuffer(RenderBucket::Lightmapped);

        passData.ssgi = MakeUnique<SSGI>(gbuffer);
        passData.ssgi->Create();

        passData.postProcessing = MakeUnique<PostProcessing>();
        passData.postProcessing->Create();

        passData.deferredShadingFramebuffer = CreateDeferredShadingFramebuffer(gbuffer);

        passData.indirectPass = MakeUnique<DeferredPass>(DPM_INDIRECT_LIGHTING, gbuffer->GetExtent(), gbuffer, passData.deferredShadingFramebuffer);
        passData.indirectPass->Create();

        passData.directPass = MakeUnique<DeferredPass>(DPM_DIRECT_LIGHTING, gbuffer->GetExtent(), gbuffer, passData.deferredShadingFramebuffer);
        passData.directPass->Create();

        passData.depthPyramidRenderer = MakeUnique<DepthPyramidRenderer>(gbuffer);
        passData.depthPyramidRenderer->Create();

        passData.cullData.depthPyramidImageView = passData.depthPyramidRenderer->GetResultImageView();
        passData.cullData.depthPyramidDimensions = passData.depthPyramidRenderer->GetExtent();

        passData.mipChain = MakeHandle<Texture>(TextureDesc {
            TextureType::Texture2D,
            opaquePassFramebuffer->GetAttachment(0)->GetFormat(),
            Vec3u(opaquePassFramebuffer->GetExtent(), 1),
            TFM_LINEAR_MIPMAP,
            TFM_LINEAR_MIPMAP,
            TWM_CLAMP_TO_EDGE
        });

        CheckResult(passData.mipChain->Create());

        passData.hbao = MakeUnique<HBAO>(gbuffer->GetExtent(), gbuffer);
        passData.hbao->Create();

        // m_dofBlur = MakeUnique<DOFBlur>(gbuffer->GetResolution(), gbuffer);
        // m_dofBlur->Create();

        passData.reflectionsPass = MakeUnique<ReflectionsPass>(
            gbuffer->GetExtent(),
            gbuffer,
            g_renderInterface->textureViewCache->GetOrCreate(passData.mipChain));

        passData.reflectionsPass->Create();

        passData.tonemapPass = MakeUnique<TonemapPass>(gbuffer->GetExtent(), gbuffer);
        passData.tonemapPass->Create();

        // We'll render the lightmap pass into the translucent framebuffer after deferred shading has been applied to OPAQUE objects.
        passData.lightmapPass = MakeUnique<LightmapPass>();
        passData.lightmapPass->Create();

        passData.fogVolumePass = MakeUnique<FogVolumePass>();
        passData.fogVolumePass->Create();

        passData.taaPass = MakeUnique<TAAPass>(passData.tonemapPass->GetFinalImageView(), gbuffer->GetExtent(), gbuffer);
        passData.taaPass->Create();

        CreateViewRayTracingPasses(view, passData);

        return pd;
    }
    else if ((view->GetFlags() & ViewFlags::RAY_TRACING) && g_renderInterface->GetRenderConfig().rayTracing)
    {
        RayTracingPassData* pd = new RayTracingPassData();
        RayTracingPassData& passData = *pd;

        passData.view = MakeWeakRef(view);

        return pd;
    }

    HYP_LOG(Rendering, Fatal,
        "Cannot create PassData for View {}! View does not have any flags set that would allow us to create PassData for it. View flags: {}",
        view->Id(), uint32(view->GetFlags()));

    return nullptr;
}

void DeferredRenderer::CreateViewRayTracingPasses(View* view, DeferredRendererPassData& passData)
{
    AssertOnThread(g_renderThread);

    if (!g_renderInterface->GetRenderConfig().rayTracing)
    {
        return;
    }

    const bool shouldEnableRayTracingForView = view->GetRayTracingView().IsValid()
        && cvRayTracingEnabled.Get();

    if (!shouldEnableRayTracingForView)
    {
        passData.rayTracingReflections.Reset();
        passData.ddgi.Reset();

        return;
    }

    GBuffer* gbuffer = view->GetOutputTarget().GetGBuffer();
    AssertDebug(gbuffer != nullptr);

    passData.rayTracingReflections = MakeUnique<RayTracingReflections>(RayTracingReflectionsConfig::FromConfig(), gbuffer);
    passData.rayTracingReflections->Create();

    /// FIXME: Proper AABB for DDGI
    passData.ddgi = MakeUnique<DDGI>(DDGIInfo { .aabb = { { -30.0f, -5.0f, -30.0f }, { 30.0f, 35.0f, 30.0f } } });
    passData.ddgi->Create();
}

void DeferredRenderer::CreateViewTopLevelAccelerationStructures(View* view, RayTracingPassData& passData)
{
    EnqueueDeletion(std::move(passData.rayTracingTlases));

    // Hack to fix driver crash when building TLAS with no meshes
    Handle<Mesh> defaultMesh = MeshBuilder::Cube(true);
    defaultMesh->SetFlags(MeshFlags::ViewIndependent);
    InitObject(defaultMesh);

    GpuBlasRef blas = MeshBlasBuilder::Build(defaultMesh);
    CheckResult(blas->Create());

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
    {
        GpuTlasRef& tlas = passData.rayTracingTlases[frameIndex];

        tlas = g_renderInterface->MakeTLAS();
        tlas->AddGpuBlas(0, blas);

        CheckResult(tlas->Create());
    }
}

void DeferredRenderer::ResizeView(Viewport viewport, View* view, DeferredRendererPassData& passData)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    HYP_LOG(Rendering, Verbose, "Resizing View '{}' to {}x{}", view->Id(), viewport.extent.x, viewport.extent.y);

    Assert(viewport.extent.Volume() > 0);

    const Vec2u newSize = Vec2u(viewport.extent);

    GBuffer* gbuffer = view->GetOutputTarget().GetGBuffer();
    Assert(gbuffer != nullptr && gbuffer->IsCreated());

    gbuffer->Resize(newSize);

    const FramebufferRef& opaquePassFramebuffer = view->GetOutputTarget().GetFramebuffer(RenderBucket::Opaque);
    const FramebufferRef& lightmapPassFramebuffer = view->GetOutputTarget().GetFramebuffer(RenderBucket::Lightmapped);

    if (passData.deferredShadingFramebuffer.IsValid())
    {
        EnqueueDeletion(std::move(passData.deferredShadingFramebuffer));
    }

    passData.deferredShadingFramebuffer = CreateDeferredShadingFramebuffer(gbuffer);

    passData.directPass->Resize(newSize);
    passData.indirectPass->Resize(newSize);

    passData.hbao = MakeUnique<HBAO>(viewport.extent, gbuffer);
    passData.hbao->Create();

    passData.ssgi.Reset();
    passData.ssgi = MakeUnique<SSGI>(gbuffer);
    passData.ssgi->Create();

    passData.reflectionsPass.Reset();
    passData.reflectionsPass = MakeUnique<ReflectionsPass>(
        newSize,
        gbuffer,
        g_renderInterface->textureViewCache->GetOrCreate(passData.mipChain));

    passData.reflectionsPass->Create();

    passData.tonemapPass = MakeUnique<TonemapPass>(viewport.extent, gbuffer);
    passData.tonemapPass->Create();

    passData.lightmapPass = MakeUnique<LightmapPass>();
    passData.lightmapPass->Create();

    passData.fogVolumePass = MakeUnique<FogVolumePass>();
    passData.fogVolumePass->Create();

    passData.taaPass = MakeUnique<TAAPass>(passData.tonemapPass->GetFinalImageView(), newSize, gbuffer);
    passData.taaPass->Create();

    passData.depthPyramidRenderer = MakeUnique<DepthPyramidRenderer>(gbuffer);
    passData.depthPyramidRenderer->Create();

    CreateViewRayTracingPasses(view, passData);

    passData.view = MakeWeakRef(view);
}

void DeferredRenderer::RenderFrame(Frame* frame, const RenderSetup& rs)
{
    HYP_SCOPE;

    AssertDebug(rs.world);

    Array<RenderProxyList*, InlineAllocator<8, RenderAllocator>> renderProxyLists;

    HYP_DEFER({
        for (RenderProxyList* rpl : renderProxyLists)
        {
            rpl->EndRead();
        }
    });

    if (!m_quadMesh)
    {
        m_quadMesh = MeshBuilder::Quad();
        m_quadMesh->SetFlags(MeshFlags::ViewIndependent);
        InitObject(m_quadMesh);
    }

    // Collect view-independent renderable types from all views, binned
    //// \todo : We could use the existing binning by subclass that ResourceTracker now provides.
    FixedArray<FlatSet<EnvProbe*>, EPT_MAX> envProbes;
    FixedArray<FlatSet<Light*>, NumLightTypes> lights;
    FlatSet<EnvGrid*> envGrids;

    // For rendering EnvGrids and EnvProbes, we use a directional light from one of the Views that references it (if found)
    FlatMap<EnvGrid*, Light*> envGridLights;
    FlatMap<EnvProbe*, Light*> envProbeLights;

    // init view pass data and collect global rendering resources
    // (env probes, env grids)
    for (View* view : rs.world->GetViews())
    {
        AssertDebug(view != nullptr);

        RenderProxyList& rpl = GetConsumerProxyList(view);
        rpl.BeginRead();

        renderProxyLists.PushBack(&rpl);

        if (view->GetFlags() & ViewFlags::GBUFFER)
        {
            GBuffer* gbuffer = view->GetOutputTarget().GetGBuffer();

            if (!gbuffer || gbuffer->GetExtent() != rs.viewport.extent)
            {
                PassData* pd = FetchViewPassData(view);
                Assert(pd != nullptr);

                DeferredRendererPassData* pdCasted = ObjCast<DeferredRendererPassData>(pd);
                Assert(pdCasted != nullptr);

                pdCasted->priority = view->GetPriority();

                if (gbuffer->GetExtent() != rs.viewport.extent)
                {
                    ResizeView(rs.viewport, view, *pdCasted);
                }
            }
        }
        else if ((view->GetFlags() & ViewFlags::RAY_TRACING) && g_renderInterface->GetRenderConfig().rayTracing)
        {
            PassData* pd = FetchViewPassData(view);
            Assert(pd != nullptr);

            RayTracingPassData* pdCasted = ObjCast<RayTracingPassData>(pd);
            Assert(pdCasted != nullptr);

            RenderSetup newRenderSetup = rs.Fork();
            newRenderSetup.passData = pd;
            newRenderSetup.view = view;

            UpdateRayTracingView(frame, newRenderSetup);
        }

        for (Light* light : rpl.GetLights())
        {
            AssertDebug(light != nullptr);

            lights[uint32(light->GetLightType())].Insert(light);
        }

        for (EnvProbe* envProbe : rpl.GetEnvProbes())
        {
            if (envProbes[envProbe->GetEnvProbeType()].Contains(envProbe))
            {
                continue;
            }

            if (!envProbeLights.Contains(envProbe))
            {
                for (Light* light : rpl.GetLights())
                {
                    AssertDebug(light != nullptr);

                    if (light->GetLightType() == LightType::Directional)
                    {
                        envProbeLights[envProbe] = light;

                        break;
                    }
                }
            }

            envProbes[envProbe->GetEnvProbeType()].Insert(envProbe);
        }

        for (EnvGrid* envGrid : rpl.GetEnvGrids())
        {
            if (envGrids.Contains(envGrid))
            {
                continue;
            }

            if (!envGridLights.Contains(envGrid))
            {
                for (Light* light : rpl.GetLights())
                {
                    if (light->GetLightType() == LightType::Directional)
                    {
                        envGridLights[envGrid] = light;

                        break;
                    }
                }
            }

            envGrids.Insert(envGrid);
        }
    }

    {
        RenderSetup envProbeSetup = rs.Fork();

        // Set sky as fallback probe
        if (envProbes[EPT_SKY].Any())
        {
            envProbeSetup.envProbe = envProbes[EPT_SKY].Front();
        }

        if (lights[uint32(LightType::Directional)].Any())
        {
            envProbeSetup.light = lights[uint32(LightType::Directional)].Front();
        }

        if (envProbes.Any())
        {
            // check for dynamic env probes to render
            for (uint32 envProbeType = 0; envProbeType <= EPT_REFLECTION; envProbeType++)
            {
                if (RendererBase* renderer = g_renderInterface->globalRenderers[GRT_ENV_PROBE][envProbeType])
                {
                    for (EnvProbe* envProbe : envProbes[envProbeType])
                    {
                        if (envProbe->IsBaked())
                        {
                            continue; // skip baked
                        }

                        RenderSetup currentEnvProbeSetup = envProbeSetup.Fork();
                        currentEnvProbeSetup.envProbe = envProbe;

                        renderer->RenderFrame(frame, currentEnvProbeSetup);
                    }
                }
                else
                {
                    HYP_LOG(Rendering, Warning, "No EnvProbeRenderer found for EnvProbeType {}! Skipping rendering of env probes of this type.", EPT_REFLECTION);
                }
            }
        }

        if (envGrids.Any())
        {
            for (EnvGrid* envGrid : envGrids)
            {
                RenderSetup envGridSetup = envProbeSetup.Fork();

                // Set global directional light as fallback
                if (envGridLights.Contains(envGrid))
                {
                    envGridSetup.light = envGridLights[envGrid];
                }

                envGridSetup.envGrid = envGrid;

                g_renderInterface->globalRenderers[GRT_ENV_GRID][0]->RenderFrame(frame, envGridSetup);
            }
        }
    }

    for (View* view : rs.world->GetViews())
    {
        if (!(view->GetFlags() & ViewFlags::GBUFFER))
        {
            continue;
        }

        DeferredRendererPassData* pd = ObjCast<DeferredRendererPassData>(FetchViewPassData(view));
        AssertDebug(pd != nullptr);

        RenderSetup currentViewSetup = rs.Fork();
        currentViewSetup.view = view;
        currentViewSetup.passData = pd;

        RenderFrameForView(frame, currentViewSetup);

        RenderProxyList& rpl = GetConsumerProxyList(view);

        g_statViews++;
        g_statTextures += rpl.GetTextures().NumCurrent();
        g_statMaterials += rpl.GetMaterials().NumCurrent();
        g_statLightmapVolumes += rpl.GetLightmapVolumes().NumCurrent();
        g_statParticleVolumes += rpl.GetParticleVolumes().NumCurrent();
        g_statLights += rpl.GetLights().NumCurrent();
        g_statEnvGrids += rpl.GetEnvGrids().NumCurrent();
        g_statEnvProbes += rpl.GetEnvProbes().NumCurrent();

#if 0
        HYP_LOG(Rendering, Verbose, "View '{}' used {} textures, {} materials, {} lightmap volumes, {} lights, {} env grids and {} env probes.",
            view->Id(),
            rpl.GetTextures().NumCurrent(),
            rpl.GetMaterials().NumCurrent(),
            rpl.GetLightmapVolumes().NumCurrent(),
            rpl.GetLights().NumCurrent(),
            rpl.GetEnvGrids().NumCurrent(),
            rpl.GetEnvProbes().NumCurrent());
#endif
    }
}

void DeferredRenderer::RenderFrameForView(Frame* frame, const RenderSetup& rs)
{
    HYP_SCOPE;

    AssertDebug(rs.world && rs.view);

    uint32 slot = GetRingIndex();
    if (m_renderedViewOutputs.frameId != slot)
    {
        m_renderedViewOutputs.frameId = slot;
        m_renderedViewOutputs.items.Clear();
    }

    View* view = rs.view;
    Assert(view->GetFlags() & ViewFlags::GBUFFER);

    RenderProxyList& rpl = GetConsumerProxyList(view);
    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    RenderCollector& renderCollector = GetRenderCollector(view);

    // must be before BeginRecordDrawCalls
    PerformOcclusionCulling(frame, rs, renderCollector);
    
    renderCollector.BeginRecordDrawCalls(frame, rs, RenderBucketMask<
        RenderBucket::Opaque, RenderBucket::Translucent, RenderBucket::Lightmapped, RenderBucket::Sky>);

    DeferredRendererPassData* passDataCasted = ObjCast<DeferredRendererPassData>(rs.passData);
    AssertDebug(passDataCasted != nullptr);

    DeferredRendererPassData& passData = *passDataCasted;

    const uint32 frameIndex = frame->GetFrameIndex();

    m_tileProcessor->ProcessView(
        rs.viewport,
        view,
        passData.gridTilesBuffer,
        passData.gridIndexBuffer);

    if (cvBypassDrawing.Get())
    {
        return;
    }

    // Render shadows for shadow casting lights
    for (Light* light : rpl.GetLights())
    {
        RendererBase* shadowRenderer = g_renderInterface->globalRenderers[GRT_SHADOW_MAP][uint32(light->GetLightType())];

        if (!shadowRenderer)
        {
            continue;
        }

        if (!(light->GetLightFlags() & LightFlags::ShadowCaster))
        {
            continue;
        }

        bool isLightInFrustum = false;

        if (view->GetFlags() & ViewFlags::NO_FRUSTUM_CULLING)
        {
            isLightInFrustum = true;
        }
        else
        {
            switch (light->GetLightType())
            {
            case LightType::Directional:
                isLightInFrustum = true;
                break;
            case LightType::Point:
                isLightInFrustum = view->GetSubFrustum().ContainsBoundingSphere(light->GetBoundingSphere(true));
                break;
            case LightType::Spot:
                /// \todo Implement frustum culling for spot lights
                isLightInFrustum = true;
                break;
            case LightType::AreaRect:
                isLightInFrustum = view->GetSubFrustum().ContainsAABB(light->GetWorldBounds());
                break;
            default:
                break;
            }
        }

        if (!isLightInFrustum)
            // Skip shadow view creation/update if the light is totally out of view.
            continue;
        
        RenderSetup shadowRs = rs.Fork();
        shadowRs.light = light;

        shadowRenderer->RenderFrame(frame, shadowRs);
    }

    Framebuffer* opaquePassFramebuffer = view->GetOutputTarget().GetFramebuffer(RenderBucket::Opaque);
    Framebuffer* lightmapPassFramebuffer = view->GetOutputTarget().GetFramebuffer(RenderBucket::Lightmapped);
    Framebuffer* translucentPassFramebuffer = view->GetOutputTarget().GetFramebuffer(RenderBucket::Translucent);
    Framebuffer* debugPassFramebuffer = view->GetOutputTarget().GetFramebuffer(RenderBucket::Debug);

    const bool doParticles = true;

    const bool useRayTracingReflections = (cvPathTracing.Get() || cvRayTracedReflections.Get())
        && view->GetRayTracingView().IsValid()
        && passData.rayTracingReflections != nullptr;

    const bool useRayTracingGlobalIllumination = cvRayTracedGI.Get()
        && view->GetRayTracingView().IsValid()
        && passData.ddgi != nullptr;

    if (cvTAA.Get())
    {
        // apply jitter to camera for TAA
        RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(GetRenderProxy(view->GetCamera()));
        Assert(cameraProxy != nullptr);

        CameraShaderData& cameraBufferData = cameraProxy->bufferData;

        if (MathUtil::ApproxEqual(cameraBufferData.projMat[3][3], 0.0f))
        {
            const uint32 frameCounter = GetWorldBufferData()->frameCounter + 1;

            Vec4f jitter = Vec4f::Zero();
            Mat4f::Jitter(frameCounter, rs.viewport.extent.x, rs.viewport.extent.y, jitter);

            cameraBufferData.jitter = jitter * CameraJitterScale;

            UpdateGpuData(view->GetCamera());
        }
    }
    
    // render opaque objects into separate framebuffer
    frame->cr << SetCurrentFramebuffer(opaquePassFramebuffer);

    // if no opaque objects will be rendered, we need to clear the color target anyway
    // as other passes are using load ops
    if (renderCollector.mappingsByBucket[uint32(RenderBucket::Opaque)].Any()
        || (!cvEnableLightmapVolumes.Get() && renderCollector.mappingsByBucket[uint32(RenderBucket::Lightmapped)].Any()))
    {
        renderCollector.ExecuteDrawCalls(frame, rs, RenderBucketMask<RenderBucket::Opaque>);

        if (!cvEnableLightmapVolumes.Get())
        {
            renderCollector.ExecuteDrawCalls(frame, rs, RenderBucketMask<RenderBucket::Lightmapped>);
        }
    }
    else
    {
        frame->cr << ClearFramebuffer(opaquePassFramebuffer, 0x1);
    }

    frame->cr << SetCurrentFramebuffer(nullptr);
    
    if (cvEnableLightmapVolumes.Get())
    {
        // render objects to be lightmapped, separate from the opaque objects.
        // The lightmap bucket's framebuffer has a color attachment that will write into the opaque framebuffer's color attachment.
        if (renderCollector.mappingsByBucket[uint32(RenderBucket::Lightmapped)].Any())
        {
            frame->cr << SetCurrentFramebuffer(lightmapPassFramebuffer);

            renderCollector.ExecuteDrawCalls(frame, rs, RenderBucketMask<RenderBucket::Lightmapped>);

            frame->cr << SetCurrentFramebuffer(nullptr);
        }
    }

    if ((useRayTracingGlobalIllumination || useRayTracingReflections) && view->GetRayTracingView().IsValid())
    {
        Handle<View> rayTracingView = view->GetRayTracingView().Lock();

        if (rayTracingView != nullptr)
        {
            RayTracingPassData* rayTracingPassData = ObjCast<RayTracingPassData>(FetchViewPassData(rayTracingView));
            Assert(rayTracingPassData != nullptr);

            const GpuTlasRef& tlas = rayTracingPassData->rayTracingTlases[frameIndex];

            if (tlas && tlas->IsCreated())
            {
                Assert(tlas->GetMeshDescriptionsBuffer() != nullptr);

                rayTracingPassData->parentPass = &passData;

                RenderSetup rayTracingRS = rs.Fork();
                rayTracingRS.passData = rayTracingPassData;

                // set sky as fallback

                // Set first found sky probe as fallback probe
                auto& skyProbes = rpl.GetEnvProbes().GetElements<SkyProbe>();
                if (skyProbes.Any())
                {
                    rayTracingRS.envProbe = skyProbes.Front();
                }

                if (useRayTracingReflections)
                {
                    AssertDebug(passData.rayTracingReflections != nullptr);
                    passData.rayTracingReflections->Render(frame, rayTracingRS);
                }

                if (useRayTracingGlobalIllumination)
                {
                    AssertDebug(passData.ddgi != nullptr);
                    passData.ddgi->Render(frame, rayTracingRS);
                }

                // unset parent pass after using it
                rayTracingPassData->parentPass = nullptr;
            }
        }
    }

    if (cvHBAO.Get())
    {
        passData.hbao->Render(frame, rs);
    }
    
    if (cvSSGI.Get())
    {
        passData.ssgi->Render(frame, rs);

        if (Texture* ssgiResultTexture = passData.ssgi->GetFinalResultTexture())
        {
            // make sure it is in a state for reading, we don't want any transitions between lightmap -> deferred indirect pass.
            frame->cr << InsertBarrier(
                ssgiResultTexture->GetGpuImage(),
                RS_SHADER_RESOURCE,
                ShaderModuleType::Pixel);
        }
    }

    if (cvSSR.Get())
    {
        passData.reflectionsPass->ssrPass->Render(frame, rs);

        if (Texture* ssrResultTexture = passData.reflectionsPass->ssrPass->GetFinalResultTexture())
        {
            // make sure it is in a state for reading, we don't want any transitions between lightmap -> deferred indirect pass.
            frame->cr << InsertBarrier(
                ssrResultTexture->GetGpuImage(),
                RS_SHADER_RESOURCE,
                ShaderModuleType::Pixel);
        }
    }

    passData.postProcessing->RenderPre(frame, rs);

    { // deferred lighting on opaque objects
        ENGINE_STAT_SCOPE(&s_statDeferredPass);

        frame->cr << InsertBarrier(
            passData.deferredShadingFramebuffer->GetAttachment(1)->GetGpuImage(),
            RS_RENDER_TARGET,
            ShaderModuleType::Pixel,
            /* onlyDepth */ false,
            /* onlyStencil */ true);

        frame->cr << SetCurrentFramebuffer(passData.deferredShadingFramebuffer);
        
        if (cvEnableLightmapVolumes.Get())
        {
            // apply baked lighting over lightmapped objects
            for (LightmapVolume* lightmapVolume : rpl.GetLightmapVolumes())
            {
                RenderSetup lightmapPassRS = rs.Fork();
                lightmapPassRS.volume = lightmapVolume;

                // Render the objects to have lightmaps applied into the translucent pass framebuffer with a full screen quad.
                // Apply lightmaps over the now shaded opaque objects.
                passData.lightmapPass->RenderToFramebuffer(frame, lightmapPassRS, passData.deferredShadingFramebuffer);
            }
        }

        passData.indirectPass->RenderToFramebuffer(frame, rs, passData.deferredShadingFramebuffer);
        passData.directPass->RenderToFramebuffer(frame, rs, passData.deferredShadingFramebuffer);

        frame->cr << SetCurrentFramebuffer(nullptr);
    }

    { // generate mipchain after rendering opaque objects' lighting, now we can use it for transmission
        const GpuImageRef& srcImage = passData.deferredShadingFramebuffer->GetAttachment(0)->GetGpuImage();
        GenerateMipChain(frame, rs, renderCollector, srcImage);
    }

    { // render Hi-Z
        passData.depthPyramidRenderer->Render(frame);

        passData.cullData.depthPyramidImageView = passData.depthPyramidRenderer->GetResultImageView();
        passData.cullData.depthPyramidDimensions = passData.depthPyramidRenderer->GetExtent();
    }

    { // combined + translucent (forward pass)
        frame->cr << SetCurrentFramebuffer(translucentPassFramebuffer);

        { // Render the deferred lighting into the translucent pass framebuffer with a full screen quad.

            frame->cr << SetCurrentViewport(rs.viewport);

            frame->cr << SetInputLayout(StaticVertexInputLayout<VT_Simple>);
            frame->cr << SetFaceCullMode(FCM_BACK);
            frame->cr << SetFillMode(FM_FILL);
            frame->cr << SetTopology(TOP_TRIANGLES);
            frame->cr << SetDepthTest(false);
            frame->cr << SetDepthWrite(false);
            frame->cr << SetStencilTest(false);

            frame->cr << SetCurrentShader(ShaderDesc(NAME("BlitTexture")));

            frame->cr << SetShaderUniform(0, "SamplerLinear"_sh, g_renderInterface->placeholderData->GetSamplerLinear());
            frame->cr << SetShaderUniform(1, "WorldsBuffer"_sh, g_renderInterface->namedBuffers[NamedBuffer::Worlds].gpuBuffer);
            frame->cr << SetShaderUniform(2, "InTexture"_sh, passData.deferredShadingFramebuffer->GetAttachment(0)->GetImageView());

            frame->cr << CommitDrawState();

            frame->cr << BindVertexBuffer(m_quadMesh->GetVertexBuffer());
            frame->cr << BindIndexBuffer(m_quadMesh->GetIndexBuffer());

            frame->cr << DrawIndexed(6);

            // reset
            frame->cr << SetDepthTest(true);
            frame->cr << SetDepthWrite(true);
        }

        // begin translucent with forward rendering
        if (renderCollector.mappingsByBucket[uint32(RenderBucket::Translucent)].Any())
        {
            renderCollector.ExecuteDrawCalls(frame, rs, RenderBucketMask<RenderBucket::Translucent>);
        }

        if (renderCollector.mappingsByBucket[uint32(RenderBucket::Sky)].Any())
        {
            renderCollector.ExecuteDrawCalls(frame, rs, RenderBucketMask<RenderBucket::Sky>);
        }

        // render fog volumes
        for (FogVolume* fogVolume : rpl.GetFogVolumes())
        {
            RenderSetup fogVolumeRS = rs.Fork();
            fogVolumeRS.volume = fogVolume;

            passData.fogVolumePass->RenderToFramebuffer(frame, fogVolumeRS, translucentPassFramebuffer);
        }

        // render particles
        if (rpl.GetParticleVolumes().NumCurrent())
        {
            for (ParticleVolume* particleVolume : rpl.GetParticleVolumes())
            {
                RenderSetup particleVolumeRS = rs.Fork();
                particleVolumeRS.volume = particleVolume;

                g_renderInterface->globalRenderers[GRT_PARTICLE_VOLUME][0]->RenderFrame(frame, particleVolumeRS);
            }
        }

        frame->cr << SetCurrentFramebuffer(nullptr);
    }

    // debug draw
    if (renderCollector.mappingsByBucket[uint32(RenderBucket::Debug)].Any()
        || DebugDrawer::GetInstance().NumEnqueuedDrawCommands() > 0)
    {
        frame->cr << SetCurrentFramebuffer(debugPassFramebuffer);

        ExecuteDrawCalls(frame, rs, renderCollector, RenderBucketMask<RenderBucket::Debug>);

        DebugDrawer::GetInstance().Render(frame, rs);

        frame->cr << SetCurrentFramebuffer(nullptr);
    }

    passData.postProcessing->RenderPost(frame, rs);

    passData.tonemapPass->Render(frame, rs);

    if (passData.taaPass != nullptr && cvTAA.Get())
    {
        passData.taaPass->Render(frame, rs);
    }

    // depth of field
    // m_dofBlur->Render(frame);

    GpuImageViewRef finalImageView = (passData.taaPass != nullptr && cvTAA.Get())
        ? g_renderInterface->textureViewCache->GetOrCreate(passData.taaPass->GetResultTexture())
        : passData.tonemapPass->GetFinalImageView();

    // Ordered by View priority
    auto outputsIt = std::lower_bound(
        m_renderedViewOutputs.items.Begin(),
        m_renderedViewOutputs.items.End(),
        passData.priority,
        [](const RenderedViewOutput& a, int priority)
        {
            return a.priority < priority;
        });

    m_renderedViewOutputs.items.Insert(outputsIt, RenderedViewOutput { view, std::move(finalImageView), passData.priority });
}

void DeferredRenderer::UpdateRayTracingView(Frame* frame, const RenderSetup& rs)
{
    HYP_SCOPE;

    View* view = rs.view;
    AssertDebug(view != nullptr);

    if (!(view->GetFlags() & ViewFlags::RAY_TRACING) || !g_renderInterface->GetRenderConfig().rayTracing)
    {
        return;
    }

    const uint32 currentFrameIndex = frame->GetFrameIndex();

    RayTracingPassData* pd = ObjCast<RayTracingPassData>(rs.passData);

    RenderProxyList& rpl = GetConsumerProxyList(rs.view);
    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    if (!pd->rayTracingTlases[currentFrameIndex])
    {
        for (GpuTlasRef& tlas : pd->rayTracingTlases)
        {
            tlas = g_renderInterface->MakeTLAS();
        }
    }

    bool hasBlas = false;

    for (Entity* entity : rpl.GetMeshEntities())
    {
        AssertDebug(entity != nullptr);

        RenderProxyMesh* meshProxy = rpl.GetMeshEntities().GetProxy(entity->Id());
        Assert(meshProxy != nullptr);

        AssertDebug(meshProxy->mesh != nullptr && meshProxy->mesh->IsReady());
        AssertDebug(meshProxy->material != nullptr && meshProxy->material->IsReady());

        const RenderBucket bucket = meshProxy->material->GetAttributes().bucket;

        if (bucket != RenderBucket::Opaque
            && bucket != RenderBucket::Lightmapped
            && bucket != RenderBucket::Translucent)
        {
            continue;
        }

        uint64 newKey;
        uint64 oldKey;
        GpuBlas* blas;

        g_renderInterface->blasCache->GetOrCreateBLAS(
            entity, meshProxy->mesh, meshProxy->material,
            newKey, oldKey,
            blas);

        if (!blas)
        {
            HYP_LOG(Rendering, Error, "Failed to build BLAS for Mesh {}", meshProxy->mesh->GetName());
            continue;
        }

        if (oldKey != 0)
        {
            for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
            {
                pd->rayTracingTlases[frameIndex]->RemoveGpuBlas(oldKey);
            }
        }

        if (!blas->IsCreated())
        {
            blas->SetTransform(meshProxy->bufferData.modelMatrix);

            const uint32 materialBinding = Resources::GetBinding(meshProxy->material);
            blas->SetMaterialBinding(materialBinding);

            CheckResult(blas->Create());
        }
        else
        {
            const uint32 materialBinding = Resources::GetBinding(meshProxy->material);

            blas->SetMaterialBinding(materialBinding);
            blas->SetTransform(meshProxy->bufferData.modelMatrix);
        }

        if (!pd->rayTracingTlases[currentFrameIndex]->HasGpuBlas(newKey))
        {
            for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
            {
                pd->rayTracingTlases[frameIndex]->AddGpuBlas(newKey, blas);
            }

            hasBlas = true;
        }
    }

    if (!pd->rayTracingTlases[currentFrameIndex]->IsCreated())
    {
        if (hasBlas)
        {
            for (GpuTlasRef& tlas : pd->rayTracingTlases)
            {
                CheckResult(tlas->Create());
            }
        }

        return;
    }

    RTUpdateStateFlags updateStateFlags;
    pd->rayTracingTlases[currentFrameIndex]->UpdateStructure(updateStateFlags);
}

void DeferredRenderer::PerformOcclusionCulling(Frame* frame, const RenderSetup& rs, RenderCollector& renderCollector)
{
    HYP_SCOPE;

    renderCollector.PerformOcclusionCulling(frame, rs, AllRenderBucketsMask);
}

void DeferredRenderer::ExecuteDrawCalls(
    Frame* frame,
    const RenderSetup& rs,
    RenderCollector& renderCollector,
    uint32 bucketMask)
{
    HYP_SCOPE;

    renderCollector.ExecuteDrawCalls(frame, rs, bucketMask);
}

void DeferredRenderer::GenerateMipChain(Frame* frame, const RenderSetup& rs, RenderCollector& renderCollector, const GpuImageRef& srcImage)
{
    HYP_SCOPE;

    const uint32 frameIndex = frame->GetFrameIndex();

    DeferredRendererPassData* pd = ObjCast<DeferredRendererPassData>(rs.passData);

    const GpuImageRef& mipmappedResult = pd->mipChain->GetGpuImage();
    Assert(mipmappedResult.IsValid());

    frame->cr << InsertBarrier(srcImage, RS_COPY_SRC);
    frame->cr << InsertBarrier(mipmappedResult, RS_COPY_DST);

    // Blit into the mipmap chain img
    frame->cr << BlitRect(
        srcImage,
        mipmappedResult,
        Rect<uint32> { 0, 0, srcImage->GetExtent().x, srcImage->GetExtent().y },
        Rect<uint32> { 0, 0, mipmappedResult->GetExtent().x, mipmappedResult->GetExtent().y });

    frame->cr << GenerateMipmaps(mipmappedResult);

    frame->cr << InsertBarrier(srcImage, RS_SHADER_RESOURCE);
}

#pragma endregion DeferredRenderer

} // namespace Hyperion
