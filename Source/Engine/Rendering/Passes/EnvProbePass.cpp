/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <Rendering/Passes/EnvProbePass.hpp>
#include <Rendering/Passes/DeferredPass.hpp>

#include <Rendering/ShaderManager.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/RenderInterface.hpp>
#include <Rendering/Frame.hpp>
#include <Rendering/GpuImage.hpp>
#include <Rendering/GpuImageView.hpp>
#include <Rendering/GpuBuffer.hpp>
#include <Rendering/DescriptorSet.hpp>
#include <Rendering/ComputePipeline.hpp>
#include <Rendering/RenderTypes.hpp>
#include <Rendering/RenderProxyList.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/AsyncCompute.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/TextureViewCache.hpp>
#include <Rendering/ShaderInstance.hpp>
#include <Rendering/RendererMain.hpp>
#include <Rendering/RenderHelpers.hpp>
#include <Rendering/ScratchImageAllocator.hpp>
#include <Rendering/CBufferAllocator.hpp>

#include <Rendering/Shadows/ShadowMapCache.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Scene/View.hpp>
#include <Scene/EnvProbe.hpp>
#include <Scene/Light.hpp>

#include <Framework/EngineStats.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Utilities/DeferredScope.hpp>

#include <Util/Img/Bitmap.hpp>

#include <HyperionEngine.hpp>

#include <EnvProbePass.generated.inl>

