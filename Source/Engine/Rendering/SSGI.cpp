/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/SSGI.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/RenderInterface.hpp>
#include <Rendering/GBuffer.hpp>
#include <Rendering/CommandRecorder.hpp>
#include <Rendering/Frame.hpp>
#include <Rendering/DescriptorSet.hpp>
#include <Rendering/ComputePipeline.hpp>
#include <Rendering/RendererMain.hpp>
#include <Rendering/RenderProxyList.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/ShaderInstance.hpp>
#include <Rendering/TextureViewCache.hpp>
#include <Rendering/RenderHelpers.hpp>
#include <Rendering/CBufferAllocator.hpp>
#include <Rendering/Texture.hpp>

#include <Asset/AssetRegistry.hpp>

#include <Rendering/Shadows/ShadowMapCache.hpp>

#include <Rendering/Passes/DeferredPass.hpp>

#include <Framework/CVarManager.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Core/Utilities/DeferredScope.hpp>

#include <Core/Threading/Threads.hpp>

#include <Scene/EnvProbe.hpp>
#include <Scene/Light.hpp>
#include <Scene/View.hpp>

namespace Hyperion {

static constexpr bool SSGIUseTemporalBlending = true;
static constexpr TextureFormat SSGIFormat = TextureFormat::RGBA8;
static constexpr uint32 SSGIMaxLights = 4;
static constexpr uint32 SSGIMaxEnvProbes = 4;
static constexpr uint32 SSGINumSamples = 32; // temporal sample count

CVar<float> g_cvSSGIDepthThreshold { "Rendering.SSGI.DepthThreshold", 0.2f };
CVar<float> g_cvSSGINormalPower { "Rendering.SSGI.NormalPower", 8.0f };
CVar<float> g_cvSSGIRayStep { "Rendering.SSGI.RayStep", 1.5f };
CVar<float> g_cvSSGIDistanceBias { "Rendering.SSGI.DistanceBias", 0.009f };
CVar<uint32> g_cvSSGIMaxIterations { "Rendering.SSGI.MaxIterations", 16 };

namespace DeferredRendererHelpers {

// Defined in DeferredPass.cpp
void FillShadowMapData(
    ShadowMapData& outShadowMapData,
    const ShadowMap& inShadowMap,
    uint32 cascadeIndex,
    View* shadowMapViewDynamic,
    View* shadowMapViewStatic);

} // namespace DeferredRendererHelpers

namespace {

static ShaderPropertySet GetShaderProperties()
{
    ShaderPropertySet shaderProperties;

    switch (SSGIFormat)
    {
    case TextureFormat::RGBA8:
        shaderProperties.Add(InternShaderProperty(ShaderProperty(NAME("OUTPUT"), NAME("RGBA8"))));
        break;
    case TextureFormat::RGBA16F:
        shaderProperties.Add(InternShaderProperty(ShaderProperty(NAME("OUTPUT"), NAME("RGBA16F"))));
        break;
    case TextureFormat::RGBA32F:
        shaderProperties.Add(InternShaderProperty(ShaderProperty(NAME("OUTPUT"), NAME("RGBA32F"))));
        break;
    default:
        HYP_FAIL("Invalid SSGI format type");
    }

    return shaderProperties;
}
} // namespace

struct SSGIConstants
{
    Vec2u dimensions;
    float rayStep;
    float maxIterations;

