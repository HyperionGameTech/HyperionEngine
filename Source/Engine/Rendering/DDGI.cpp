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

#include <Scene/View.hpp>
#include <Scene/EnvProbe.hpp>

#include <Core/Utilities/DeferredScope.hpp>

#include <Framework/EngineDriver.hpp>

namespace Hyperion {

static constexpr TextureFormat IrradianceFormat = TextureFormat::RGBA8;
static constexpr TextureFormat DepthFormat = TextureFormat::RG16F;
static constexpr uint32 DDGIMaxBoundLights = 4;

static const ShaderPropertyId s_propUpdateProbeDataModeIrradiance = InternShaderProperty(ShaderProperty(NAME("MODE"), NAME("IRRADIANCE")));
static const ShaderPropertyId s_propUpdateProbeDataModeDepth = InternShaderProperty(ShaderProperty(NAME("MODE"), NAME("DEPTH")));

static Vec3u NumProbesPerDimension(const DDGIInfo& info)
{
    const Vec3f probesPerDimension = MathUtil::Ceil((info.aabb.GetExtent() / info.probeDistance) + Vec3f(DDGI::ProbeBorder));

    return Vec3u(probesPerDimension);
}

static uint32 NumProbes(const DDGIInfo& info)
{
    const Vec3u perDimension = NumProbesPerDimension(info);

    return perDimension.x * perDimension.y * perDimension.z;
}

static Vec2u GetImageDimensions(const DDGIInfo& info)
{
    return { uint32(MathUtil::NextPowerOf2(NumProbes(info))), info.numRaysPerProbe };
}

DDGI::DDGI(DDGIInfo&& gridInfo)
    : m_gridInfo(std::move(gridInfo)),
      m_counter(0)
{
}

DDGI::~DDGI()
{
    EnqueueDeletion(std::move(m_cbuffers));
    EnqueueDeletion(std::move(m_radianceBuffer));
    EnqueueDeletion(std::move(m_irradianceImage));
    EnqueueDeletion(std::move(m_irradianceImageView));
    EnqueueDeletion(std::move(m_depthImage));
    EnqueueDeletion(std::move(m_depthImageView));
}

void DDGI::Create()
{
    FillProbeGrid();

    CreateConstantBuffers();
    CreateStorageBuffers();
}

void DDGI::FillProbeGrid()
{
    const Vec3u grid = NumProbesPerDimension(m_gridInfo);
    m_probeData.Resize(NumProbes(m_gridInfo));

    for (uint32 x = 0; x < grid.x; x++)
    {
        for (uint32 y = 0; y < grid.y; y++)
        {
            for (uint32 z = 0; z < grid.z; z++)
            {
                const uint32 index = x * grid.x * grid.y + y * grid.z + z;

                m_probeData[index] = DDGIProbeData {
                    (Vec3f { float(x), float(y), float(z) } - (Vec3f(ProbeBorder) * 0.5f)) * m_gridInfo.probeDistance
                };
            }
        }
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
    const Vec3u probeCounts = NumProbesPerDimension(m_gridInfo);
    const Vec2u imageDimensions = GetImageDimensions(m_gridInfo);

    // RWStructuredBuffer must live on the DEFAULT heap so that D3D12 can set
    // D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS.  UPLOAD-heap resources cannot
    // carry that flag, which would cause CreateUnorderedAccessView to fail when
    // the descriptor set is updated.  The buffer is written by the GPU raygen
    // shader every frame before it is read by the compute pass, so CPU-side
    // initialisation is not required.
    m_radianceBuffer = RI.MakeGpuBuffer(GpuBufferType::RWStructuredBuffer, imageDimensions.x * imageDimensions.y * sizeof(ProbeRayData));
    Assert(m_radianceBuffer->Create());

    { // irradiance image
        const Vec3u extent {
            (IrradianceOctahedronSize + 2) * probeCounts.x * probeCounts.y + 2,
            (IrradianceOctahedronSize + 2) * probeCounts.z + 2,
            1
        };

        m_irradianceImage = RI.MakeImage(TextureDesc {
            TextureType::Texture2D,
            IrradianceFormat,
            extent,
            TFM_NEAREST,
            TFM_NEAREST,
            TWM_CLAMP_TO_EDGE,
            1,
            IU_STORAGE | IU_SAMPLED
        });
        Assert(m_irradianceImage->Create());
    }

    { // irradiance image view
        m_irradianceImageView = RI.MakeImageView(m_irradianceImage);
        Assert(m_irradianceImageView->Create());
    }

    { // depth image
        const Vec3u extent {
            (DepthOctahedronSize + 2) * probeCounts.x * probeCounts.y + 2,
            (DepthOctahedronSize + 2) * probeCounts.z + 2,
            1
        };

        m_depthImage = RI.MakeImage(TextureDesc {
            TextureType::Texture2D,
            DepthFormat,
            extent,
            TFM_NEAREST,
            TFM_NEAREST,
            TWM_CLAMP_TO_EDGE,
            1,
            IU_STORAGE | IU_SAMPLED
        });

        Assert(m_depthImage->Create());
    }

    { // depth image view
        m_depthImageView = RI.MakeImageView(m_depthImage);

        Assert(m_depthImageView->Create());
    }
}

void DDGI::UpdateUniforms(Frame* frame, const RenderSetup& renderSetup)
{
    const uint32 frameIndex = frame->GetFrameIndex();

    RenderProxyList& rpl = GetConsumerProxyList(renderSetup.view);
    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    const Vec2u gridImageDimensions = GetImageDimensions(m_gridInfo);
    const Vec3u numProbesPerDimension = NumProbesPerDimension(m_gridInfo);

    DDGIConstants ddgiConstants {};
    ddgiConstants.rotationMatrix = m_randomGenerator.Next();
    ddgiConstants.aabbMax = Vec4f(m_gridInfo.aabb.max, 1.0f);
    ddgiConstants.aabbMin = Vec4f(m_gridInfo.aabb.min, 1.0f);
    ddgiConstants.probeBorder = Vec4u(ProbeBorder, 0);
    ddgiConstants.probeCounts = { numProbesPerDimension.x, numProbesPerDimension.y, numProbesPerDimension.z, 0 };
    ddgiConstants.gridDimensions = { gridImageDimensions.x, gridImageDimensions.y, 0, 0 };
    ddgiConstants.imageDimensions = { m_irradianceImage->GetExtent().x, m_irradianceImage->GetExtent().y, m_depthImage->GetExtent().x, m_depthImage->GetExtent().y };
    ddgiConstants.probeDistance = m_gridInfo.probeDistance;
    ddgiConstants.numRaysPerProbe = m_gridInfo.numRaysPerProbe;
    ddgiConstants.numBoundLights = 0;
    ddgiConstants.counter = m_counter++;

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
            static const EnvProbeShaderData s_dummyEnvProbeShaderData;
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

    RayTracingPassData* pd = DynamicCast<RayTracingPassData>(renderSetup.passData);
    Assert(pd != nullptr);

    const uint32 frameIndex = frame->GetFrameIndex();

    GpuTlas* tlas = pd->rayTracingTlases[frameIndex];
    Assert(tlas != nullptr);

    GpuBuffer* meshDescriptionsBuffer = tlas->GetMeshDescriptionsBuffer();
    Assert(meshDescriptionsBuffer != nullptr && meshDescriptionsBuffer->IsCreated());

    frame->cr << InsertBarrier(m_radianceBuffer, RS_UNORDERED_ACCESS);

    ShaderPropertySet shaderProperties;
    shaderProperties.Add(InternShaderProperty(ShaderProperty(NAME("MAX_LIGHTS"), int(DDGIMaxBoundLights))));

    frame->cr << SetCurrentShader(ShaderDesc(NAME("DDGI"), shaderProperties));

    frame->cr << SetShaderUniform(0, "SamplerNearest"_sh, RI.placeholderData->GetSamplerNearest());
    frame->cr << SetShaderUniform(1, "SamplerLinear"_sh, RI.placeholderData->GetSamplerLinearMipmap());
    frame->cr << SetShaderUniform(2, "TLAS"_sh, tlas);
    frame->cr << SetShaderUniform(3, "MeshDescriptionsBuffer"_sh, meshDescriptionsBuffer, ShaderDataOffset(0, sizeof(MeshDescription)));
    frame->cr << SetShaderUniform(4, "CBuffer"_sh, m_dynamicCBuffer, ShaderDataOffset(m_dynamicCBufferOffset, m_dynamicCBufferSize));
    frame->cr << SetShaderUniform(5, "ProbeRayData"_sh, m_radianceBuffer, ShaderDataOffset(0, sizeof(ProbeRayData)));

    frame->cr << SetShaderUniform(6, "ShadowMapsTextureArray"_sh, RI.shadowMapCache->GetAtlasImageView());
    frame->cr << SetShaderUniform(7, "PointLightShadowMapsTextureArray"_sh, RI.shadowMapCache->GetPointLightShadowMapImageView());

    frame->cr << SetShaderUniform(8, "MaterialsBuffer"_sh, RI.namedBuffers[NamedBuffer::Materials]);
    frame->cr << SetShaderUniform(9, "EntitiesBuffer"_sh, RI.namedBuffers[NamedBuffer::Entities]);
    frame->cr << SetShaderUniform(10, "WorldsBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);

    frame->cr << SetShaderUniform(11, "EnvProbesTexture"_sh, RI.textureViewCache->GetOrCreate(RI.envProbesTexture));

    frame->cr << TraceRays(Vec3u { NumProbes(m_gridInfo), m_gridInfo.numRaysPerProbe, 1u });

    frame->cr << InsertBarrier(m_radianceBuffer, RS_UNORDERED_ACCESS);

    // Compute irradiance for ray traced probes
    const Vec3u probeCounts = NumProbesPerDimension(m_gridInfo);

    frame->cr << InsertBarrier(m_irradianceImage, RS_UNORDERED_ACCESS);
    frame->cr << InsertBarrier(m_depthImage, RS_UNORDERED_ACCESS);

    // Update irradiance
    shaderProperties = ShaderPropertySet();
    shaderProperties.Add(s_propUpdateProbeDataModeIrradiance);

    frame->cr << SetCurrentShader(ShaderDesc(NAME("UpdateProbeData"), shaderProperties));

    frame->cr << SetShaderUniform(0, "CBuffer"_sh, m_dynamicCBuffer, ShaderDataOffset(m_dynamicCBufferOffset, m_dynamicCBufferSize));
    frame->cr << SetShaderUniform(1, "ProbeRayData"_sh, m_radianceBuffer, ShaderDataOffset(0, sizeof(ProbeRayData)));
    frame->cr << SetShaderUniform(2, "OutputImage"_sh, m_irradianceImageView);

    frame->cr << DispatchCompute(Vec3u { probeCounts.x * probeCounts.y, probeCounts.z, 1u });

    frame->cr << InsertBarrier(m_irradianceImage, RS_UNORDERED_ACCESS);

    // Update depth
    shaderProperties = ShaderPropertySet();
    shaderProperties.Add(s_propUpdateProbeDataModeDepth);

    frame->cr << SetCurrentShader(ShaderDesc(NAME("UpdateProbeData"), shaderProperties));

    frame->cr << SetShaderUniform(0, "CBuffer"_sh, m_dynamicCBuffer, ShaderDataOffset(m_dynamicCBufferOffset, m_dynamicCBufferSize));
    frame->cr << SetShaderUniform(1, "ProbeRayData"_sh, m_radianceBuffer, ShaderDataOffset(0, sizeof(ProbeRayData)));
    frame->cr << SetShaderUniform(2, "OutputImage"_sh, m_depthImageView);

    frame->cr << DispatchCompute(Vec3u { probeCounts.x * probeCounts.y, probeCounts.z, 1u });

    frame->cr << InsertBarrier(m_irradianceImage, RS_SHADER_RESOURCE);
    frame->cr << InsertBarrier(m_depthImage, RS_SHADER_RESOURCE);

#if 0 // @FIXME: Properly implement an optimized way to copy border texels without invoking for each pixel in the images.
    frame->cr << InsertBarrier(m_irradianceImage, RS_UNORDERED_ACCESS);
    frame->cr << InsertBarrier(m_depthImage, RS_UNORDERED_ACCESS);

    // Copy border texels irradiance
    frame->cr << SetCurrentShader(ShaderDesc(NAME("RTCopyBorderTexelsIrradiance")));
    frame->cr << SetShaderUniform(0, "DDGIConstants"_sh, m_cbuffers[frameIndex]);
    frame->cr << SetShaderUniform(1, "ProbeRayData"_sh, m_radianceBuffer, ShaderDataOffset(0, sizeof(ProbeRayData)));
    frame->cr << SetShaderUniform(2, "OutputIrradianceImage"_sh, m_irradianceImageView);
    frame->cr << SetShaderUniform(3, "OutputDepthImage"_sh, m_depthImageView);

    frame->cr << DispatchCompute(Vec3u {
        (probeCounts.x * probeCounts.y * (m_gridInfo.irradianceOctahedronSize + m_gridInfo.probeBorder.x)) + 7 / 8,
        (probeCounts.z * (m_gridInfo.irradianceOctahedronSize + m_gridInfo.probeBorder.z)) + 7 / 8,
        1u
    });

    // Copy border texels depth
    frame->cr << SetCurrentShader(ShaderDesc(NAME("RTCopyBorderTexelsDepth")));
    frame->cr << SetShaderUniform(0, "DDGIConstants"_sh, m_cbuffers[frameIndex]);
    frame->cr << SetShaderUniform(1, "ProbeRayData"_sh, m_radianceBuffer, ShaderDataOffset(0, sizeof(ProbeRayData)));
    frame->cr << SetShaderUniform(2, "OutputIrradianceImage"_sh, m_irradianceImageView);
    frame->cr << SetShaderUniform(3, "OutputDepthImage"_sh, m_depthImageView);

    frame->cr << DispatchCompute(Vec3u {
        (probeCounts.x * probeCounts.y * (m_gridInfo.depthOctahedronSize + m_gridInfo.probeBorder.x)) + 15 / 16,
        (probeCounts.z * (m_gridInfo.depthOctahedronSize + m_gridInfo.probeBorder.z)) + 15 / 16,
        1u
    });

    frame->cr << InsertBarrier(m_irradianceImage, RS_SHADER_RESOURCE);
    frame->cr << InsertBarrier(m_depthImage, RS_SHADER_RESOURCE);
#endif
}

} // namespace Hyperion
