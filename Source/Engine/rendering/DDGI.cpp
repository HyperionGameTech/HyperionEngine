/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/DDGI.hpp>
#include <rendering/AccelerationStructure.hpp>

#include <rendering/renderers/DeferredRenderer.hpp>

#include <rendering/RenderInterface.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/Frame.hpp>
#include <rendering/GpuBuffer.hpp>
#include <rendering/GpuImage.hpp>
#include <rendering/RenderCollection.hpp>
#include <rendering/RenderProxyList.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/ShaderInstance.hpp>
#include <rendering/RenderHelpers.hpp>

#include <rendering/shadows/ShadowMapAllocator.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <scene/View.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/EnvGrid.hpp>

#include <Core/utilities/ByteUtil.hpp>
#include <Core/utilities/DeferredScope.hpp>

#include <engine/EngineDriver.hpp>

namespace Hyperion {

static constexpr TextureFormat DdgiIrradianceFormat = TextureFormat::RGBA16F;
static constexpr TextureFormat DdgiDepthFormat = TextureFormat::RG16F;
static constexpr uint32 MaxBoundLights = sizeof(DDGIConstants::lightIndices) / sizeof(uint32);

#pragma region Render commands

#pragma endregion Render commands

DDGI::DDGI(DDGIInfo&& gridInfo)
    : m_gridInfo(std::move(gridInfo)),
      m_counter(0)
{
}

DDGI::~DDGI()
{
    EnqueueDeletion(std::move(m_cBuffers));
    EnqueueDeletion(std::move(m_lightsBuffers));
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
    const Vec3u grid = m_gridInfo.NumProbesPerDimension();
    m_probeData.Resize(m_gridInfo.NumProbes());

    for (uint32 x = 0; x < grid.x; x++)
    {
        for (uint32 y = 0; y < grid.y; y++)
        {
            for (uint32 z = 0; z < grid.z; z++)
            {
                const uint32 index = x * grid.x * grid.y + y * grid.z + z;

                m_probeData[index] = DDGIProbeData {
                    (Vec3f { float(x), float(y), float(z) } - (Vec3f(m_gridInfo.probeBorder) * 0.5f)) * m_gridInfo.probeDistance
                };
            }
        }
    }
}

void DDGI::CreateConstantBuffers()
{
    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
    {
        m_cBuffers[frameIndex] = g_renderInterface->MakeGpuBuffer(GpuBufferType::CONSTANT_BUFFER, sizeof(DDGIConstants));
        Assert(m_cBuffers[frameIndex]->Create());
        m_cBuffers[frameIndex]->Memset(sizeof(DDGIConstants), 0);
        
        m_lightsBuffers[frameIndex] = g_renderInterface->MakeGpuBuffer(GpuBufferType::CONSTANT_BUFFER, sizeof(LightShaderData) * MaxBoundLights);
        Assert(m_lightsBuffers[frameIndex]->Create());
        m_lightsBuffers[frameIndex]->Memset(sizeof(LightShaderData) * MaxBoundLights, 0);
    }
}

void DDGI::CreateStorageBuffers()
{
    const Vec3u probeCounts = m_gridInfo.NumProbesPerDimension();

    m_radianceBuffer = g_renderInterface->MakeGpuBuffer(GpuBufferType::STORAGE_BUFFER, m_gridInfo.GetImageDimensions().x * m_gridInfo.GetImageDimensions().y * sizeof(ProbeRayData));
    m_radianceBuffer->SetRequireCpuAccessible(true);
    Assert(m_radianceBuffer->Create());
    m_radianceBuffer->Memset(m_radianceBuffer->Size(), 0);

    { // irradiance image
        const Vec3u extent {
            (m_gridInfo.irradianceOctahedronSize + 2) * probeCounts.x * probeCounts.y + 2,
            (m_gridInfo.irradianceOctahedronSize + 2) * probeCounts.z + 2,
            1
        };

        m_irradianceImage = g_renderInterface->MakeImage(TextureDesc {
            TextureType::Texture2D,
            DdgiIrradianceFormat,
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
        m_irradianceImageView = g_renderInterface->MakeImageView(m_irradianceImage);
        Assert(m_irradianceImageView->Create());
    }

    { // depth image
        const Vec3u extent {
            (m_gridInfo.depthOctahedronSize + 2) * probeCounts.x * probeCounts.y + 2,
            (m_gridInfo.depthOctahedronSize + 2) * probeCounts.z + 2,
            1
        };

        m_depthImage = g_renderInterface->MakeImage(TextureDesc {
            TextureType::Texture2D,
            DdgiDepthFormat,
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
        m_depthImageView = g_renderInterface->MakeImageView(m_depthImage);

        Assert(m_depthImageView->Create());
    }
}

void DDGI::UpdateUniforms(Frame* frame, const RenderSetup& renderSetup)
{
    const uint32 frameIndex = frame->GetFrameIndex();

    RenderProxyList& rpl = GetConsumerProxyList(renderSetup.view);
    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    const Vec2u gridImageDimensions = m_gridInfo.GetImageDimensions();
    const Vec3u numProbesPerDimension = m_gridInfo.NumProbesPerDimension();

    DDGIConstants uniforms {};
    uniforms.rotationMatrix = m_randomGenerator.Next();
    uniforms.aabbMax = Vec4f(m_gridInfo.aabb.max, 1.0f);
    uniforms.aabbMin = Vec4f(m_gridInfo.aabb.min, 1.0f);
    uniforms.probeBorder = Vec4u(m_gridInfo.probeBorder, 0);
    uniforms.probeCounts = { numProbesPerDimension.x, numProbesPerDimension.y, numProbesPerDimension.z, 0 };
    uniforms.gridDimensions = { gridImageDimensions.x, gridImageDimensions.y, 0, 0 };
    uniforms.imageDimensions = { m_irradianceImage->GetExtent().x, m_irradianceImage->GetExtent().y, m_depthImage->GetExtent().x, m_depthImage->GetExtent().y };
    uniforms.probeDistance = m_gridInfo.probeDistance;
    uniforms.numRaysPerProbe = m_gridInfo.numRaysPerProbe;
    uniforms.numBoundLights = 0;

    uint32* lightIndicesU32 = reinterpret_cast<uint32*>(uniforms.lightIndices);
    Memory::Fill(lightIndicesU32, 0, sizeof(uniforms.lightIndices));

    for (Light* light : rpl.GetLights())
    {
        const LightType lightType = light->GetLightType();

        if (lightType != LT_DIRECTIONAL && lightType != LT_POINT)
        {
            continue;
        }

        if (uniforms.numBoundLights >= MaxBoundLights)
        {
            break;
        }
        
        RenderProxyLight* lightProxy = static_cast<RenderProxyLight*>(GetRenderProxy(light));
        Assert(lightProxy != nullptr);
                

        m_lightsBuffers[frameIndex]->Copy(
            uniforms.numBoundLights * sizeof(LightShaderData),
            sizeof(LightShaderData),
            &lightProxy->bufferData);

        lightIndicesU32[uniforms.numBoundLights++] = RetrieveResourceBinding(light);
    }

    m_cBuffers[frameIndex]->Copy(sizeof(DDGIConstants), &uniforms);

    if (m_counter == 0)
    {
        uniforms.flags |= PROBE_SYSTEM_FLAGS_FIRST_RUN;
    }
}

void DDGI::Render(Frame* frame, const RenderSetup& renderSetup)
{
    AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.world && renderSetup.view);
    AssertDebug(renderSetup.passData != nullptr);

    UpdateUniforms(frame, renderSetup);

    RayTracingPassData* pd = ObjCast<RayTracingPassData>(renderSetup.passData);
    Assert(pd != nullptr);

    const uint32 frameIndex = frame->GetFrameIndex();
    const GpuTlasRef& tlas = pd->rayTracingTlases[frameIndex];
    Assert(tlas != nullptr);

    const GpuBufferRef& meshDescriptionsBuffer = tlas->GetMeshDescriptionsBuffer();
    Assert(meshDescriptionsBuffer != nullptr && meshDescriptionsBuffer->IsCreated());

    frame->renderQueue << InsertBarrier(m_radianceBuffer, RS_UNORDERED_ACCESS);

    ShaderPropertySet shaderProperties;
    shaderProperties.Add(InternShaderProperty(ShaderProperty(NAME("MAX_LIGHTS"), int(MaxBoundLights))));

    if (renderSetup.envProbe != nullptr)
        shaderProperties.Add(InternShaderProperty(ShaderProperty(NAME("HAS_ENV_PROBE"))));

    frame->renderQueue << SetCurrentShader(ShaderDesc(NAME("DDGI"), shaderProperties));
    
    frame->renderQueue << SetShaderUniform(0, "SamplerNearest"_sh, g_renderInterface->placeholderData->GetSamplerNearest());
    frame->renderQueue << SetShaderUniform(1, "SamplerLinear"_sh, g_renderInterface->placeholderData->GetSamplerLinearMipmap());
    frame->renderQueue << SetShaderUniform(2, "TLAS"_sh, tlas);
    frame->renderQueue << SetShaderUniform(3, "MeshDescriptionsBuffer"_sh, meshDescriptionsBuffer);
    frame->renderQueue << SetShaderUniform(4, "DDGIConstants"_sh, m_cBuffers[frameIndex]);
    frame->renderQueue << SetShaderUniform(5, "Lights"_sh, m_lightsBuffers[frameIndex]);
    frame->renderQueue << SetShaderUniform(6, "ProbeRayData"_sh, m_radianceBuffer);
    
    frame->renderQueue << SetShaderUniform(7, "ShadowMapsTextureArray"_sh, g_renderInterface->shadowMapAllocator->GetAtlasImageView());
    frame->renderQueue << SetShaderUniform(8, "PointLightShadowMapsTextureArray"_sh, g_renderInterface->shadowMapAllocator->GetPointLightShadowMapImageView());

    frame->renderQueue << SetShaderUniform(9, "MaterialsBuffer"_sh, g_renderInterface->gpuBuffers[GRB_MATERIALS]->GetBuffer(frameIndex));
    frame->renderQueue << SetShaderUniform(10, "EntitiesBuffer"_sh, g_renderInterface->gpuBuffers[GRB_ENTITIES]->GetBuffer(frameIndex));
    frame->renderQueue << SetShaderUniform(11, "WorldsBuffer"_sh, g_renderInterface->gpuBuffers[GRB_WORLDS]->GetBuffer(frameIndex));

    if (renderSetup.envProbe != nullptr)
        frame->renderQueue << SetShaderUniform(12, "CurrentEnvProbe"_sh, g_renderInterface->gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frameIndex), TShaderDataOffset<EnvProbeShaderData>(renderSetup.envProbe));

    frame->renderQueue << TraceRays(Vec3u { m_gridInfo.NumProbes(), m_gridInfo.numRaysPerProbe, 1u });

    frame->renderQueue << InsertBarrier(m_radianceBuffer, RS_UNORDERED_ACCESS);

    // Compute irradiance for ray traced probes
    const Vec3u probeCounts = m_gridInfo.NumProbesPerDimension();

    frame->renderQueue << InsertBarrier(m_irradianceImage, RS_UNORDERED_ACCESS);
    frame->renderQueue << InsertBarrier(m_depthImage, RS_UNORDERED_ACCESS);

    // Update irradiance
    frame->renderQueue << SetCurrentShader(ShaderDesc(NAME("RTProbeUpdateIrradiance")));
    frame->renderQueue << SetShaderUniform(0, "DDGIConstants"_sh, m_cBuffers[frameIndex]);
    frame->renderQueue << SetShaderUniform(1, "ProbeRayData"_sh, m_radianceBuffer);
    frame->renderQueue << SetShaderUniform(2, "OutputIrradianceImage"_sh, m_irradianceImageView);
    frame->renderQueue << SetShaderUniform(3, "OutputDepthImage"_sh, m_depthImageView);

    frame->renderQueue << DispatchCompute(Vec3u { probeCounts.x * probeCounts.y, probeCounts.z, 1u });

    // Update depth
    frame->renderQueue << SetCurrentShader(ShaderDesc(NAME("RTProbeUpdateDepth")));
    frame->renderQueue << SetShaderUniform(0, "DDGIConstants"_sh, m_cBuffers[frameIndex]);
    frame->renderQueue << SetShaderUniform(1, "ProbeRayData"_sh, m_radianceBuffer);
    frame->renderQueue << SetShaderUniform(2, "OutputIrradianceImage"_sh, m_irradianceImageView);
    frame->renderQueue << SetShaderUniform(3, "OutputDepthImage"_sh, m_depthImageView);

    frame->renderQueue << DispatchCompute(Vec3u { probeCounts.x * probeCounts.y, probeCounts.z, 1u });

#if 0 // @FIXME: Properly implement an optimized way to copy border texels without invoking for each pixel in the images.
    frame->renderQueue << InsertBarrier(m_irradianceImage, RS_UNORDERED_ACCESS);
    frame->renderQueue << InsertBarrier(m_depthImage, RS_UNORDERED_ACCESS);

    // Copy border texels irradiance
    frame->renderQueue << SetCurrentShader(ShaderDesc(NAME("RTCopyBorderTexelsIrradiance")));
    frame->renderQueue << SetShaderUniform(0, "DDGIConstants"_sh, m_cBuffers[frameIndex]);
    frame->renderQueue << SetShaderUniform(1, "ProbeRayData"_sh, m_radianceBuffer);
    frame->renderQueue << SetShaderUniform(2, "OutputIrradianceImage"_sh, m_irradianceImageView);
    frame->renderQueue << SetShaderUniform(3, "OutputDepthImage"_sh, m_depthImageView);

    frame->renderQueue << DispatchCompute(Vec3u {
        (probeCounts.x * probeCounts.y * (m_gridInfo.irradianceOctahedronSize + m_gridInfo.probeBorder.x)) + 7 / 8,
        (probeCounts.z * (m_gridInfo.irradianceOctahedronSize + m_gridInfo.probeBorder.z)) + 7 / 8,
        1u
    });

    // Copy border texels depth
    frame->renderQueue << SetCurrentShader(ShaderDesc(NAME("RTCopyBorderTexelsDepth")));
    frame->renderQueue << SetShaderUniform(0, "DDGIConstants"_sh, m_cBuffers[frameIndex]);
    frame->renderQueue << SetShaderUniform(1, "ProbeRayData"_sh, m_radianceBuffer);
    frame->renderQueue << SetShaderUniform(2, "OutputIrradianceImage"_sh, m_irradianceImageView);
    frame->renderQueue << SetShaderUniform(3, "OutputDepthImage"_sh, m_depthImageView);

    frame->renderQueue << DispatchCompute(Vec3u {
        (probeCounts.x * probeCounts.y * (m_gridInfo.depthOctahedronSize + m_gridInfo.probeBorder.x)) + 15 / 16,
        (probeCounts.z * (m_gridInfo.depthOctahedronSize + m_gridInfo.probeBorder.z)) + 15 / 16,
        1u
    });

    frame->renderQueue << InsertBarrier(m_irradianceImage, RS_SHADER_RESOURCE);
    frame->renderQueue << InsertBarrier(m_depthImage, RS_SHADER_RESOURCE);
#endif
}

} // namespace Hyperion