    float distanceBias;
    uint32 numSamples;
    uint32 numBoundLights;
    uint32 numBoundEnvProbes;
};

#pragma region SSGI

SSGI::SSGI(GBuffer* gbuffer)
    : FullScreenPass(SSGIFormat, gbuffer, FSP_EXTERNAL_RENDERTARGET)
{
    SetPassName(NAME("SSGI"));
}

SSGI::~SSGI()
{
    if (m_temporalBlending)
    {
        m_temporalBlending.Reset();
    }
}

void SSGI::Create()
{
    Assert(m_gbuffer != nullptr);

    m_extent = MathUtil::Max(m_gbuffer->GetExtent() * 0.25f, Vec2u::One());

    m_ssgiTexture = MakeHandle<Texture>(TextureDesc {
        TextureType::Texture2D,
        SSGIFormat,
        Vec3u(m_extent, 1),
        TFM_LINEAR,
        TFM_LINEAR,
        TWM_CLAMP_TO_EDGE,
        1,
        IU_STORAGE | IU_SAMPLED });
    m_ssgiTexture->SetIsTransient(true);
    m_ssgiTexture->SetName(NAME("SSGITexture"));
    Check(m_ssgiTexture->Create());

    GetEngineAssetRegistry()->PutAsset(m_ssgiTexture);

    for (uint32 i = 0; i < NumDownsamplePasses; i++)
    {
        m_downsampleTextures[i] = MakeHandle<Texture>(TextureDesc {
            TextureType::Texture2D,
            SSGIFormat,
            Vec3u(MathUtil::Max(m_extent / (2 * (i + 1)), Vec2u::One()), 1),
            TFM_LINEAR,
            TFM_LINEAR,
            TWM_CLAMP_TO_EDGE,
            1,
            IU_SAMPLED });
        m_downsampleTextures[i]->SetName(NAME_FMT("SSGIDownsampleTexture{}", i));
        Check(m_downsampleTextures[i]->Create());
    }

    for (uint32 i = 0; i < NumDownsamplePasses; i++)
    {
        // scaling back up
        const Vec2u targetExtent = i == NumDownsamplePasses - 1
            ? m_extent
            : m_extent / (2 * (NumDownsamplePasses - i - 1));

        m_upsamplePasses[i] = MakeUnique<FullScreenPass>(SSGIFormat, MathUtil::Max(targetExtent, Vec2u::One()), nullptr, FSP_NONE);
        m_upsamplePasses[i]->SetShaderDesc(ShaderDesc(NAME("Upsample"), ShaderPropertySet {}));
        m_upsamplePasses[i]->Create();
    }

    if (SSGIUseTemporalBlending)
    {
        m_temporalBlending = MakeUnique<TemporalBlending>(
            m_extent,
            SSGIFormat,
            TemporalBlendTechnique::TECHNIQUE_1,
            0.9,
            m_upsamplePasses[NumDownsamplePasses - 1]->GetAttachment(0)->GetImageView(),
            m_gbuffer);

        m_temporalBlending->Create();
    }
}

const Handle<Texture>& SSGI::GetFinalResultTexture() const
{
    return m_temporalBlending
        ? m_temporalBlending->GetResultTexture()
        : m_ssgiTexture;
}

void SSGI::Render(Frame* frame, const RenderSetup& renderSetup)
{
    HYP_NAMED_SCOPE("Screen Space Global Illumination");

    AssertDebug(renderSetup.world && renderSetup.view);
    AssertDebug(renderSetup.passData != nullptr);

    DeferredPassData* dpd = DynamicCast<DeferredPassData>(renderSetup.passData);
    AssertDebug(dpd != nullptr);

    const FramebufferRef& inputsFramebuffer = dpd->view.GetUnsafe()->GetOutputTarget().GetFramebuffer(GBufferPass::Opaque);

    RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(GetRenderProxy(renderSetup.view->GetCamera()));
    Assert(cameraProxy != nullptr);

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferSize = 0;
    size_t cbufferOffset = 0;

    /// Used loosely as a source around down/upsampling for SSGI
    /// https://gamehacker1999.github.io/posts/SSGI/

    CommandRecorder& cr = frame->cr;

    {     // Compute pass
        { // Update constant buffer
            SSGIConstants ssgiConstants {};
            ssgiConstants.dimensions = m_extent;
            ssgiConstants.rayStep = g_cvSSGIRayStep.Get();
            ssgiConstants.maxIterations = g_cvSSGIMaxIterations.Get();
            ssgiConstants.distanceBias = g_cvSSGIDistanceBias.Get();
            ssgiConstants.numSamples = SSGINumSamples;

            Array<Pair<Light*, LightShaderData*>, RenderAllocator> tempLights;
            Array<Pair<EnvProbe*, EnvProbeShaderData*>, RenderAllocator> tempEnvProbes;

            uint32& numBoundLights = ssgiConstants.numBoundLights;
            uint32& numBoundEnvProbes = ssgiConstants.numBoundEnvProbes;

            RenderProxyList& rpl = GetConsumerProxyList(renderSetup.view);
            rpl.BeginRead();
            HYP_DEFER({ rpl.EndRead(); });

            for (Light* light : rpl.GetLights())
            {
                const LightType lightType = light->GetLightType();

                if (lightType != LightType::Directional && lightType != LightType::Point)
                {
                    continue;
                }

                if (numBoundLights >= SSGIMaxLights)
                {
                    break;
                }

                RenderProxyLight* lightProxy = static_cast<RenderProxyLight*>(GetRenderProxy(light));
                Assert(lightProxy != nullptr);

                tempLights.EmplaceBack(light, &lightProxy->bufferData);

                ++numBoundLights;
            }

            for (EnvProbe* envProbe : rpl.GetEnvProbes())
            {
                if (envProbe->IsA(ReflectionProbe::StaticClass()) || envProbe->IsA(SkyProbe::StaticClass()))
                {
                    if (numBoundEnvProbes >= SSGIMaxEnvProbes)
                    {
                        break;
                    }

                    RenderProxyEnvProbe* envProbeProxy = static_cast<RenderProxyEnvProbe*>(GetRenderProxy(envProbe));
                    Assert(envProbeProxy != nullptr);

                    tempEnvProbes.EmplaceBack(envProbe, &envProbeProxy->bufferData);

                    ++numBoundEnvProbes;
                }
            }

            RI.cbufferAllocator->Write(&ssgiConstants);

            for (uint32 i = 0; i < SSGIMaxLights; i++)
            {
                if (i < uint32(tempLights.Size()))
                {
                    RI.cbufferAllocator->Write(tempLights[i].second);
                    continue;
                }

                LightShaderData dummy {};
                RI.cbufferAllocator->Write(&dummy);
            }

            for (uint32 i = 0; i < SSGIMaxLights; i++)
            {
                ShadowMapData shadowMapData {};

                if (i < uint32(tempLights.Size()))
                {
                    View* shadowMapViewDynamic;
                    View* shadowMapViewStatic;

                    Light* light = tempLights[i].first;

                    const uint32 cascadeIndex = 0;

                    ShadowMap* shadowMap = RI.shadowMapCache->GetShadowMap(
                        light,
                        renderSetup.view,
                        cascadeIndex,
                        shadowMapViewDynamic,
                        shadowMapViewStatic);

                    if (shadowMap != nullptr)
                    {
                        DeferredRendererHelpers::FillShadowMapData(
                            shadowMapData,
                            *shadowMap,
                            cascadeIndex,
                            shadowMapViewDynamic,
                            shadowMapViewStatic);
                    }
                }

                RI.cbufferAllocator->Write(&shadowMapData);
            }

            // sort probes
            // we want to draw reflection probes first, sky should be the very last
            // within the reflection probes subgroup, we want to sort based on distance from the
            // camera to ensure we sample the closest probes first in the shader
            const Vec4f& cameraPosition = cameraProxy->bufferData.cameraPosition;

            std::sort(tempEnvProbes.Begin(), tempEnvProbes.End(),
                      [cam = cameraPosition.GetXYZ()](const Pair<EnvProbe*, EnvProbeShaderData*>& a, const Pair<EnvProbe*, EnvProbeShaderData*>& b)
                      {
                        const bool aIsSky = a.first->IsA(SkyProbe::StaticClass());
                        const bool bIsSky = b.first->IsA(SkyProbe::StaticClass());

                        if (aIsSky ^ bIsSky)
                        {
                            return !aIsSky;
                        }
                        
                        const Vec3f aProbePosition = a.second->worldPosition.GetXYZ();
                        const Vec3f bProbePosition = b.second->worldPosition.GetXYZ();

                        const float aDistSq = (aProbePosition - cam).LengthSquared();
                        const float bDistSq = (bProbePosition - cam).LengthSquared();

                        // This is to satisfy strict weak ordering requirements.
                        if (aDistSq == bDistSq)
                        {
                            return a.first->Id() < b.first->Id();
                        }

                        return aDistSq < bDistSq;
                      });

            for (uint32 i = 0; i < SSGIMaxEnvProbes; i++)
            {
                const EnvProbeShaderData* pEnvProbeShaderData = nullptr;

                if (i < uint32(tempEnvProbes.Size()))
                {
                    pEnvProbeShaderData = tempEnvProbes[i].second;
                }
                else
                {
                    static const EnvProbeShaderData s_dummyEnvProbeShaderData {};
                    pEnvProbeShaderData = &s_dummyEnvProbeShaderData;
                }

                RI.cbufferAllocator->Write(pEnvProbeShaderData);
            }

            RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);
        }

        const uint32 totalPixelsInImage = m_extent.Volume();
        const uint32 numDispatchCalls = (totalPixelsInImage + 255) / 256;

        // put sample image in writeable state
        cr << InsertBarrier(m_ssgiTexture->GetGpuImage(), RS_UNORDERED_ACCESS);

        cr << SetCurrentShader(ShaderDesc(NAME("SSGI"), GetShaderProperties()));

        uint32 numShaderUniforms = 0;

        cr << SetShaderUniform(numShaderUniforms++, "OutImage"_sh, RI.textureViewCache->GetOrCreate(m_ssgiTexture));
        cr << SetShaderUniform(numShaderUniforms++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

        // GBuffer textures
        cr << SetShaderUniform(numShaderUniforms++, "GBufferAlbedoTexture"_sh, inputsFramebuffer->GetAttachment(GBufferTarget::Color)->GetImageView());
        cr << SetShaderUniform(numShaderUniforms++, "GBufferNormalsTexture"_sh, inputsFramebuffer->GetAttachment(GBufferTarget::Normals)->GetImageView());
        cr << SetShaderUniform(numShaderUniforms++, "GBufferMaterialTexture"_sh, inputsFramebuffer->GetAttachment(GBufferTarget::MatData)->GetImageView());
        cr << SetShaderUniform(numShaderUniforms++, "GBufferDepthTexture"_sh, inputsFramebuffer->GetAttachment(GBufferTarget::Depth)->GetImageView());

        cr << SetShaderUniform(numShaderUniforms++, "DeferredShadingTexture"_sh, dpd->lightingFramebuffer->GetAttachment(0)->GetImageView());

        cr << SetShaderUniform(numShaderUniforms++, "BlueNoiseBuffer"_sh, RI.blueNoiseBuffer);

        // Samplers
        cr << SetShaderUniform(numShaderUniforms++, "SamplerLinear"_sh, RI.placeholderData->GetSamplerLinear());
        cr << SetShaderUniform(numShaderUniforms++, "SamplerNearest"_sh, RI.placeholderData->GetSamplerNearest());

        // World and camera buffers
        cr << SetShaderUniform(numShaderUniforms++, "WorldsBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);
        cr << SetShaderUniform(numShaderUniforms++, "CamerasBuffer"_sh, RI.namedBuffers[NamedBuffer::Cameras], Resources::GetBinding(renderSetup.view->GetCamera()));

        // Shadow maps
        cr << SetShaderUniform(numShaderUniforms++, "ShadowMapsTextureArray"_sh, RI.shadowMapCache->GetAtlasImageView());
        cr << SetShaderUniform(numShaderUniforms++, "PointLightShadowMapsTextureArray"_sh, RI.shadowMapCache->GetPointLightShadowMapImageView());

        // Env probes
        cr << SetShaderUniform(numShaderUniforms++, "EnvProbesColorTexture"_sh, RI.textureViewCache->GetOrCreate(RI.envProbesColorTexture));
        cr << SetShaderUniform(numShaderUniforms++, "EnvProbesDepthTexture"_sh, RI.textureViewCache->GetOrCreate(RI.envProbesDepthTexture));

        cr << SetShaderUniform(numShaderUniforms++, "EnvProbesBuffer"_sh, RI.namedBuffers[NamedBuffer::EnvProbes]);

        cr << DispatchCompute(Vec3u { numDispatchCalls, 1, 1 });
    }

    for (uint32 i = 0; i < NumDownsamplePasses; i++)
    {
        if (i == 0)
        {
            cr << InsertBarrier(m_ssgiTexture->GetGpuImage(), RS_COPY_SRC);
            cr << InsertBarrier(m_downsampleTextures[i]->GetGpuImage(), RS_COPY_DST);

            cr << Blit(m_ssgiTexture, m_downsampleTextures[i]);
        }
        else
        {
            cr << InsertBarrier(m_downsampleTextures[i - 1]->GetGpuImage(), RS_COPY_SRC);
            cr << InsertBarrier(m_downsampleTextures[i]->GetGpuImage(), RS_COPY_DST);

            cr << Blit(m_downsampleTextures[i - 1], m_downsampleTextures[i]);
        }
    }

    // Upsample + blur
    for (uint32 i = 0; i < NumDownsamplePasses; i++)
    {
        FullScreenPass* pass = m_upsamplePasses[i].Get();

        const Vec2f sourceResolution = MathUtil::Max(Vec2f(pass->GetExtent()) / 2, Vec2f::One());

        // Need new cbuffer
        cbuffer = nullptr;
        cbufferSize = 0;
        cbufferOffset = 0;

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
            upsampleConstants.depthThreshold = g_cvSSGIDepthThreshold.Get();
            upsampleConstants.normalThreshold = g_cvSSGINormalPower.Get();

            RI.cbufferAllocator->Write(&upsampleConstants);
            RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);
        }

        pass->Begin(frame, renderSetup);

        uint32 numShaderUniforms = 0;

        // Samplers
        cr << SetShaderUniform(numShaderUniforms++, "SamplerLinear"_sh, RI.placeholderData->GetSamplerLinear());
        cr << SetShaderUniform(numShaderUniforms++, "SamplerNearest"_sh, RI.placeholderData->GetSamplerNearest());

        // GBuffer textures
        cr << SetShaderUniform(numShaderUniforms++, "NormalsTexture"_sh, inputsFramebuffer->GetAttachment(GBufferTarget::Normals)->GetImageView());
        cr << SetShaderUniform(numShaderUniforms++, "DepthTexture"_sh, inputsFramebuffer->GetAttachment(GBufferTarget::Depth)->GetImageView());

        cr << SetShaderUniform(numShaderUniforms++, "PrevPassTexture"_sh,
                               i == 0 ? RI.textureViewCache->GetOrCreate(m_downsampleTextures[NumDownsamplePasses - 1])
                                      : m_upsamplePasses[i - 1]->GetAttachment(0)->GetImageView());

        cr << SetShaderUniform(numShaderUniforms++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

        // Draw quad
        pass->RenderFullScreenQuad(frame, renderSetup);

        pass->End(frame, renderSetup);

        if (i == 0)
        {
            cr << InsertBarrier(pass->GetAttachment(0)->GetGpuImage(), RS_SHADER_RESOURCE);
        }
    }

    // transition sample image back into read state
    cr << InsertBarrier(m_upsamplePasses[NumDownsamplePasses - 1]->GetAttachment(0)->GetGpuImage(), RS_SHADER_RESOURCE);

    if (SSGIUseTemporalBlending && m_temporalBlending != nullptr)
    {
        m_temporalBlending->Render(frame, renderSetup);
    }
}

#pragma endregion SSGI

} // namespace Hyperion
