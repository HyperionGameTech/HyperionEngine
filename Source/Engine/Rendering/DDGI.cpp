/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/DDGI.hpp>
#include <Rendering/AccelerationStructure.hpp>

#include <Rendering/Passes/DeferredPass.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/Frame.hpp>
#include <Rendering/GpuBuffer.hpp>
#include <Rendering/GpuImage.hpp>
#include <Rendering/RendererMain.hpp>
#include <Rendering/RenderProxyList.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/ShaderInstance.hpp>
#include <Rendering/RenderHelpers.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/TextureViewCache.hpp>
#include <Rendering/CBufferAllocator.hpp>

#include <Rendering/Shadows/ShadowMapCache.hpp>

#include <Rendering/Util/DeletionQueue.hpp>
#include <Rendering/Util/ShaderPropertyDictionary.hpp>

#include <Scene/EnvProbe.hpp>

#include <Core/Utilities/DeferredScope.hpp>

#include <Framework/EngineDriver.hpp>
#include <Framework/View.hpp>

namespace Hyperion {

static constexpr TextureFormat IrradianceFormat = TextureFormat::RGBA16F;
static constexpr TextureFormat DepthFormat = TextureFormat::RG16F;
static constexpr uint32 DDGIMaxBoundLights = 4;

static constexpr float DDGIHysteresis = 0.97f;

static StaticShaderPropertyId s_propUpdateProbeDataModeIrradiance { ShaderProperty(NAME("MODE"), NAME("IRRADIANCE")) };
static StaticShaderPropertyId s_propUpdateProbeDataModeDepth { ShaderProperty(NAME("MODE"), NAME("DEPTH")) };

DDGI::DDGI(DDGIInfo&& gridInfo)
    : m_gridInfo(std::move(gridInfo)),
      m_cascadeUpdateMask(0),
      m_cascadeResetMask(0),
      m_counter(0)
{
    m_gridInfo.numCascades = MathUtil::Clamp(m_gridInfo.numCascades, 1u, MaxCascades);
    m_gridInfo.probeCountsPerCascade = MathUtil::Max(m_gridInfo.probeCountsPerCascade, Vec3u { 2, 2, 2 });
    m_gridInfo.probeDistance = MathUtil::Max(m_gridInfo.probeDistance, 0.01f);
    m_gridInfo.numRaysPerProbe = MathUtil::Max(m_gridInfo.numRaysPerProbe, 1u);
}

DDGI::~DDGI()
{
    EnqueueDeletion(std::move(m_cbuffers));
    EnqueueDeletion(std::move(m_radianceBuffer));
    EnqueueDeletion(std::move(m_irradianceTexture));
    EnqueueDeletion(std::move(m_visibilityTexture));
}

uint32 DDGI::NumProbesPerCascade() const
{
    return m_gridInfo.probeCountsPerCascade.x * m_gridInfo.probeCountsPerCascade.y * m_gridInfo.probeCountsPerCascade.z;
}

uint32 DDGI::NumProbesTotal() const
{
    return NumProbesPerCascade() * m_gridInfo.numCascades;
}

Vec2u DDGI::GetRayDataDimensions() const
{
    return { NumProbesTotal(), m_gridInfo.numRaysPerProbe };
}

void DDGI::Create()
{
    InitializeCascades();

    CreateConstantBuffers();
    CreateStorageBuffers();
}

void DDGI::InitializeCascades()
{
    for (uint32 cascadeIndex = 0; cascadeIndex < m_gridInfo.numCascades; cascadeIndex++)
    {
        CascadeState& cascade = m_cascades[cascadeIndex];

        cascade.probeSpacing = m_gridInfo.probeDistance * float(1u << cascadeIndex);
        cascade.updateInterval = 1u << cascadeIndex;
        cascade.blendAlpha = 1.0f - MathUtil::Pow(DDGIHysteresis, float(cascade.updateInterval));
        cascade.gridOffset = Vec3i::Zero();
        cascade.gridOffsetPrev = Vec3i::Zero();
        cascade.needsReset = true;
    }
}

void DDGI::ScrollCascades(const Vec3f& cameraPosition)
{
    m_cascadeUpdateMask = 0;
    m_cascadeResetMask = 0;

    for (uint32 cascadeIndex = 0; cascadeIndex < m_gridInfo.numCascades; cascadeIndex++)
    {
        CascadeState& cascade = m_cascades[cascadeIndex];

        const bool shouldUpdate = cascade.needsReset
            || (m_counter % cascade.updateInterval) == (cascadeIndex % cascade.updateInterval);

        if (!shouldUpdate)
        {
            continue;
        }

        m_cascadeUpdateMask |= 1u << cascadeIndex;

        if (cascade.needsReset)
        {
            m_cascadeResetMask |= 1u << cascadeIndex;
        }

        Vec3i gridOffset;

        for (uint32 axis = 0; axis < 3; axis++)
        {
            const int centerLatticeCoord = MathUtil::Floor((cameraPosition[axis] / cascade.probeSpacing) + 0.5f);

            gridOffset[axis] = centerLatticeCoord - int(m_gridInfo.probeCountsPerCascade[axis] / 2);
        }

        cascade.gridOffsetPrev = cascade.gridOffset;
        cascade.gridOffset = gridOffset;
        cascade.needsReset = false;
    }
}

void DDGI::CreateConstantBuffers()
{
    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
    {
        m_cbuffers[frameIndex] = RI.MakeGpuBuffer(GpuBufferType::ConstantBuffer, ByteUtil::AlignAs(sizeof(DDGIConstants), 256));
        Assert(m_cbuffers[frameIndex]->Create());

        m_cbuffers[frameIndex]->Memset(sizeof(DDGIConstants), 0);
    }
}

void DDGI::CreateStorageBuffers()
{
    const Vec3u probeCounts = m_gridInfo.probeCountsPerCascade;
    const Vec2u rayDataDimensions = GetRayDataDimensions();

    m_radianceBuffer = RI.MakeGpuBuffer(GpuBufferType::RWStructuredBuffer, rayDataDimensions.x * rayDataDimensions.y * sizeof(ProbeRayData));
    Assert(m_radianceBuffer->Create());

    // Cascades are stacked vertically in the probe atlases; each one owns probeCounts.z rows of probeCounts.x * probeCounts.y probes.
    { // irradiance image
        const Vec3u extent {
            (IrradianceOctahedronSize + ProbeBorder.x) * probeCounts.x * probeCounts.y + 2,
            (IrradianceOctahedronSize + ProbeBorder.x) * probeCounts.z * m_gridInfo.numCascades + 2,
            1
        };

        m_irradianceTexture = MakeHandle<Texture>(
            TextureDesc {
                TextureType::Texture2D,
                IrradianceFormat,
                extent,
                TextureFilterMode::Nearest,
                TextureFilterMode::Nearest,
                TextureWrapMode::ClampToEdge,
                1,
                ImageUsage::Storage | ImageUsage::Sampled
            });

        m_irradianceTexture->SetName(NAME("DDGIIrradianceTexture"));
        m_irradianceTexture->SetIsTransient(true);
        Check(m_irradianceTexture->Create());
    }

    { // depth image
        const Vec3u extent {
            (DepthOctahedronSize + ProbeBorder.x) * probeCounts.x * probeCounts.y + 2,
            (DepthOctahedronSize + ProbeBorder.x) * probeCounts.z * m_gridInfo.numCascades + 2,
            1
        };

        m_visibilityTexture = MakeHandle<Texture>(
            TextureDesc {
                TextureType::Texture2D,
                DepthFormat,
                extent,
                TextureFilterMode::Nearest,
                TextureFilterMode::Nearest,
                TextureWrapMode::ClampToEdge,
                1,
                ImageUsage::Storage | ImageUsage::Sampled
            });

        m_visibilityTexture->SetName(NAME("DDGIVisibilityTexture"));
        m_visibilityTexture->SetIsTransient(true);
        Check(m_visibilityTexture->Create());
    }
}

void DDGI::UpdateUniforms(Frame* frame, const RenderSetup& renderSetup)
{
    const uint32 frameIndex = frame->GetFrameIndex();

    RenderProxyList& rpl = GetConsumerProxyList(renderSetup.view);
    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    Vec3f cameraPosition = Vec3f::Zero();

    if (Camera* camera = renderSetup.view->GetCamera())
    {
        if (RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(GetRenderProxy(camera)))
        {
            cameraPosition = cameraProxy->bufferData.cameraPosition.GetXYZ();
        }
    }

    ScrollCascades(cameraPosition);

    const Vec2u rayDataDimensions = GetRayDataDimensions();

    DDGIConstants ddgiConstants {};
    ddgiConstants.rotationMatrix = m_randomGenerator.Next();
    ddgiConstants.probeBorder = Vec4u(ProbeBorder, 0);
    ddgiConstants.probeCounts = Vec4u(m_gridInfo.probeCountsPerCascade, 0);
    ddgiConstants.gridDimensions = { rayDataDimensions.x, rayDataDimensions.y, 0, 0 };
    ddgiConstants.imageDimensions = Vec4u { m_irradianceTexture->GetExtent().GetXY(), m_visibilityTexture->GetExtent().GetXY() };
    ddgiConstants.numCascades = m_gridInfo.numCascades;
    ddgiConstants.numRaysPerProbe = m_gridInfo.numRaysPerProbe;
    ddgiConstants.numBoundLights = 0;
    ddgiConstants.counter = m_counter++;
    ddgiConstants.cascadeUpdateMask = m_cascadeUpdateMask;
    ddgiConstants.cascadeResetMask = m_cascadeResetMask;
    ddgiConstants.probeDistance = m_gridInfo.probeDistance;

    for (uint32 cascadeIndex = 0; cascadeIndex < m_gridInfo.numCascades; cascadeIndex++)
    {
        const CascadeState& cascade = m_cascades[cascadeIndex];

        DDGICascadeData& cascadeData = ddgiConstants.cascades[cascadeIndex];
        cascadeData.gridOffset = Vec4i(cascade.gridOffset, 0);
        cascadeData.gridOffsetPrev = Vec4i(cascade.gridOffsetPrev, 0);
        cascadeData.probeSpacing = Vec4f(Vec3f(cascade.probeSpacing), 0.0f);
        cascadeData.blendAlpha = cascade.blendAlpha;
        cascadeData.rayMaxDistance = m_gridInfo.rayMaxDistance;
        cascadeData.normalBias = cascade.probeSpacing * 0.05f;
    }

    Array<Pair<Light*, LightShaderData*>, RenderTempAllocator> tempLights;
    tempLights.Reserve(DDGIMaxBoundLights);

    for (Light* light : rpl.GetLights())
    {
        const LightType lightType = light->GetLightType();

        if (lightType != LightType::Directional && lightType != LightType::Point)
        {
            continue;
        }

        if (ddgiConstants.numBoundLights >= DDGIMaxBoundLights)
        {
            break;
        }

        RenderProxyLight* lightProxy = static_cast<RenderProxyLight*>(GetRenderProxy(light));
        Assert(lightProxy != nullptr);

        tempLights.EmplaceBack(light, &lightProxy->bufferData);
        ++ddgiConstants.numBoundLights;
    }

    // Update static DDGIConstants buffer (used by UpdateProbeData compute and DeferredIndirect)
    m_cbuffers[frameIndex]->Copy(sizeof(ddgiConstants), &ddgiConstants);
    m_cbuffers[frameIndex]->Flush(0, sizeof(ddgiConstants));

    // Build dynamic CBuffer for DDGI raygen: DDGIConstants + lights[MAX_LIGHTS] + EnvProbe
    RI.cbufferAllocator->Write(&ddgiConstants);

    for (uint32 i = 0; i < DDGIMaxBoundLights; i++)
    {
        if (i < uint32(tempLights.Size()))
        {
            RI.cbufferAllocator->Write(tempLights[i].second);
            continue;
        }

        LightShaderData dummy {};
        RI.cbufferAllocator->Write(&dummy);
    }

    {
        const EnvProbeShaderData* pEnvProbeShaderData = nullptr;

        if (renderSetup.envProbe != nullptr)
        {
            RenderProxyEnvProbe* envProbeProxy = static_cast<RenderProxyEnvProbe*>(GetRenderProxy(renderSetup.envProbe));
            Assert(envProbeProxy != nullptr);
            pEnvProbeShaderData = &envProbeProxy->bufferData;
        }
        else
        {
            static const EnvProbeShaderData s_dummyEnvProbeShaderData {};
            pEnvProbeShaderData = &s_dummyEnvProbeShaderData;
        }

        RI.cbufferAllocator->Write(pEnvProbeShaderData);
    }

    RI.cbufferAllocator->Commit(m_dynamicCBuffer, m_dynamicCBufferOffset, m_dynamicCBufferSize);
}

void DDGI::Render(Frame* frame, const RenderSetup& renderSetup)
{
    AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.world && renderSetup.view);
    AssertDebug(renderSetup.passData != nullptr);