namespace Hyperion {

static constexpr Vec2u ShNumSamples = { 16, 16 };
static constexpr Vec2u ShNumTiles = { 16, 16 };
static constexpr uint32 ShNumLevels = MathUtil::Max(1u, uint32(MathUtil::FastLog2(ShNumSamples.Max()) + 1));
static constexpr bool ShParallelReduce = false;

static EngineStatGpuTimer s_statDrawEnvProbe("Rendering/GPU/DrawEnvProbe");
static EngineStatGpuTimer s_statConvolveEnvProbe("Rendering/GPU/ConvolveEnvProbe");
static EngineStatGpuTimer s_statComputeEnvProbeSH("Rendering/GPU/ComputeEnvProbeSH");

#pragma region ConvolveProbe

namespace ConvolveProbe {

struct ConvolveProbeConstants
{
    Vec2u outImageDimensions;
    Vec2u inImageDimensions;
};

void ConvolveEnvProbeCubemap(
    const Handle<Texture>& inTexture,
    const EnvProbe& envProbe)
{
    Assert(inTexture != nullptr);

    HYP_LOG(Rendering, Info, "Convolve probe {}", envProbe.GetName());

    // Alloc command recorder
    // we need to do this after we Create() the src texture,
    // because CreateGpuImage in Texture.cpp creates its own command recorder,
    // so we need that one to run before this one.
    CommandRecorder& cr = RI.commandRecorderAllocator.GetCommandRecorder();
    HYP_DEFER({ cr.Done(); });

    ENGINE_STAT_GPU_SCOPE(&s_statConvolveEnvProbe, &cr);

    Handle<Texture> prefilteredEnvMap = envProbe.GetPrefilteredEnvMap();
    Assert(prefilteredEnvMap.IsValid() && prefilteredEnvMap->IsCreated());

    Handle<Texture> srcTexture;
    bool needsMipMapGeneration = false;

    Handle<Texture> dstTexture = RI.scratchImageAllocator->AcquireScratchImage(
        TextureType::Cubemap,
        prefilteredEnvMap->GetFormat(),
        prefilteredEnvMap->GetExtent());

    if (inTexture->HasMipMaps())
    {
        srcTexture = inTexture;
    }
    else
    {
        needsMipMapGeneration = true;

        // copy into new texture, we need to generate mips on it before convolving
        srcTexture = RI.scratchImageAllocator->AcquireScratchImage(
            TextureType::Cubemap,
            prefilteredEnvMap->GetFormat(),
            inTexture->GetExtent());
    }

    ConvolveProbeConstants constants {};
    constants.inImageDimensions = inTexture->GetExtent().GetXY();

    const Vec2u extent = envProbe.GetDimensions();
    const uint8 numMips = uint8(MathUtil::FastLog2(MathUtil::Max(extent.x, extent.y))) + 1;

    if (needsMipMapGeneration)
    { // Blit into mip 0 of the source texture
        Texture* dst = srcTexture;
        Texture* src = inTexture;

        ImageSubResource subResource {};
        subResource.baseMipLevel = 0;
        subResource.numLevels = 1;
        subResource.baseArrayLayer = 0;
        subResource.numLayers = 6;

        cr << InsertBarrier(src->GetGpuImage(), RS_COPY_SRC, subResource);
        cr << InsertBarrier(dst->GetGpuImage(), RS_COPY_DST, subResource);

        const Vec3u srcMipExtent = src->GetTextureDesc().extent;
        const Vec3u dstMipExtent = dst->GetTextureDesc().extent;

        if (srcMipExtent == dstMipExtent && src->GetTextureDesc().format == dst->GetTextureDesc().format)
        {
            cr << CopyImage(src->GetGpuImage(), dst->GetGpuImage(), srcMipExtent);
        }
        else
        {
            cr << Blit(
                src,
                dst,
                Rect<uint32> { 0, 0, srcMipExtent.x, srcMipExtent.y },
                Rect<uint32> { 0, 0, dstMipExtent.x, dstMipExtent.y });
        }

        // back to shader resource state.
        cr << InsertBarrier(src->GetGpuImage(), RS_SHADER_RESOURCE, subResource);

        // put ALL the remaining mips of dstImage into copy dst so we can generate mips on it
        cr << InsertBarrier(dst->GetGpuImage(), RS_COPY_DST);

        // generate mips before running convolve shader using it as a source
        cr << GenerateMipmaps(dst);

        cr << InsertBarrier(src->GetGpuImage(), RS_SHADER_RESOURCE);
    }

    GpuImageViewRef srcImageView = RI.textureViewCache->GetOrCreate(srcTexture);

    for (uint8 mipIndex = 0; mipIndex < numMips; mipIndex++)
    {
        const float roughness = float(mipIndex) / float(numMips - 1);

        const Vec2u mipExtent = mipIndex == 0
            ? extent
            : Vec2u(MathUtil::Max(extent.x >> mipIndex, 1u), MathUtil::Max(extent.y >> mipIndex, 1u));

        ShaderPropertySet shaderProperties;
        // we have to round otherwise we'll potentially make too many permutations for *almost* the same values.
        shaderProperties.Add(InternShaderProperty(ShaderProperty(NAME("LOBE_SIZE"), MathUtil::Round(roughness, 3))));
        shaderProperties.Add(InternShaderProperty(ShaderProperty(NAME("NUM_SAMPLES"), 256)));

        cr << SetCurrentShader(ShaderDesc(NAME("ConvolveProbe"), shaderProperties));



        CBufferAllocator& cba = *RI.cbufferAllocator;

        GpuBuffer* cbuffer = nullptr;
        size_t cbufferSize = 0;
        size_t cbufferOffset = 0;

        constants.outImageDimensions = mipExtent;

        cba.Write(&constants);
        cba.Commit(cbuffer, cbufferOffset, cbufferSize);

        ImageSubResource subResource {};
        subResource.baseMipLevel = mipIndex;
        subResource.numLevels = 1;
        subResource.baseArrayLayer = 0;
        subResource.numLayers = 6;

        GpuImageViewRef dstImageView = RI.textureViewCache->GetOrCreate(
            dstTexture, subResource, TextureType::Texture2DArray);

        Assert(dstImageView.IsValid() && srcImageView.IsValid());

        cr << InsertBarrier(dstTexture->GetGpuImage(), RS_UNORDERED_ACCESS, subResource);

        // @TODO Just write the env probe to constant buffer?
        cr << SetShaderUniform(0, "CurrentEnvProbe"_sh, RI.namedBuffers[NamedBuffer::EnvProbes], Resources::GetBinding(&envProbe));
        cr << SetShaderUniform(1, "SphereSamplesBuffer"_sh, RI.sphereSamplesBuffer);
        cr << SetShaderUniform(2, "ColorTexture"_sh, srcImageView);
        cr << SetShaderUniform(3, "SamplerLinear"_sh, RI.placeholderData->GetSamplerLinear());
        cr << SetShaderUniform(4, "SamplerNearest"_sh, RI.placeholderData->GetSamplerNearest());
        cr << SetShaderUniform(5, "OutImage"_sh, dstImageView);
        cr << SetShaderUniform(6, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

        cr << DispatchCompute(Vec3u { (mipExtent.x + 7) / 8, (mipExtent.y + 7) / 8, 6 });

        cr << InsertBarrier(dstTexture->GetGpuImage(), RS_COPY_SRC, subResource);
        cr << InsertBarrier(prefilteredEnvMap->GetGpuImage(), RS_COPY_DST, subResource);

        cr << CopyImage(dstTexture->GetGpuImage(), prefilteredEnvMap->GetGpuImage(),
            Vec3u::Zero(), Vec3u::Zero(),
            Vec3u(mipExtent, 1),
            subResource, subResource);

        // put prefiltered map back into shader read
        cr << InsertBarrier(prefilteredEnvMap->GetGpuImage(), RS_SHADER_RESOURCE, subResource);
    }

    // readback on completion and write to cpu-side data if probe is baked
    if (envProbe.IsBaked())
    {
        HYP_LOG(Rendering, Verbose, "Enquueing readback of convolved EnvProbe {}.", envProbe.GetName());
        prefilteredEnvMap->EnqueueReadback([envProbeWeak = MakeWeakRef(&envProbe), prefilteredEnvMap](GpuBuffer& buffer)
        {
            Handle<EnvProbe> envProbeStrong = envProbeWeak.Lock();
            if (!envProbeStrong.IsValid())
            {
                HYP_LOG(Rendering, Warning, "EnvProbe was destroyed before readback of convolved data completed, skipping write to cpu-side data.");
                return;
            }

            HYP_LOG(Rendering, Info, "Readback of convolved EnvProbe {} completed, size {} bytes", envProbeStrong->GetName(), buffer.Size());

            auto textureWriteScope = prefilteredEnvMap->GetWriteScope();

            TextureDesc desc = prefilteredEnvMap->GetTextureDesc();
            AssertDebug(desc.extent.Volume() != 0 && desc.extent.Volume() <= 2048*2048);

            // sanity check
            Assert(buffer.Size() == desc.GetByteSize(/* allMips */ true));

            ConstByteView view;
            view.first = static_cast<const ubyte*>(buffer.Map());
            view.last = view.first + buffer.Size();

            // set all mip offsets.
            desc.mipOffsets = {};

            const uint8 numMips = desc.NumMips();

            size_t mipOffset = 0;
            for (uint8 mipIndex = 0; mipIndex < numMips; mipIndex++)
            {
                const size_t mipByteSize = desc.GetMipByteSize(mipIndex, /* includeArrayLayers */ true);

                if (mipIndex > 0)
                {
                    desc.mipOffsets[mipIndex - 1] = uint32(mipOffset);
                }

                mipOffset += mipByteSize;
            }

            // Update image data and desc
            prefilteredEnvMap->SetTextureDesc(desc);
            prefilteredEnvMap->SetImageData(view);

            textureWriteScope.Reset();

            auto envProbeWriteScope = envProbeStrong->GetWriteScope();
            envProbeStrong->SetBakedTexture(prefilteredEnvMap);
        }, /* allMips */ true);
    }

    // Update in env probes texture array if bound
    if (envProbe.IsA<SkyProbe>() || envProbe.IsA<ReflectionProbe>())
    {
        const uint32 boundIndex = Resources::GetBinding(&envProbe);

        if (boundIndex != ~0u)
        {
            cr << InsertBarrier(prefilteredEnvMap->GetGpuImage(), RS_COPY_SRC);
            cr << InsertBarrier(RI.envProbesTexture->GetGpuImage(), RS_COPY_DST);

            const uint8 numMips = MathUtil::Min(
                RI.envProbesTexture->GetTextureDesc().NumMips(),
                prefilteredEnvMap->GetTextureDesc().NumMips());

            for (uint8 mipIndex = 0; mipIndex < numMips; mipIndex++)
            {
                ImageSubResource srcSubResource {};
                srcSubResource.baseMipLevel = mipIndex;
                srcSubResource.numLevels = 1;
                srcSubResource.baseArrayLayer = 0;
                srcSubResource.numLayers = 6;

                ImageSubResource dstSubResource {};
                dstSubResource.baseMipLevel = mipIndex;
                dstSubResource.numLevels = 1;
                dstSubResource.baseArrayLayer = 6 * boundIndex;
                dstSubResource.numLayers = 6;

                const Vec3u srcMipExtent = prefilteredEnvMap->GetTextureDesc().GetMipExtent(mipIndex);
                const Vec3u dstMipExtent = RI.envProbesTexture->GetTextureDesc().GetMipExtent(mipIndex);

                if (srcMipExtent == dstMipExtent && prefilteredEnvMap->GetTextureDesc().format == RI.envProbesTexture->GetTextureDesc().format)
                {
                    cr << CopyImage(
                        prefilteredEnvMap->GetGpuImage(),
                        RI.envProbesTexture->GetGpuImage(),
                        srcMipExtent,
                        srcSubResource,
                        dstSubResource);
                }
                else
                {
                    cr << Blit(
                        prefilteredEnvMap,
                        RI.envProbesTexture,
                        Rect<uint32> { 0, 0, srcMipExtent.x, srcMipExtent.y },
                        Rect<uint32> { 0, 0, dstMipExtent.x, dstMipExtent.y },
                        srcSubResource,
                        dstSubResource);
                }
            }

            cr << InsertBarrier(prefilteredEnvMap->GetGpuImage(), RS_SHADER_RESOURCE);
            cr << InsertBarrier(RI.envProbesTexture->GetGpuImage(), RS_SHADER_RESOURCE);
        }
    }
}

} // namespace ConvolveProbe

#pragma endregion ConvolveProbe

#pragma region ComputeSH

namespace ComputeSH {

constexpr bool UseAsyncCompute = false;

void ComputeEnvProbeSphericalHarmonics(
    const EnvProbe& envProbe,
    const Texture& inColorTexture)
{
    bool useAsyncCompute = UseAsyncCompute;
    if (!IsOnThread(g_renderThread))
    {
        useAsyncCompute = false;
    }

    AsyncCompute* asyncCompute = useAsyncCompute ? RI.CreateAsyncCompute() : nullptr;

    CommandRecorder& cr = useAsyncCompute ? asyncCompute->cr : RI.commandRecorderAllocator.GetCommandRecorder();

    {
        ENGINE_STAT_GPU_SCOPE(&s_statComputeEnvProbeSH, &cr);

        FixedArray<RWStructuredBuffer, ShNumLevels> shTilesBuffers;

        for (uint32 i = 0; i < ShNumLevels; i++)
        {
            shTilesBuffers[i] = RWStructuredBuffer((ShNumTiles.x >> i) * (ShNumTiles.y >> i), sizeof(SHTile));
            shTilesBuffers[i].Initialize();
        }

        const Vec2u cubemapDimensions = inColorTexture.GetExtent().GetXY();

        struct ComputeSHConstants
        {
            Vec4u levelDimensions;
        };

        ComputeSHConstants constants {};

        static constexpr uint32 ShDataSize = sizeof(EnvProbeShaderData::shData);

        GpuBufferRef shBuffer = RI.MakeGpuBuffer(GpuBufferType::RWStructuredBuffer, MathUtil::NextPowerOf2(ShDataSize));
        CheckResult(shBuffer->Create());

        cr << InsertBarrier(shTilesBuffers[0].gpuBuffer, RS_UNORDERED_ACCESS, ShaderModuleType::Compute);
        cr << InsertBarrier(shBuffer, RS_UNORDERED_ACCESS, ShaderModuleType::Compute);

        ShaderPropertySet shaderProperties;

        // Helper to run pass
        auto RunPass = [&](Name mode, const ComputeSHConstants& passConstants, const Vec3u& dispatchGroupSize, const StructuredBuffer& inputBuffer, const StructuredBuffer& outputBuffer)
        {
            ShaderPropertySet passShaderProperties;
            passShaderProperties.Add(InternShaderProperty(ShaderProperty(NAME("MODE"), mode)));
            passShaderProperties = passShaderProperties | shaderProperties;

            ShaderDesc shaderDesc(NAME("ComputeSH"), passShaderProperties);
            cr << SetCurrentShader(shaderDesc);

            CBufferAllocator& cba = *RI.cbufferAllocator;

            GpuBuffer* cbuffer = nullptr;
            size_t cbufferOffset = 0;
            size_t cbufferSize = 0;

            cba.Write(&passConstants);
            cba.Commit(cbuffer, cbufferOffset, cbufferSize);

            cr << SetShaderUniform(0, "SamplerLinear"_sh, RI.placeholderData->GetSamplerLinear());
            cr << SetShaderUniform(1, "SamplerNearest"_sh, RI.placeholderData->GetSamplerNearest());
            cr << SetShaderUniform(3, "EnvProbesBuffer"_sh, RI.namedBuffers[NamedBuffer::EnvProbes]);

            cr << SetShaderUniform(4, "OutSHBuffer"_sh, shBuffer, ShaderDataOffset(0, sizeof(Vec4f)));

            cr << SetShaderUniform(8, "InColorCubemap"_sh, RI.textureViewCache->GetOrCreate(const_cast<Texture*>(&inColorTexture)));
            cr << SetShaderUniform(11, "InputSHTilesBuffer"_sh, inputBuffer);
            cr << SetShaderUniform(12, "OutputSHTilesBuffer"_sh, outputBuffer);
            cr << SetShaderUniform(13, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

            cr << DispatchCompute(dispatchGroupSize);
        };

        // MODE_CLEAR
        RunPass(NAME("CLEAR"), constants, Vec3u { 1, 1, 1 }, shTilesBuffers[0], shTilesBuffers[1]);

        cr << InsertBarrier(shTilesBuffers[0].gpuBuffer, RS_UNORDERED_ACCESS, ShaderModuleType::Compute);

        // MODE_BUILD_COEFFICIENTS
        RunPass(NAME("BUILD_COEFFICIENTS"), constants, Vec3u { 1, 1, 1 }, shTilesBuffers[0], shTilesBuffers[1]);

        // Parallel reduce
        if (ShParallelReduce)
        {
            for (uint32 i = 1; i < ShNumLevels; i++)
            {
                cr << InsertBarrier(shTilesBuffers[i - 1].gpuBuffer, RS_UNORDERED_ACCESS, ShaderModuleType::Compute);

                const Vec2u prevDimensions {
                    MathUtil::Max(1u, ShNumSamples.x >> (i - 1)),
                    MathUtil::Max(1u, ShNumSamples.y >> (i - 1))
                };

                const Vec2u nextDimensions {
                    MathUtil::Max(1u, ShNumSamples.x >> i),
                    MathUtil::Max(1u, ShNumSamples.y >> i)
                };

                Assert(prevDimensions.x >= 2);
                Assert(prevDimensions.x > nextDimensions.x);
                Assert(prevDimensions.y > nextDimensions.y);

                ComputeSHConstants reducePassConstants = constants;
                reducePassConstants.levelDimensions = {
                    prevDimensions.x,
                    prevDimensions.y,
                    nextDimensions.x,
                    nextDimensions.y
                };

                RunPass(
                    NAME("REDUCE"),
                    reducePassConstants,
                    Vec3u { 1, (nextDimensions.x + 3) / 4, (nextDimensions.y + 3) / 4 },
                    shTilesBuffers[i - 1],
                    shTilesBuffers[i]);
            }
        }

        const uint32 finalizeShBufferIndex = ShParallelReduce ? ShNumLevels - 1 : 0;

        // Finalize - build into final buffer
        cr << InsertBarrier(shTilesBuffers[finalizeShBufferIndex].gpuBuffer, RS_UNORDERED_ACCESS, ShaderModuleType::Compute);
        cr << InsertBarrier(shBuffer, RS_UNORDERED_ACCESS, ShaderModuleType::Compute);

        // MODE_FINALIZE
        RunPass(
            NAME("FINALIZE"),
            constants,
            Vec3u { 1, 1, 1 },
            shTilesBuffers[finalizeShBufferIndex],
            shTilesBuffers[finalizeShBufferIndex]);

        cr << InsertBarrier(shBuffer, RS_COPY_SRC, ShaderModuleType::Compute);

        GpuBufferRef readbackBuffer = RI.MakeGpuBuffer(GpuBufferType::ReadbackBuffer, shBuffer->Size());
        readbackBuffer->SetIsCpuAccessible(true);
#if HYP_DEBUG_MODE
        readbackBuffer->SetDebugName(NAME("ComputeEnvProbeSphericalHarmonics_ReadbackBuffer"));
#endif // HYP_DEBUG_MODE
        CheckResult(readbackBuffer->Create());

        // Copy to readback buffer
        cr << InsertBarrier(readbackBuffer, RS_COPY_DST, ShaderModuleType::Compute);
        cr << CopyBuffer(shBuffer, readbackBuffer, shBuffer->Size());

        struct ReadbackSphericalHarmonicsPayload
        {
            Handle<EnvProbe> envProbe;
            GpuBufferRef shBuffer;
            GpuBufferRef readbackBuffer;
            FixedArray<RWStructuredBuffer, ShNumLevels> shTilesBuffers;
        };

        // Custom CmdBase class, executes when all previous commands are done.
        // Always executes on the Render thread.
        class ReadbackSphericalHarmonics : public CmdBase
        {
        public:
            ReadbackSphericalHarmonicsPayload* payload;

            explicit ReadbackSphericalHarmonics(ReadbackSphericalHarmonicsPayload* payload)
                : payload(payload)
            {
            }

            static void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
            {
                ReadbackSphericalHarmonics* cmdCasted = static_cast<ReadbackSphericalHarmonics*>(cmd);

                Frame* frame = RI.GetCurrentFrame();
                Assert(frame != nullptr);

                // Readback happens after the frame is finished.
                // Hand over the payload to the delegate handler.
                frame->OnFrameEnd.Bind([pPayload = cmdCasted->payload](...)
                                     {
                                         ReadbackSphericalHarmonicsPayload& payload = *pPayload;

                                         // Read back the SH coefficients from the GPU buffer and store on the EnvProbe.
                                         EnvProbeSphericalHarmonics shData {};

                                         Assert(payload.readbackBuffer.IsValid() && payload.readbackBuffer->Size() >= sizeof(shData.values));
                                         payload.readbackBuffer->Read(sizeof(shData.values), &shData.values[0]);

                                         {
                                             // SetSphericalHarmonicsData() marks it dirty so we don't need to do that here.
                                             auto envProbeWriteScope = payload.envProbe->GetWriteScope();
                                             payload.envProbe->SetSphericalHarmonicsData(shData);
                                         }

                                         EnqueueDeletion(std::move(payload.shBuffer));
                                         EnqueueDeletion(std::move(payload.readbackBuffer));

                                         delete pPayload;
                                     })
                    .Detach();

                // not necessary but just to aid in debugging
                cmdCasted->payload = nullptr;
            }
        };

        ReadbackSphericalHarmonicsPayload* payload = new ReadbackSphericalHarmonicsPayload;
        payload->envProbe = MakeStrongRef(&envProbe);
        payload->shBuffer = std::move(shBuffer);
        payload->readbackBuffer = std::move(readbackBuffer);
        payload->shTilesBuffers = std::move(shTilesBuffers);

        cr << ReadbackSphericalHarmonics(payload);
    }

    if (useAsyncCompute)
    {
        RI.SubmitAsyncCompute(asyncCompute);
    }
    else
    {
        cr.Done();
    }
}

} // namespace ComputeSH

#pragma endregion ComputeSH

#pragma region EnvProbePassBase

EnvProbePassBase::EnvProbePassBase()
{
}

EnvProbePassBase::~EnvProbePassBase()
{
}

void EnvProbePassBase::Initialize()
{
}

void EnvProbePassBase::Shutdown()
{
}

void EnvProbePassBase::RenderFrame(Frame* frame, const RenderSetup& renderSetup)
{
    AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.world && renderSetup.envProbe);

