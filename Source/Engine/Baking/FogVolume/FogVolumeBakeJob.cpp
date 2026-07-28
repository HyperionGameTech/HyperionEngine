/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Baking/FogVolume/FogVolumeBakeJob.hpp>

#include <Scene/Scene.hpp>
#include <Scene/FogVolume.hpp>
#include <Scene/Light.hpp>
#include <Scene/EnvProbe.hpp>

#include <Scene/Util/VoxelOctree.hpp>

#include <Core/Utilities/Float16.hpp>

#include <bit>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/Frame.hpp>
#include <Rendering/GpuBuffer.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/TextureViewCache.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/CBufferAllocator.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/Util/DeletionQueue.hpp>

#include <Framework/EngineGlobals.hpp>

namespace Hyperion {
namespace Baking {

static constexpr uint32 NumPointLightShadowSamples = 8;
static constexpr float PointLightSourceRadiusScale = 0.05f;

static constexpr float ShBand0Normalization = 0.282095f;

static uint32 PackHalf2(float lo, float hi)
{
    return uint32(Float16(lo).Raw()) | (uint32(Float16(hi).Raw()) << 16);
}

static constexpr float MaxLightFalloffExponent = 8.0f;

struct FogVolumeLightGpuData
{
    Vec4f positionRadiusType;  // xyz = world position, w = asfloat(f16(radius) | (type << 16) | (quantized falloff << 20))
    Vec4f colorIntensity;      // rgb = color, w = intensity
    Vec4f directionSpotAngles; // xyz = spot direction (normal), w = asfloat(f16(outerAngle) | (f16(innerAngle) << 16))
};

// Must match `FogVolumeEnvProbeBakeData` in Source/Shaders/Deferred/FogVolumeOcclusionBake.hlsl
struct FogVolumeEnvProbeGpuData
{
    Vec4f positionRadius; // xyz = world position, w = influence radius
    Vec4f ambientColor;   // rgb = ambient irradiance (SH DC term, pre-scaled), w = unused
};

// Must match the `CBuffer` layout in Source/Shaders/Deferred/FogVolumeOcclusionBake.hlsl
struct FogVolumeOcclusionBakeConstants
{
    Vec4u dimensionsAndNumLights; // xyz = volume dimensions, w = number of lights
    Vec4f aabbMin;
    Vec4f aabbMax;
    Vec4f samplesAndRadiusScaleAndNumEnvProbes; // x = numSamples, y = source radius scale, z = numEnvProbes, w = unused
};

#pragma region Render command

struct FogVolumeOcclusionBakePayload
{
    BakeJob<FogVolume>* job;
    Handle<Texture> sdfTexture;
    StructuredBuffer lightsBuffer;
    StructuredBuffer envProbesBuffer;
    RWStructuredBuffer outputBuffer;
    GpuBufferRef constantsBuffer;
    Vec3u dimensions;
    uint32 numLights;
    uint32 numEnvProbes;
    Vec3f aabbMin;
    Vec3f aabbMax;
};

class FogVolumeOcclusionBakeCmd : public CmdBase
{
public:
    FogVolumeOcclusionBakePayload* payload;

    explicit FogVolumeOcclusionBakeCmd(FogVolumeOcclusionBakePayload* payload)
        : payload(payload)
    {
    }