    UpdateUniforms(frame, renderSetup);

    if (m_cascadeUpdateMask == 0)
    {
        return;
    }

    RayTracingPassData* pd = DynamicCast<RayTracingPassData>(renderSetup.passData);
    Assert(pd != nullptr);

    const uint32 frameIndex = frame->GetFrameIndex();

    TopLevelAS* tlas = pd->rayTracingTlases[frameIndex];
    Assert(tlas != nullptr);

    const StructuredBuffer& meshDescriptionsBuffer = tlas->GetMeshDescriptionsBuffer();

    frame->cr << InsertBarrier(m_radianceBuffer, ResourceState::UnorderedAccess);

    ShaderPropertySet shaderProperties;
    frame->cr << SetCurrentShader(ShaderDesc(NAME("DDGI"), shaderProperties));

    frame->cr << SetShaderUniform(0, "SamplerNearest"_sh, RI.placeholderData->GetSamplerNearest());
    frame->cr << SetShaderUniform(1, "SamplerLinear"_sh, RI.placeholderData->GetSamplerLinearMipmap());
    frame->cr << SetShaderUniform(2, "TLAS"_sh, tlas);
    frame->cr << SetShaderUniform(3, "MeshDescriptionsBuffer"_sh, meshDescriptionsBuffer);
    frame->cr << SetShaderUniform(4, "CBuffer"_sh, m_dynamicCBuffer, ShaderDataOffset(m_dynamicCBufferOffset, m_dynamicCBufferSize));
    frame->cr << SetShaderUniform(5, "ProbeRayData"_sh, m_radianceBuffer, ShaderDataOffset(0, sizeof(ProbeRayData)));