    EnvProbe* envProbe = renderSetup.envProbe;
    AssertDebug(envProbe != nullptr);

    RenderSetup rs = renderSetup.Fork();
    rs.envProbe = renderSetup.prev ? renderSetup.prev->envProbe : nullptr;
    rs.viewport = Viewport { envProbe->GetDimensions() };

    ENGINE_STAT_GPU_SCOPE(&s_statDrawEnvProbe, &frame->cr);

    RenderProbe(frame, rs, envProbe);
}

PassData* EnvProbePassBase::CreateViewPassData(View* view, PassDataExt& ext)
{
    EnvProbePassData* pd = new EnvProbePassData();
    pd->view = MakeWeakRef(view);

    return pd;
}

#pragma endregion EnvProbePassBase

#pragma region ReflectionProbePass

ReflectionProbePass::ReflectionProbePass()
{
}

ReflectionProbePass::~ReflectionProbePass()
{
}

void ReflectionProbePass::Initialize()
{
    EnvProbePassBase::Initialize();
}

void ReflectionProbePass::Shutdown()
{
    EnvProbePassBase::Shutdown();
}

void ReflectionProbePass::RenderProbe(Frame* frame, const RenderSetup& renderSetup, EnvProbe* envProbe)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(!envProbe->IsBaked());