    static void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
    {
        FogVolumeOcclusionBakeCmd* cmdCasted = static_cast<FogVolumeOcclusionBakeCmd*>(cmd);
        FogVolumeOcclusionBakePayload* payload = cmdCasted->payload;

        CommandRecorder& cr = RI.commandRecorderAllocator.GetCommandRecorder();

        cr << SetCurrentShader(ShaderDesc(NAME("FogVolumeOcclusionBake")));

        cr << SetShaderUniform(0, "OcclusionSDF"_sh, RI.textureViewCache->GetOrCreate(payload->sdfTexture.Get()));
        cr << SetShaderUniform(1, "SamplerLinear"_sh, RI.placeholderData->GetSamplerLinear());
        cr << SetShaderUniform(2, "Lights"_sh, payload->lightsBuffer);
        cr << SetShaderUniform(3, "EnvProbes"_sh, payload->envProbesBuffer);
        cr << SetShaderUniform(4, "OutputBuffer"_sh, payload->outputBuffer);

        FogVolumeOcclusionBakeConstants constants {};
        constants.dimensionsAndNumLights = Vec4u(payload->dimensions.x, payload->dimensions.y, payload->dimensions.z, payload->numLights);
        constants.aabbMin = Vec4f(payload->aabbMin, 0.0f);
        constants.aabbMax = Vec4f(payload->aabbMax, 0.0f);
        constants.samplesAndRadiusScaleAndNumEnvProbes = Vec4f(float(NumPointLightShadowSamples), PointLightSourceRadiusScale, float(payload->numEnvProbes), 0.0f);

        payload->constantsBuffer = RI.MakeGpuBuffer(GpuBufferType::ConstantBuffer, sizeof(constants));

        Check(payload->constantsBuffer->Create());

        payload->constantsBuffer->Copy(sizeof(constants), &constants);
        payload->constantsBuffer->Flush(0, sizeof(constants));

        cr << SetShaderUniform(5, "CBuffer"_sh, payload->constantsBuffer);

        const Vec3u groupCount {
            (payload->dimensions.x + 3) / 4,
            (payload->dimensions.y + 3) / 4,
            (payload->dimensions.z + 3) / 4
        };

        cr << InsertBarrier(payload->outputBuffer.gpuBuffer, RS_UNORDERED_ACCESS, ShaderModuleType::Compute);

        cr << DispatchCompute(groupCount);

        cr << InsertBarrier(payload->outputBuffer.gpuBuffer, RS_COPY_SRC, ShaderModuleType::Compute);

        GpuBufferRef readbackBuffer = RI.MakeGpuBuffer(GpuBufferType::ReadbackBuffer, payload->outputBuffer.gpuBuffer->Size());
        readbackBuffer->SetIsCpuAccessible(true);
        Check(readbackBuffer->Create());

        cr << InsertBarrier(readbackBuffer, RS_COPY_DST, ShaderModuleType::Compute);
        cr << CopyBuffer(payload->outputBuffer.gpuBuffer, readbackBuffer, payload->outputBuffer.gpuBuffer->Size());

        cr.Submit();

        struct ReadbackPayload
        {
            BakeJob<FogVolume>* job;
            Handle<Texture> sdfTexture;
            GpuBuffer* lightsBuffer;
            GpuBuffer* outputBuffer;
            GpuBufferRef readbackBuffer;
            uint32 numResults;
        };

        ReadbackPayload* readbackPayload = new ReadbackPayload;
        readbackPayload->job = payload->job;
        readbackPayload->sdfTexture = payload->sdfTexture;
        readbackPayload->lightsBuffer = payload->lightsBuffer.gpuBuffer;
        readbackPayload->outputBuffer = payload->outputBuffer.gpuBuffer;
        readbackPayload->readbackBuffer = std::move(readbackBuffer);
        readbackPayload->numResults = payload->dimensions.x * payload->dimensions.y * payload->dimensions.z;

        Frame* frame = RI.GetCurrentFrame();
        Assert(frame != nullptr);

        // Readback happens after the frame is finished.
        frame->OnFrameEnd.Bind(
            [readbackPayload, payload](...)
            {
                BakeJob<FogVolume>* job = readbackPayload->job;

                job->m_gpuResults.Resize(readbackPayload->numResults);

                // GPU writes are not guaranteed to be visible to the CPU until the range is invalidated
                readbackPayload->readbackBuffer->Invalidate();

                readbackPayload->readbackBuffer->Read(readbackPayload->numResults * sizeof(Vec4f), job->m_gpuResults.Data());

                EnqueueDeletion(std::move(readbackPayload->sdfTexture));
                EnqueueDeletion(std::move(readbackPayload->readbackBuffer));

                job->m_gpuBakeReady.Set(true, MemoryOrder::RELEASE);
                job->m_gpuBakeReady.NotifyAll();

                delete readbackPayload;
                delete payload;
            })
            .Detach();

        cmdCasted->payload = nullptr;
    }
};

#pragma endregion Render command

BakeJob<FogVolume>::~BakeJob()
{
    if (m_gpuBakeDispatched.Get(MemoryOrder::ACQUIRE))
    {
        while (!m_gpuBakeReady.Get(MemoryOrder::ACQUIRE))
        {
            m_gpuBakeReady.Wait(false, MemoryOrder::ACQUIRE);
        }
    }
}

void BakeJob<FogVolume>::Start_Internal()
{
    const typename BakeData<FogVolume>::VolumeBitmap& volumeBitmap = m_bakeData->GetVolumeBitmap();

    const Vec3u volumeExtent = Vec3u {
        volumeBitmap.GetWidth(),
        volumeBitmap.GetHeight(),
        volumeBitmap.GetDepth()
    };

    // Flatten texel indices for processing
    m_texelIndices.Resize(volumeExtent.x * volumeExtent.y * volumeExtent.z);

    for (uint32 z = 0; z < volumeExtent.z; ++z)
    {
        for (uint32 y = 0; y < volumeExtent.y; ++y)
        {
            for (uint32 x = 0; x < volumeExtent.x; ++x)
            {
                const uint32 texelIndex = z * (volumeExtent.x * volumeExtent.y) + y * volumeExtent.x + x;

                m_texelIndices[texelIndex] = texelIndex;
            }
        }
    }
}

void BakeJob<FogVolume>::DispatchOcclusionBake()
{
    const typename BakeData<FogVolume>::OccSdfBitmap& sdfBitmap = m_bakeData->GetOccSdfBitmap();

    TextureDesc sdfTextureDesc {
        TextureType::Texture3D,
        sdfBitmap.GetFormat(),
        Vec3u { sdfBitmap.GetWidth(), sdfBitmap.GetHeight(), sdfBitmap.GetDepth() },
        TFM_LINEAR,
        TFM_LINEAR,
        TWM_CLAMP_TO_EDGE
    };

    Handle<Texture> sdfTexture = MakeHandle<Texture>(sdfTextureDesc, sdfBitmap.ToByteView());
    Check(sdfTexture->Create());

    Array<FogVolumeLightGpuData> lightData;

    for (const Handle<Light>& light : m_bakeData->GetLights())
    {
        if (!light.IsValid())
        {
            continue;
        }

        const LightType lightType = light->GetLightType();

        Vec3f direction = Vec3f::Zero();
        Vec2f spotAngles = Vec2f::Zero();

        if (lightType == LightType::Spot)
        {
            direction = light->GetNormal();
            spotAngles = light->GetSpotAngles();
        }

        const Color& color = light->GetColor();

        const uint32 falloffQuantized = uint32(MathUtil::Clamp(light->GetFalloff() / MaxLightFalloffExponent, 0.0f, 1.0f) * 4095.0f + 0.5f);

        FogVolumeLightGpuData entry {};
        entry.positionRadiusType = Vec4f(light->GetWorldTranslation(), 0.0f);
        entry.positionRadiusType.w = std::bit_cast<float>(
            uint32(Float16(light->GetRadius()).Raw())
            | ((uint32(lightType) & 0xFu) << 16)
            | ((falloffQuantized & 0xFFFu) << 20));

        entry.directionSpotAngles = Vec4f(direction, 0.0f);
        entry.directionSpotAngles.w = std::bit_cast<float>(PackHalf2(spotAngles.x, spotAngles.y));

        entry.colorIntensity = Vec4f(color.GetRed(), color.GetGreen(), color.GetBlue(), light->GetIntensity());

        lightData.PushBack(entry);
    }

    Array<FogVolumeEnvProbeGpuData> envProbeData;

    for (const Handle<EnvProbe>& envProbe : m_bakeData->GetEnvProbes())
    {
        if (!envProbe.IsValid())
        {
            continue;
        }

        const BoundingBox worldBounds = envProbe->GetWorldBounds();

        if (!worldBounds.IsValid() || !worldBounds.IsFinite() || worldBounds.IsZero())
        {
            continue;
        }

        const SphericalHarmonicsData& shData = envProbe->GetSphericalHarmonicsData();

        FogVolumeEnvProbeGpuData entry {};
        entry.positionRadius = Vec4f(worldBounds.GetCenter(), worldBounds.GetExtent().Length() * 0.5f);
        entry.ambientColor = Vec4f(shData.values[0], shData.values[1], shData.values[2], 0.0f) * ShBand0Normalization;

        envProbeData.PushBack(entry);
    }

    const typename BakeData<FogVolume>::VolumeBitmap& volumeBitmap = m_bakeData->GetVolumeBitmap();
    const Vec3u dims = Vec3u { volumeBitmap.GetWidth(), volumeBitmap.GetHeight(), volumeBitmap.GetDepth() };
    const uint32 numTexels = dims.x * dims.y * dims.z;

    const BoundingBox worldAabb = m_fogVolume->GetWorldBounds();

    FogVolumeOcclusionBakePayload* payload = new FogVolumeOcclusionBakePayload;
    payload->job = this;
    payload->sdfTexture = sdfTexture;
    payload->dimensions = dims;
    payload->numLights = uint32(lightData.Size());
    payload->numEnvProbes = uint32(envProbeData.Size());
    payload->aabbMin = worldAabb.GetMin();
    payload->aabbMax = worldAabb.GetMax();

    payload->lightsBuffer = StructuredBuffer(MathUtil::Max<size_t>(lightData.Size(), 1), sizeof(FogVolumeLightGpuData));
    payload->lightsBuffer.Initialize();

    if (lightData.Any())
    {
        payload->lightsBuffer.Write(0, lightData.ByteSize(), lightData.Data());
        payload->lightsBuffer.Flush();
    }

    payload->envProbesBuffer = StructuredBuffer(MathUtil::Max<size_t>(envProbeData.Size(), 1), sizeof(FogVolumeEnvProbeGpuData));
    payload->envProbesBuffer.Initialize();

    if (envProbeData.Any())
    {
        payload->envProbesBuffer.Write(0, envProbeData.ByteSize(), envProbeData.Data());
        payload->envProbesBuffer.Flush();
    }

    payload->outputBuffer = RWStructuredBuffer(numTexels, sizeof(Vec4f));
    payload->outputBuffer.Initialize();

    CommandRecorder& cr = RI.commandRecorderAllocator.GetCommandRecorder();
    cr << FogVolumeOcclusionBakeCmd(payload);
    cr.Done();
}

void BakeJob<FogVolume>::Process_Internal(bool* outIsReadyToProcess)
{
    if (!m_gpuBakeDispatched.Get(MemoryOrder::ACQUIRE))
    {
        m_gpuBakeDispatched.Set(true, MemoryOrder::RELEASE);

        DispatchOcclusionBake();

        if (outIsReadyToProcess)
        {
            *outIsReadyToProcess = false;
        }

        return;
    }

    if (!m_gpuBakeReady.Get(MemoryOrder::ACQUIRE))
    {
        if (outIsReadyToProcess)
        {
            *outIsReadyToProcess = false;
        }

        return;
    }

    if (outIsReadyToProcess)
    {
        *outIsReadyToProcess = true;
    }
}

uint32 BakeJob<FogVolume>::ProcessTexels(Span<LightmapTexel*> texels, uint32 texelOffset)
{
    for (uint32 txlIdx = 0; txlIdx < uint32(texels.Size()); ++txlIdx)
    {
        LightmapTexel* texel = texels[txlIdx];
        const uint32 realTexelIndex = texelOffset + txlIdx;

        const Vec4f pointLightData = realTexelIndex < uint32(m_gpuResults.Size())
            ? m_gpuResults[realTexelIndex]
            : Vec4f(0.0f, 0.0f, 0.0f, 1.0f);

        texel->color0.x = pointLightData.x;
        texel->color0.y = pointLightData.y;
        texel->color0.z = pointLightData.z;
        texel->color0.w = pointLightData.w;
    }

    return uint32(texels.Size());
}

} // namespace Baking
} // namespace Hyperion
