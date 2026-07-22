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

#include <Scene/Util/VoxelOctree.hpp>

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

// Must match `PointLightBakeData` in Source/Shaders/Deferred/FogVolumeOcclusionBake.hlsl
struct FogVolumePointLightGpuData
{
    Vec4f positionRadius; // xyz = world position, w = falloff radius
    Vec4f colorIntensity; // rgb = color, w = intensity
};

// Must match the `CBuffer` layout in Source/Shaders/Deferred/FogVolumeOcclusionBake.hlsl
struct FogVolumeOcclusionBakeConstants
{
    Vec4u dimensionsAndNumLights; // xyz = volume dimensions, w = number of point lights
    Vec4f aabbMin;
    Vec4f aabbMax;
    Vec4f samplesAndRadiusScale; // x = numSamples, y = source radius scale, zw = unused
};

#pragma region Render command

struct FogVolumeOcclusionBakePayload
{
    BakeJob<FogVolume>* job;
    Handle<Texture> sdfTexture;
    StructuredBuffer lightsBuffer;
    RWStructuredBuffer outputBuffer;
    GpuBufferRef constantsBuffer;
    Vec3u dimensions;
    uint32 numLights;
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
        cr << SetShaderUniform(2, "PointLights"_sh, payload->lightsBuffer);
        cr << SetShaderUniform(3, "OutputBuffer"_sh, payload->outputBuffer);

        FogVolumeOcclusionBakeConstants constants {};
        constants.dimensionsAndNumLights = Vec4u(payload->dimensions.x, payload->dimensions.y, payload->dimensions.z, payload->numLights);
        constants.aabbMin = Vec4f(payload->aabbMin, 0.0f);
        constants.aabbMax = Vec4f(payload->aabbMax, 0.0f);
        constants.samplesAndRadiusScale = Vec4f(float(NumPointLightShadowSamples), PointLightSourceRadiusScale, 0.0f, 0.0f);

        payload->constantsBuffer = RI.MakeGpuBuffer(GpuBufferType::ConstantBuffer, sizeof(constants));

        Check(payload->constantsBuffer->Create());
        
        payload->constantsBuffer->Copy(sizeof(constants), &constants);
        payload->constantsBuffer->Flush(0, sizeof(constants));

        cr << SetShaderUniform(4, "CBuffer"_sh, payload->constantsBuffer);

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

    Array<FogVolumePointLightGpuData> lightData;

    for (const Handle<Light>& light : m_bakeData->GetPointLights())
    {
        if (!light.IsValid())
        {
            continue;
        }

        const Color& color = light->GetColor();

        FogVolumePointLightGpuData entry {};
        entry.positionRadius = Vec4f(light->GetWorldTranslation(), light->GetRadius());
        entry.colorIntensity = Vec4f(color.GetRed(), color.GetGreen(), color.GetBlue(), light->GetIntensity());

        lightData.PushBack(entry);
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
    payload->aabbMin = worldAabb.GetMin();
    payload->aabbMax = worldAabb.GetMax();

    payload->lightsBuffer = StructuredBuffer(MathUtil::Max<size_t>(lightData.Size(), 1), sizeof(FogVolumePointLightGpuData));
    payload->lightsBuffer.Initialize();

    if (lightData.Any())
    {
        payload->lightsBuffer.Write(0, lightData.ByteSize(), lightData.Data());
        payload->lightsBuffer.Flush();
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