    View* firstView = envProbe->GetView(0);
    if (firstView == nullptr)
    {
        return;
    }

    EnvProbePassData* pd = static_cast<EnvProbePassData*>(FetchViewPassData(firstView));
    AssertDebug(pd != nullptr);

    RenderProxyEnvProbe* envProbeProxy = static_cast<RenderProxyEnvProbe*>(GetRenderProxy(envProbe));
    AssertDebug(envProbeProxy != nullptr);

    bool needsRerender = false;

    // special checks for Sky + caching result based on light position + intensity
    if (envProbe->IsA(SkyProbe::StaticClass()))
    {
        if (!renderSetup.light)
        {
            HYP_LOG_ONCE(Rendering, Warning, "No directional light bound while rendering SkyProbe {}", envProbe->Id());

            pd->cachedLightDirIntensity = MathUtil::NaN<Vec4f>();
        }

        RenderProxyLight* lightProxy = static_cast<RenderProxyLight*>(GetRenderProxy(renderSetup.light));
        AssertDebug(lightProxy != nullptr);

#if HYP_DEBUG_MODE
        AssertDebug(Resources::GetBinding(renderSetup.light) != ~0u);
#endif

        if (lightProxy->bufferData.positionIntensity != pd->cachedLightDirIntensity)
        {
            needsRerender = true;
        }

        // cache it to save on rendering later
        pd->cachedLightDirIntensity = lightProxy->bufferData.positionIntensity;
    }
    else if (envProbe->IsA(ReflectionProbe::StaticClass())
        && pd->cachedProbeOrigin == envProbeProxy->bufferData.worldPosition.GetXYZ())
    {
        return;
    }