    frame->cr << SetShaderUniform(6, "ShadowMapsTextureArray"_sh, RI.shadowMapCache->GetAtlasImageView());
    frame->cr << SetShaderUniform(7, "PointLightShadowMapsTextureArray"_sh, RI.shadowMapCache->GetPointLightShadowMapImageView());

    frame->cr << SetShaderUniform(8, "MaterialsBuffer"_sh, RI.namedBuffers[NamedBuffer::Materials]);
    frame->cr << SetShaderUniform(9, "EntitiesBuffer"_sh, RI.namedBuffers[NamedBuffer::Entities]);
    frame->cr << SetShaderUniform(10, "WorldsBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);

    frame->cr << SetShaderUniform(11, "EnvProbesColorTexture"_sh, RI.textureViewCache->GetOrCreate(RI.envProbesColorTexture));
    frame->cr << SetShaderUniform(12, "EnvProbesDepthTexture"_sh, RI.textureViewCache->GetOrCreate(RI.envProbesDepthTexture));

    // Probes in cascades that aren't scheduled this frame exit immediately in the raygen shader.
    frame->cr << TraceRays(Vec3u { NumProbesTotal(), m_gridInfo.numRaysPerProbe, 1u });

    frame->cr << InsertBarrier(m_radianceBuffer, ResourceState::UnorderedAccess);

    const Vec3u probeCounts = m_gridInfo.probeCountsPerCascade;
    const Vec3u updateProbeDataGroupCount { probeCounts.x * probeCounts.y, probeCounts.z * m_gridInfo.numCascades, 1u };

    frame->cr << InsertBarrier(m_irradianceTexture->GetGpuImage(), ResourceState::UnorderedAccess);
    frame->cr << InsertBarrier(m_visibilityTexture->GetGpuImage(), ResourceState::UnorderedAccess);

    // Update irradiance
    shaderProperties = ShaderPropertySet();
    shaderProperties.Add(s_propUpdateProbeDataModeIrradiance);

    frame->cr << SetCurrentShader(ShaderDesc(NAME("UpdateProbeData"), shaderProperties));

    frame->cr << SetShaderUniform(0, "CBuffer"_sh, m_dynamicCBuffer, ShaderDataOffset(m_dynamicCBufferOffset, m_dynamicCBufferSize));
    frame->cr << SetShaderUniform(1, "ProbeRayData"_sh, m_radianceBuffer, ShaderDataOffset(0, sizeof(ProbeRayData)));
    frame->cr << SetShaderUniform(2, "OutputImage"_sh, RI.textureViewCache->GetOrCreate(m_irradianceTexture));

    frame->cr << DispatchCompute(updateProbeDataGroupCount);

    frame->cr << InsertBarrier(m_irradianceTexture->GetGpuImage(), ResourceState::ShaderResource);

    // Update depth
    shaderProperties = ShaderPropertySet();
    shaderProperties.Add(s_propUpdateProbeDataModeDepth);

    frame->cr << SetCurrentShader(ShaderDesc(NAME("UpdateProbeData"), shaderProperties));

    frame->cr << SetShaderUniform(0, "CBuffer"_sh, m_dynamicCBuffer, ShaderDataOffset(m_dynamicCBufferOffset, m_dynamicCBufferSize));
    frame->cr << SetShaderUniform(1, "ProbeRayData"_sh, m_radianceBuffer, ShaderDataOffset(0, sizeof(ProbeRayData)));
    frame->cr << SetShaderUniform(2, "OutputImage"_sh, RI.textureViewCache->GetOrCreate(m_visibilityTexture));

    frame->cr << DispatchCompute(updateProbeDataGroupCount);

    frame->cr << InsertBarrier(m_visibilityTexture->GetGpuImage(), ResourceState::ShaderResource);
}

} // namespace Hyperion
