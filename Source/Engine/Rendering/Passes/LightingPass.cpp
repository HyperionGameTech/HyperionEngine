/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Passes/LightingPass.hpp>
#include <Rendering/Passes/DeferredPass.hpp>
#include <Rendering/Passes/HBAOPass.hpp>
#include <Rendering/Passes/ReflectionsPass.hpp>
#include <Rendering/Passes/SSRPass.hpp>
#include <Rendering/Passes/DeferredPassShared.hpp>

#include <Rendering/MaterialTextureCache.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/GBuffer.hpp>
#include <Rendering/RenderInterface.hpp>
#include <Rendering/Material.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/RenderTypes.hpp>
#include <Rendering/GpuBuffer.hpp>
#include <Rendering/RenderProxyList.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/TextureViewCache.hpp>
#include <Rendering/SamplerCache.hpp>
#include <Rendering/DDGI.hpp>
#include <Rendering/SSGI.hpp>
#include <Rendering/StencilMasks.hpp>
#include <Rendering/RayTracingReflections.hpp>
#include <Rendering/CBufferAllocator.hpp>
#include <Rendering/RawBufferAllocator.hpp>

#include <Rendering/Shadows/ShadowMapAllocator.hpp>
#include <Rendering/Shadows/ShadowMapCache.hpp>
#include <Rendering/Shadows/ShadowMap.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Scene/View.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Utilities/DeferredScope.hpp>
#include <Core/Utilities/Float16.hpp>

#include <Framework/CVarManager.hpp>