    pd->cachedProbeOrigin = envProbeProxy->bufferData.worldPosition.GetXYZ();

    bool renderedView = false;

    for (uint8 viewIndex = 0; viewIndex < 6; viewIndex++)
    {
        RenderSetup rs = renderSetup.Fork();
        rs.view = envProbe->GetView(viewIndex);
        rs.framebuffer = envProbe->GetViewFramebuffer(viewIndex);
        rs.passData = pd;

        RenderProxyList& rpl = GetConsumerProxyList(rs.view);
        rpl.BeginRead();
        HYP_DEFER({ rpl.EndRead(); });

        if (!needsRerender
            && !rpl.GetMeshEntities().GetDiff().NeedsUpdate()
            && !rpl.GetLights().GetDiff().NeedsUpdate())
        {
            continue;
        }

        RenderProbeView(frame, rs, envProbe);

        renderedView = true;
    }

    if (!renderedView)
    {
        return;
    }

    if (envProbe->ShouldComputePrefilteredEnvMap())
    {
        ComputePrefilteredEnvMap(frame, renderSetup, envProbe);
    }

    if (envProbe->ShouldComputeSphericalHarmonics())
    {
        ComputeSH(frame, envProbe);
    }
}

void ReflectionProbePass::RenderProbeView(Frame* frame, const RenderSetup& renderSetup, EnvProbe* envProbe)
{
    View* view = renderSetup.view;
    AssertDebug(view != nullptr);

    RenderCollector& renderCollector = GetRenderCollector(view);

#if HYP_DEBUG_MODE
        HYP_LOG(Rendering, Verbose, "Render EnvProbe {}, num total draw calls: {}", envProbe->Id(), renderCollector.NumDrawCallsCollected());
#endif

    renderCollector.ExecuteDrawCalls(frame, renderSetup, RenderBucketMask<RenderBucket::Opaque, RenderBucket::Translucent>);
}

void ReflectionProbePass::ComputePrefilteredEnvMap(Frame* frame, const RenderSetup& renderSetup, EnvProbe* envProbe)
{
    AssertDebug(envProbe);

    RenderProxyEnvProbe* envProbeProxy = static_cast<RenderProxyEnvProbe*>(GetRenderProxy(envProbe));
    AssertDebug(envProbeProxy != nullptr);

    const FramebufferRef& framebuffer = envProbe->GetViewFramebuffer(0);
    AssertDebug(framebuffer.IsValid());

    AttachmentBase* colorAttachment = framebuffer->GetAttachment(0);
    AssertDebug(colorAttachment != nullptr);

    ConvolveProbe::ConvolveEnvProbeCubemap(MakeStrongRef(colorAttachment), *envProbe);
}

void ReflectionProbePass::ComputeSH(Frame* frame, EnvProbe* envProbe)
{
    const FramebufferRef& framebuffer = envProbe->GetViewFramebuffer(0);
    AssertDebug(framebuffer.IsValid() && framebuffer->IsCreated());

    AttachmentBase* colorAttachment = framebuffer->GetAttachment(0);
    Assert(colorAttachment != nullptr && colorAttachment->IsCreated());

    ComputeSH::ComputeEnvProbeSphericalHarmonics(*envProbe, *colorAttachment);
}

#pragma endregion ReflectionProbePass

} // namespace Hyperion