namespace Hyperion {

static const Float16 s_ltcMatrix[] = {
#include <Rendering/Inl/LTCMatrix.inl>
};

static_assert(sizeof(s_ltcMatrix) == 64 * 64 * 4 * 2, "Invalid LTC matrix size");

static const Float16 s_ltcBrdf[] = {
#include <Rendering/Inl/LTCBRDF.inl>
};

static_assert(sizeof(s_ltcBrdf) == 64 * 64 * 4 * 2, "Invalid LTC BRDF size");

// Maps individual light types to per-light specific properties.
static const FixedArray<ShaderPropertySet, NumLightTypes> s_deferredLightTypeProperties {
    ShaderPropertySet { { InternShaderProperty(ShaderProperty(NAME("LIGHT_TYPE"), NAME("DIRECTIONAL"))) } },
    ShaderPropertySet { { InternShaderProperty(ShaderProperty(NAME("LIGHT_TYPE"), NAME("POINT"))) } },
    ShaderPropertySet { { InternShaderProperty(ShaderProperty(NAME("LIGHT_TYPE"), NAME("SPOT"))) } },
    ShaderPropertySet { { InternShaderProperty(ShaderProperty(NAME("LIGHT_TYPE"), NAME("AREA_RECT"))) } }
};

static const ShaderPropertyId s_propHBAOEnabled = InternShaderProperty(ShaderProperty(NAME("HBAO_ENABLED")));
static const ShaderPropertyId s_propSSGIEnabled = InternShaderProperty(ShaderProperty(NAME("SSGI_ENABLED")));
static const ShaderPropertyId s_propSSREnabled = InternShaderProperty(ShaderProperty(NAME("SSR_ENABLED")));

static const ShaderPropertyId s_propRayTracingReflections = InternShaderProperty(ShaderProperty(NAME("RT_REFLECTIONS")));
static const ShaderPropertyId s_propRayTracingGlobalIllumination = InternShaderProperty(ShaderProperty(NAME("RT_GI")));
static const ShaderPropertyId s_propPathTracer = InternShaderProperty(ShaderProperty(NAME("PATHTRACER")));

static const ShaderPropertyId s_propDebugReflections = InternShaderProperty(ShaderProperty(NAME("DEBUG_REFLECTIONS")));
static const ShaderPropertyId s_propDebugIrradiance = InternShaderProperty(ShaderProperty(NAME("DEBUG_IRRADIANCE")));
static const ShaderPropertyId s_propDebugAO = InternShaderProperty(ShaderProperty(NAME("DEBUG_AO")));
static const ShaderPropertyId s_propDebugNormals = InternShaderProperty(ShaderProperty(NAME("DEBUG_NORMALS")));
static const ShaderPropertyId s_propDebugVelocity = InternShaderProperty(ShaderProperty(NAME("DEBUG_VELOCITY")));

static const ShaderPropertyId s_propLightTypeClustered = InternShaderProperty(ShaderProperty(NAME("LIGHT_TYPE"), NAME("CLUSTERED")));

extern CVar<bool> g_cvHBAO;
extern CVar<bool> g_cvSSGI;
extern CVar<bool> g_cvSSR;
extern CVar<bool> g_cvClusteredShading;
extern CVar<int> g_cvDeferredDebugVis;
extern CVar<bool> g_cvRayTracedReflections;
extern CVar<bool> g_cvDDGI;
extern CVar<bool> g_cvPathTracing;

void MergeGlobalShaderProperties(ShaderPropertySet& out);

namespace DeferredRendererHelpers {

void GetDeferredShaderProperties(
    DeferredPassMode mode,
    ShaderPropertySet& outShaderProperties,
    const RenderProxyList* rpl,
    LightType lightType,
    bool clustered)
{
    static const IRenderConfig& s_renderConfig = RI.GetRenderConfig();

    MergeGlobalShaderProperties(outShaderProperties);

    if (g_cvHBAO.Get())
    {
        outShaderProperties.Add(s_propHBAOEnabled);
    }

    if (mode == DPM_INDIRECT_LIGHTING)
    {
        outShaderProperties.Set(s_propRayTracingReflections, s_renderConfig.rayTracing && g_cvRayTracedReflections.Get());

        if (s_renderConfig.rayTracing && g_cvDDGI.Get())
        {
            outShaderProperties.Add(s_propRayTracingGlobalIllumination);
        }

        outShaderProperties.Set(s_propSSGIEnabled, g_cvSSGI.Get());
        outShaderProperties.Set(s_propSSREnabled, g_cvSSR.Get());
    }
    else
    {
        if (clustered)
        {
            outShaderProperties.Add(s_propLightTypeClustered);
        }
    }

    if (s_renderConfig.rayTracing && g_cvPathTracing.Get())
    {
        outShaderProperties.Add(s_propPathTracer);
    }
    else
    {
        static constexpr const ShaderPropertyId* const DebugShaderProperties[] = {
            nullptr,
            &s_propDebugReflections,
            &s_propDebugIrradiance,
            &s_propDebugAO,
            &s_propDebugNormals,
            &s_propDebugVelocity
        };
        
        const int debugMode = g_cvDeferredDebugVis.Get();
        const ShaderPropertyId* shaderProperty = debugMode < std::size(DebugShaderProperties)
            ? DebugShaderProperties[debugMode]
            : nullptr;
        
        if (shaderProperty != nullptr)
        {
            outShaderProperties.Add(*shaderProperty);
        }
    }

    if (!clustered && lightType != InvalidLightType)
    {
        outShaderProperties = outShaderProperties | s_deferredLightTypeProperties[uint32(lightType)];
    }
}

} // namespace DeferredRendererHelpers

#pragma region LightingPass

LightingPass::LightingPass(DeferredPassMode mode, Vec2u extent, GBuffer* gbuffer, const FramebufferRef& framebuffer)
    : FullScreenPass(ShaderDesc(), framebuffer, TextureFormat::RGBA16F, extent, gbuffer, FSP_EXTERNAL_RENDERTARGET),
      m_mode(mode),
      m_ltcSampler(nullptr)
{
    SetPassName(NAME("Deferred"));
    Assert(m_framebuffer.IsValid());

    SetBlendFunction(BlendFunction(BlendModeFactor::One, BlendModeFactor::One, BlendModeFactor::One, BlendModeFactor::One));
}

LightingPass::~LightingPass()
{
    m_ltcSampler = nullptr;
}

void LightingPass::Create()
{
    AssertOnThread(g_renderThread);

    FullScreenPass::Create();

    // linear transform cosines texture data
    if (m_mode == DPM_DIRECT_LIGHTING && !m_ltcSampler)
    {
        m_ltcSampler = RI.samplerCache->GetOrCreate(SamplerDesc { TFM_NEAREST, TFM_LINEAR, TWM_CLAMP_TO_EDGE });

        ByteBuffer ltcMatrixData(sizeof(s_ltcMatrix), s_ltcMatrix);

        m_ltcMatrixTexture = MakeHandle<Texture>(
            TextureDesc {
                TextureType::Texture2D,
                TextureFormat::RGBA16F,
                Vec3u { 64, 64, 1 },
                TFM_LINEAR,
                TFM_LINEAR,
                TWM_CLAMP_TO_EDGE },
            ltcMatrixData.ToByteView());

        m_ltcMatrixTexture->SetName(NAME("LTC_Matrix"));
        Check(m_ltcMatrixTexture->Create());

        ByteBuffer ltcBrdfData(sizeof(s_ltcBrdf), s_ltcBrdf);

        m_ltcBrdfTexture = MakeHandle<Texture>(
            TextureDesc {
                TextureType::Texture2D,
                TextureFormat::RGBA16F,
                Vec3u { 64, 64, 1 },
                TFM_LINEAR,
                TFM_LINEAR,
                TWM_CLAMP_TO_EDGE },
            ltcBrdfData.ToByteView());

        m_ltcBrdfTexture->SetName(NAME("LTC_BRDF"));
        Check(m_ltcBrdfTexture->Create());
    }
}

void LightingPass::Resize_Internal(Vec2u newSize)
{
    FullScreenPass::Resize_Internal(newSize);
}

void LightingPass::RenderToFramebuffer_Internal(Frame* frame, const RenderSetup& rs, Framebuffer* framebuffer)
{
    AssertDebug(rs.world && rs.view);
    AssertDebug(rs.passData != nullptr);

    const uint32 frameIndex = frame->GetFrameIndex();

    RenderProxyList& rpl = GetConsumerProxyList(rs.view);
    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(GetRenderProxy(rs.view->GetCamera()));
    if (!cameraProxy)
    {
        return;
    }

    DeferredPassData* dpd = DynamicCast<DeferredPassData>(rs.passData);
    AssertDebug(dpd != nullptr);

    Framebuffer* opaquePassFramebuffer = dpd->view.GetUnsafe()->GetOutputTarget().GetFramebuffer(GBufferPass::Opaque);

    CommandRecorder& cr = frame->cr;

    cr << SetCurrentViewport(rs.viewport);

    cr << SetInputLayout(StaticVertexInputLayout<VT_Simple>);
    cr << SetTopology(TOP_TRIANGLES);
    cr << SetFillMode(FM_FILL);
    cr << SetCurrentBlendFunction(m_blendFunction);
    cr << SetDepthWrite(false);
    cr << SetDepthTest(false);

    static constexpr uint8 StencilFilterMask = (0xFF & ~SkyStencilMask);

    //cr << SetStencilTest(true);
    //cr << SetStencilFunction(StencilFunction { SO_KEEP, SO_KEEP, SO_KEEP, SCO_EQUAL });
    //cr << SetStencilState(0, StencilFilterMask, 0x0);

    HYP_DEFER({
        // reset states
        cr << SetCurrentBlendFunction(BlendFunction::None());
        cr << SetStencilState(0, 0xFF, 0x0);
        cr << SetDepthWrite(true);
        cr << SetDepthTest(true);
        //cr << SetStencilTest(false);
    });

    uint32 numShaderUniforms = 0;

    Sampler* shadowSampler = RI.samplerCache->GetOrCreate(SamplerDesc { TFM_LINEAR, TFM_LINEAR, TWM_CLAMP_TO_EDGE, SamplerCompareOp::LessEq });

    cr << SetShaderUniform(numShaderUniforms++, "SamplerLinear"_sh, RI.placeholderData->GetSamplerLinearMipmap());
    cr << SetShaderUniform(numShaderUniforms++, "SamplerNearest"_sh, RI.placeholderData->GetSamplerNearest());
    cr << SetShaderUniform(numShaderUniforms++, "SamplerShadow"_sh, shadowSampler);

    cr << SetShaderUniform(numShaderUniforms++, "WorldsBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);

    cr << SetShaderUniform(numShaderUniforms++, "LightsBuffer"_sh, RI.namedBuffers[NamedBuffer::Lights]);
    cr << SetShaderUniform(numShaderUniforms++, "EnvProbesBuffer"_sh, RI.namedBuffers[NamedBuffer::EnvProbes]);

    // use the same index for the CBuffer uniform across shaders
    const uint32 cbufferUniformIndex = numShaderUniforms++;

    cr << SetShaderUniform(numShaderUniforms++, "ShadowMapsTextureArray"_sh, RI.shadowMapCache->GetAtlasImageView());
    cr << SetShaderUniform(numShaderUniforms++, "PointLightShadowMapsTextureArray"_sh, RI.shadowMapCache->GetPointLightShadowMapImageView());

    cr << SetShaderUniform(numShaderUniforms++, "EnvProbesColorTexture"_sh, RI.textureViewCache->GetOrCreate(RI.envProbesColorTexture));
    cr << SetShaderUniform(numShaderUniforms++, "EnvProbesDepthTexture"_sh, RI.textureViewCache->GetOrCreate(RI.envProbesDepthTexture));

    for (uint32 attachmentIndex = 0; attachmentIndex < NumGBufferTargets; attachmentIndex++)
    {
        cr << SetShaderUniform(numShaderUniforms++, GBufferTextureNames[attachmentIndex], opaquePassFramebuffer->GetAttachment(attachmentIndex)->GetImageView());
    }

    cr << SetShaderUniform(numShaderUniforms++, "GBufferMipChain"_sh, RI.textureViewCache->GetOrCreate(dpd->mipChain));

    if (dpd->hbao != nullptr && g_cvHBAO.Get())
    {
        cr << SetShaderUniform(numShaderUniforms++, "SSAOResultTexture"_sh, dpd->hbao->GetFinalImageView());
    }
    else
    {
        cr << SetShaderUniform(numShaderUniforms++, "SSAOResultTexture"_sh, RI.textureViewCache->GetOrCreate(RI.placeholderData->textureSolidWhite));
    }

    if (dpd->reflectionsPass != nullptr && dpd->reflectionsPass->ssrPass != nullptr && dpd->reflectionsPass->ssrPass->IsRendered())
        cr << SetShaderUniform(numShaderUniforms++, "SSRResultTexture"_sh, RI.textureViewCache->GetOrCreate(dpd->reflectionsPass->ssrPass->GetFinalResultTexture()));
    else
        cr << SetShaderUniform(numShaderUniforms++, "SSRResultTexture"_sh, RI.textureViewCache->GetOrCreate(RI.placeholderData->textureSolidBlack));

    const bool useClusteredShading = g_cvClusteredShading.Get();

    if (useClusteredShading || m_mode == DPM_INDIRECT_LIGHTING)
    {
        AssertDebug(dpd->gridTilesBuffer != nullptr && dpd->gridIndexBuffer != nullptr);

        // Indirect pass uses clusters for EnvProbes.
        cr << SetShaderUniform(numShaderUniforms++, "ClusterGridBuffer"_sh, *dpd->gridTilesBuffer);
        cr << SetShaderUniform(numShaderUniforms++, "ClusterIndexBuffer"_sh, *dpd->gridIndexBuffer);
    }

    if (m_mode == DPM_INDIRECT_LIGHTING)
    {
        if (dpd->ssgi != nullptr)
            cr << SetShaderUniform(numShaderUniforms++, "SSGIResultTexture"_sh, RI.textureViewCache->GetOrCreate(dpd->ssgi->GetFinalResultTexture()));

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
            RI.cbufferAllocator->Write(&cameraProxy->bufferData);

            RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

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

    dpd->numClusteredShadowMaps = 0;

    if (useClusteredShading)
    {
        ShaderPropertySet shaderProperties;
        DeferredRendererHelpers::GetDeferredShaderProperties(DPM_DIRECT_LIGHTING, shaderProperties, &rpl, InvalidLightType, /* clustered */ true);

        cr << SetCurrentShader(ShaderDesc(NAME("DeferredDirect"), shaderProperties));

        uint32 localNumShaderUniforms = numShaderUniforms;

        // Write out all ShadowMaps for the View, indexed by light idx (GetBinding())
        { // Build constants
            GpuBuffer* cbuffer = nullptr;
            size_t cbufferOffset = 0;
            size_t cbufferSize = 0;

            // write camera
            RI.cbufferAllocator->Write(&cameraProxy->bufferData);

            uint32 maxLightBinding = 1;

            Array<Pair<Light*, uint32>, RenderTempAllocator> shadowCasterLightsInView;
            shadowCasterLightsInView.Reserve(MaxClusteredShadowMaps);

            Array<ShadowMapData, RenderTempAllocator> tempShadowMapData;
            tempShadowMapData.Resize(MaxClusteredShadowMaps);

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

            ByteAddressBuffer& shadowMapIndexBuffer = RI.bufferAllocator->AcquireByteAddressBuffer(maxLightBinding * sizeof(uint32));

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

                shadowMapIndexBuffer.Write(lightBinding * sizeof(uint32), sizeof(uint32), &shadowMapIndex);

                const uint32 cascadeIndex = 0;

                View* shadowMapViewDynamic;
                View* shadowMapViewStatic;

                ShadowMap* shadowMap = RI.shadowMapCache->GetShadowMap(
                    light,
                    rs.view,
                    cascadeIndex,
                    shadowMapViewDynamic,
                    shadowMapViewStatic);

                if (shadowMap != nullptr)
                {
                    ShadowMapData& currShadowMapData = tempShadowMapData[shadowMapIndex];

                    DeferredRendererHelpers::FillShadowMapData(
                        currShadowMapData,
                        *shadowMap,
                        cascadeIndex,
                        shadowMapViewDynamic,
                        shadowMapViewStatic);
                }

                ++shadowMapIndex;
            }

            shadowMapIndexBuffer.FlushBatched();

            dpd->clusteredShadowMapIndexBuffer = &shadowMapIndexBuffer;
            dpd->numClusteredShadowMaps = shadowMapIndex;
            Memory::Copy(dpd->clusteredShadowMaps, tempShadowMapData.Data(), shadowMapIndex * sizeof(ShadowMapData));

            cr << SetShaderUniform(localNumShaderUniforms++, "ShadowMapIndexBuffer"_sh, shadowMapIndexBuffer);

            RI.cbufferAllocator->Write(tempShadowMapData.Data(), MaxClusteredShadowMaps * sizeof(ShadowMapData), alignof(ShadowMapData));
            RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

            cr << SetShaderUniform(cbufferUniformIndex, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
        }

        cr << CommitDrawState();

        cr << BindVertexBuffer(m_fullScreenQuad->GetVertexBuffer());
        cr << BindIndexBuffer(m_fullScreenQuad->GetIndexBuffer());
        cr << DrawIndexed(6);
    }

    if (dpd->clusteredShadowMapIndexBuffer == nullptr)
    {
        ByteAddressBuffer& dummyIndexBuffer = RI.bufferAllocator->AcquireByteAddressBuffer(sizeof(uint32));
        dpd->clusteredShadowMapIndexBuffer = &dummyIndexBuffer;
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
                RI.cbufferAllocator->Write(&cameraProxy->bufferData);

                // write current light
                RI.cbufferAllocator->Write(&lightProxy->bufferData);

                // Directional light writes out all cascades, other light types write only one.
                uint32 numCascadesToWrite = (lightType == LightType::Directional) ? lightProxy->numCascades : 1;
                numCascadesToWrite = MathUtil::Clamp(numCascadesToWrite, 1u, MaxShadowMapCascades);

                ShadowMap* shadowMaps[MaxShadowMapCascades] {};

                View* shadowMapViewsDynamic[MaxShadowMapCascades] {};
                View* shadowMapViewsStatic[MaxShadowMapCascades] {};

                // Initialize shadowMaps / views
                for (uint32 cascadeIndex = 0; cascadeIndex < numCascadesToWrite; cascadeIndex++)
                {
                    shadowMaps[cascadeIndex] = RI.shadowMapCache->GetShadowMap(
                        light,
                        rs.view,
                        cascadeIndex,
                        shadowMapViewsDynamic[cascadeIndex],
                        shadowMapViewsStatic[cascadeIndex]);
                }

                // CSM: Directional lights use the shared shadow view matrix to perform cascade selection
                // Write it out before writing any per-cascade shadow map data
                if (lightType == LightType::Directional)
                {
                    DirectionalLightCSMData* csmData = RI.cbufferAllocator->Allocate<DirectionalLightCSMData>();
                    Memory::Zero(csmData, sizeof(DirectionalLightCSMData));

                    DeferredRendererHelpers::FillShadowMapDataCSM(
                        csmData,
                        shadowMapViewsDynamic,
                        shadowMaps,
                        numCascadesToWrite);
                }
                else
                {
                    ShadowMapData* shadowMapData = RI.cbufferAllocator->Allocate<ShadowMapData>(numCascadesToWrite);
                    Memory::Zero(shadowMapData, sizeof(ShadowMapData) * numCascadesToWrite);

                    for (uint32 cascadeIndex = 0; cascadeIndex < numCascadesToWrite; cascadeIndex++)
                    {
                        ShadowMapData& currShadowMapData = shadowMapData[cascadeIndex];

                        ShadowMap* shadowMap = shadowMaps[cascadeIndex];

                        if (shadowMap != nullptr)
                        {
                            DeferredRendererHelpers::FillShadowMapData(
                                currShadowMapData,
                                *shadowMap,
                                cascadeIndex,
                                shadowMapViewsDynamic[cascadeIndex],
                                shadowMapViewsStatic[cascadeIndex]);
                        }
                    }
                }

                RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

                cr << SetShaderUniform(cbufferUniformIndex, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
            }

            cr << SetShaderUniform(localNumShaderUniforms++, "CurrentLight"_sh, RI.namedBuffers[NamedBuffer::Lights], Resources::GetBinding(light));

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

                        Span<const GpuImageViewRef> imageViews = RI.materialTextureCache->imageViews.Get(materialBoundIndex);
                        AssertDebug(imageViews.Size() >= materialProxy->boundTextures.Size());

                        cr << SetShaderUniform(localNumShaderUniforms++, "DiffuseMap"_sh, imageViews[materialProxy->boundTextureIndices[0]]);
                    }

                    cr << SetShaderUniform(localNumShaderUniforms++, "CurrentMaterial"_sh, RI.namedBuffers[NamedBuffer::Materials], Resources::GetBinding(lightProxy->lightMaterial));
                }
                else
                {
                    cr << SetShaderUniform(localNumShaderUniforms++, "CurrentMaterial"_sh, RI.namedBuffers[NamedBuffer::Materials], 0);
                }

                cr << SetShaderUniform(localNumShaderUniforms++, "LTCSampler"_sh, m_ltcSampler);

                if (m_ltcMatrixTexture != nullptr)
                    cr << SetShaderUniform(localNumShaderUniforms++, "LTCMatrixTexture"_sh, RI.textureViewCache->GetOrCreate(m_ltcMatrixTexture));

                if (m_ltcBrdfTexture != nullptr)
                    cr << SetShaderUniform(localNumShaderUniforms++, "LTCBRDFTexture"_sh, RI.textureViewCache->GetOrCreate(m_ltcBrdfTexture));
            }

            cr << CommitDrawState();

            cr << BindVertexBuffer(m_fullScreenQuad->GetVertexBuffer());
            cr << BindIndexBuffer(m_fullScreenQuad->GetIndexBuffer());
            cr << DrawIndexed(6);

            prevLightType = lightType;
        }
    }
}

#pragma endregion LightingPass

} // namespace Hyperion
