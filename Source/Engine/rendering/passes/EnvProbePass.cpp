/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <rendering/passes/EnvProbePass.hpp>
#include <rendering/passes/DeferredPass.hpp>

#include <rendering/ShaderManager.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/Frame.hpp>
#include <rendering/GpuImage.hpp>
#include <rendering/GpuImageView.hpp>
#include <rendering/GpuBuffer.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/ComputePipeline.hpp>
#include <rendering/RenderTypes.hpp>
#include <rendering/RenderProxyList.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/AsyncCompute.hpp>
#include <rendering/Texture.hpp>
#include <rendering/TextureViewCache.hpp>
#include <rendering/ShaderInstance.hpp>
#include <rendering/RendererMain.hpp>
#include <rendering/RenderHelpers.hpp>
#include <rendering/ScratchImageAllocator.hpp>

#include <rendering/shadows/ShadowMapCache.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <scene/View.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/Light.hpp>

#include <Core/math/MathUtil.hpp>

#include <Core/utilities/DeferredScope.hpp>

#include <util/img/Bitmap.hpp>

#include <HyperionEngine.hpp>

#include <EnvProbePass.generated.inl>

namespace Hyperion {

static constexpr Vec2u ShNumSamples = { 16, 16 };
static constexpr Vec2u ShNumTiles = { 16, 16 };
static constexpr uint32 ShNumLevels = MathUtil::Max(1u, uint32(MathUtil::FastLog2(ShNumSamples.Max()) + 1));
static constexpr bool ShParallelReduce = false;

static FixedArray<Mat4f, 6> CreateCubemapMatrices(const BoundingBox& aabb, const Vec3f& origin)
{
    FixedArray<Mat4f, 6> viewMatrices;

    for (uint32 i = 0; i < 6; i++)
    {
        viewMatrices[i] = Mat4f::LookAt(
            origin,
            origin + Texture::s_cubemapDirections[i].first,
            Texture::s_cubemapDirections[i].second);
    }

    return viewMatrices;
}

#pragma region ConvolveProbe

namespace ConvolveProbe {

struct ConvolveProbeUniforms
{
    Vec2u outImageDimensions;
    Vec2u inImageDimensions;
};

void ConvolveEnvProbeCubemap(
    const Handle<Texture>& inTexture,
    const EnvProbe& envProbe)
{
    Assert(inTexture != nullptr);

    // Alloc command recorder
    // we need to do this after we Create() the src texture,
    // because CreateGpuImage in Texture.cpp creates its own command recorder,
    // so we need that one to run before this one.
    CommandRecorder& cr = RI.commandRecorderAllocator.GetCommandRecorder();
    HYP_DEFER({ cr.Done(); });

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

    ConvolveProbeUniforms uniforms {};
    uniforms.outImageDimensions = Vec2u::Zero(); // set for each mip pass
    uniforms.inImageDimensions = inTexture->GetExtent().GetXY();

    const Vec2u extent = envProbe.GetDimensions();
    const uint8 numMips = uint8(MathUtil::FastLog2(MathUtil::Max(extent.x, extent.y))) + 1;

    Array<GpuBufferRef> buffers;
    buffers.Resize(numMips);

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

    for (uint8 mipIndex = 0; mipIndex < numMips; mipIndex++)
    {
        const float roughness = float(mipIndex) / float(numMips - 1);

        ShaderPropertySet shaderProperties;
        // we have to round otherwise we'll potentially make too many permutations for *almost* the same values.
        shaderProperties.Add(InternShaderProperty(ShaderProperty(NAME("LOBE_SIZE"), MathUtil::Round(roughness, 3))));
        shaderProperties.Add(InternShaderProperty(ShaderProperty(NAME("NUM_SAMPLES"), 4096)));

        const Vec2u mipExtent = mipIndex == 0
            ? extent
            : Vec2u(MathUtil::Max(extent.x >> mipIndex, 1u), MathUtil::Max(extent.y >> mipIndex, 1u));

        GpuBufferRef& uniformBuffer = buffers[mipIndex];

        uniformBuffer = RI.MakeGpuBuffer(GpuBufferType::ConstantBuffer, sizeof(uniforms));
        Assert(uniformBuffer->Create());

        uniforms.outImageDimensions = mipExtent;

        uniformBuffer->Copy(sizeof(uniforms), &uniforms);

        cr << SetCurrentShader(ShaderDesc(NAME("ConvolveProbe"), shaderProperties));

        ImageSubResource subResource {};
        subResource.baseMipLevel = mipIndex;
        subResource.numLevels = 1;
        subResource.baseArrayLayer = 0;
        subResource.numLayers = 6;

        // create the view as 2D array instead of cubemap
        GpuImageViewRef dstImageView = RI.textureViewCache->GetOrCreate(
            dstTexture, subResource, TextureType::Texture2DArray);

        GpuImageViewRef srcImageView = RI.textureViewCache->GetOrCreate(srcTexture);

        Assert(dstImageView.IsValid() && srcImageView.IsValid());

        cr << InsertBarrier(dstTexture->GetGpuImage(), RS_UNORDERED_ACCESS, subResource);

        // @TODO Just write the env probe to constant buffer?
        cr << SetShaderUniform(0, "CurrentEnvProbe"_sh, RI.namedBuffers[NamedBuffer::EnvProbes], Resources::GetBinding(&envProbe));
        cr << SetShaderUniform(1, "SphereSamplesBuffer"_sh, RI.sphereSamplesBuffer);
        cr << SetShaderUniform(2, "ColorTexture"_sh, srcImageView);
        cr << SetShaderUniform(3, "SamplerLinear"_sh, RI.placeholderData->GetSamplerLinear());
        cr << SetShaderUniform(4, "SamplerNearest"_sh, RI.placeholderData->GetSamplerNearest());
        cr << SetShaderUniform(5, "OutImage"_sh, dstImageView);
        cr << SetShaderUniform(6, "UniformBuffer"_sh, uniformBuffer);

        cr << DispatchCompute(Vec3u { (mipExtent.x + 7) / 8, (mipExtent.y + 7) / 8, 6 });

        // now copy it to the actual dst
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

            auto textureResGuard = prefilteredEnvMap->GetWriteScope();

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

            textureResGuard.Reset();

            auto envProbeResGuard = envProbeStrong->GetWriteScope();
            envProbeStrong->SetBakedTexture(prefilteredEnvMap);
        }, /* allMips */ true);
    }

    // Update in env probes texture array if bound
    if (envProbe.IsA<SkyProbe>() || envProbe.IsA<ReflectionProbe>())
    {
        const uint32 boundIndex = Resources::GetBinding(&envProbe);

        if (boundIndex != ~0u)
        {
            // blit to the array texture
            const GpuImageRef& srcImage = prefilteredEnvMap->GetGpuImage();
            AssertDebug(srcImage.IsValid());

            const GpuImageRef& dstImage = RI.envProbesTexture->GetGpuImage();
            Assert(dstImage.IsValid());

            cr << InsertBarrier(srcImage, RS_COPY_SRC);
            cr << InsertBarrier(dstImage, RS_COPY_DST);

            for (uint8 mipIndex = 0; mipIndex < dstImage->NumMips(); mipIndex++)
            {
                if (mipIndex >= srcImage->NumMips())
                {
                    break;
                }

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

                const Vec3u srcMipExtent = srcImage->GetTextureDesc().GetMipExtent(mipIndex);
                const Vec3u dstMipExtent = dstImage->GetTextureDesc().GetMipExtent(mipIndex);

                if (srcMipExtent == dstMipExtent && srcImage->GetTextureDesc().format == dstImage->GetTextureDesc().format)
                {
                    cr << CopyImage(srcImage, dstImage, srcMipExtent, srcSubResource, dstSubResource);
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

            cr << InsertBarrier(srcImage, RS_SHADER_RESOURCE);
            cr << InsertBarrier(dstImage, RS_SHADER_RESOURCE);
        }
    }

    // keep some resources around until we know we're done with them from this pass
    EnqueueDeletion(std::move(buffers));
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

    // @TODO fix thread safety
    Frame* frame = RI.GetCurrentFrame();
    Assert(frame != nullptr);

    AsyncCompute* asyncCompute = useAsyncCompute ? RI.CreateAsyncCompute() : nullptr;

    CommandRecorder& cr = useAsyncCompute ? asyncCompute->cr : RI.commandRecorderAllocator.GetCommandRecorder();

    FixedArray<RWStructuredBuffer, ShNumLevels> shTilesBuffers;

    for (uint32 i = 0; i < ShNumLevels; i++)
    {
        shTilesBuffers[i] = RWStructuredBuffer((ShNumTiles.x >> i) * (ShNumTiles.y >> i), sizeof(SHTile));
        shTilesBuffers[i].Initialize();
    }

    const Vec2u cubemapDimensions = inColorTexture.GetExtent().GetXY();

    struct SHUniforms
    {
        Vec4u levelDimensions;
    } uniforms;

    static constexpr uint32 ShDataSize = sizeof(EnvProbeShaderData::shData);

    GpuBufferRef shBuffer = RI.MakeGpuBuffer(GpuBufferType::RWStructuredBuffer, MathUtil::NextPowerOf2(ShDataSize));
    CheckResult(shBuffer->Create());

    Array<GpuBufferRef> uniformBuffers;

    cr << InsertBarrier(shTilesBuffers[0].gpuBuffer, RS_UNORDERED_ACCESS, ShaderModuleType::Compute);
    cr << InsertBarrier(shBuffer, RS_UNORDERED_ACCESS, ShaderModuleType::Compute);

    ShaderPropertySet shaderProperties;

    // Helper to run pass
    auto RunPass = [&](Name mode, const SHUniforms& passUniforms, const Vec3u& dispatchGroupSize, const StructuredBuffer& inputBuffer, const StructuredBuffer& outputBuffer)
    {
        ShaderPropertySet passShaderProperties;
        passShaderProperties.Add(InternShaderProperty(ShaderProperty(NAME("MODE"), mode)));
        passShaderProperties = passShaderProperties | shaderProperties;

        ShaderDesc shaderDesc(NAME("ComputeSH"), passShaderProperties);
        cr << SetCurrentShader(shaderDesc);

        GpuBufferRef ub = RI.MakeGpuBuffer(GpuBufferType::ConstantBuffer, sizeof(SHUniforms));
        ub->Create();
        ub->Copy(sizeof(SHUniforms), &passUniforms);
        uniformBuffers.PushBack(ub);

        cr << SetShaderUniform(0, "SamplerLinear"_sh, RI.placeholderData->GetSamplerLinear());
        cr << SetShaderUniform(1, "SamplerNearest"_sh, RI.placeholderData->GetSamplerNearest());
        cr << SetShaderUniform(3, "EnvProbesBuffer"_sh, RI.namedBuffers[NamedBuffer::EnvProbes]);

        cr << SetShaderUniform(4, "OutSHBuffer"_sh, shBuffer, ShaderDataOffset(0, sizeof(Vec4f)));

        cr << SetShaderUniform(8, "InColorCubemap"_sh, RI.textureViewCache->GetOrCreate(const_cast<Texture*>(&inColorTexture)));
        cr << SetShaderUniform(11, "InputSHTilesBuffer"_sh, inputBuffer);
        cr << SetShaderUniform(12, "OutputSHTilesBuffer"_sh, outputBuffer);
        cr << SetShaderUniform(13, "SHUniforms"_sh, ub);

        cr << DispatchCompute(dispatchGroupSize);
    };

    // MODE_CLEAR
    RunPass(NAME("CLEAR"), uniforms, Vec3u { 1, 1, 1 }, shTilesBuffers[0], shTilesBuffers[1]);

    cr << InsertBarrier(shTilesBuffers[0].gpuBuffer, RS_UNORDERED_ACCESS, ShaderModuleType::Compute);

    // MODE_BUILD_COEFFICIENTS
    RunPass(NAME("BUILD_COEFFICIENTS"), uniforms, Vec3u { 1, 1, 1 }, shTilesBuffers[0], shTilesBuffers[1]);

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

            SHUniforms reduceUniforms = uniforms;
            reduceUniforms.levelDimensions = {
                prevDimensions.x,
                prevDimensions.y,
                nextDimensions.x,
                nextDimensions.y
            };

            RunPass(
                NAME("REDUCE"),
                reduceUniforms,
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
        uniforms,
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
        Array<GpuBufferRef> uniformBuffers;
    };

    // Custom CmdBase class, executes when all previous commands are done.
    //Always executes on the Render thread.
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
                        auto resGuard = payload.envProbe->GetWriteScope();
                        payload.envProbe->SetSphericalHarmonicsData(shData);
                    }

                    EnqueueDeletion(std::move(payload.shBuffer));
                    EnqueueDeletion(std::move(payload.readbackBuffer));
                    EnqueueDeletion(std::move(payload.uniformBuffers));

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
    payload->uniformBuffers = uniformBuffers;

    cr << ReadbackSphericalHarmonics(payload);

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
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.world && renderSetup.envProbe);

    EnvProbe* envProbe = renderSetup.envProbe;
    AssertDebug(envProbe != nullptr);

    RenderSetup rs = renderSetup.Fork();
    rs.view = envProbe->GetView();
    rs.passData = FetchViewPassData(rs.view);
    rs.envProbe = renderSetup.prev ? renderSetup.prev->envProbe : nullptr;
    rs.viewport = Viewport { envProbe->GetDimensions() };

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
    HYP_SCOPE;

    EnvProbePassBase::Initialize();
}

void ReflectionProbePass::Shutdown()
{
    HYP_SCOPE;

    EnvProbePassBase::Shutdown();
}

void ReflectionProbePass::RenderProbe(Frame* frame, const RenderSetup& renderSetup, EnvProbe* envProbe)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(!envProbe->IsBaked());

    AssertDebug(renderSetup.world && renderSetup.view);

    View* view = renderSetup.view;
    AssertDebug(view != nullptr);

    EnvProbePassData* pd = DynamicCast<EnvProbePassData>(renderSetup.passData);
    AssertDebug(pd != nullptr);

    RenderProxyList& rpl = GetConsumerProxyList(view);
    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    // special checks for Sky + caching result based on light position + intensity
    if (envProbe->IsA(SkyProbe::StaticClass()))
    {
        if (!renderSetup.light)
        {
            HYP_LOG_ONCE(Rendering, Warning, "No directional light bound while rendering SkyProbe {} in view {}", envProbe->Id(), view->Id());

            pd->cachedLightDirIntensity = MathUtil::NaN<Vec4f>();

            return;
        }

        if (renderSetup.light->GetLightType() != LightType::Directional)
        {
            HYP_LOG_ONCE(Rendering, Warning, "Light bound to SkyProbe pass is not a directional light: {} in view {}",
                renderSetup.light->Id(), view->Id());

            pd->cachedLightDirIntensity = MathUtil::NaN<Vec4f>();

            return;
        }

        RenderProxyLight* lightProxy = static_cast<RenderProxyLight*>(GetRenderProxy(renderSetup.light));
        AssertDebug(lightProxy != nullptr);

#if HYP_DEBUG_MODE
        AssertDebug(Resources::GetBinding(renderSetup.light) != ~0u);
#endif

        if (lightProxy->bufferData.positionIntensity == pd->cachedLightDirIntensity
            && !rpl.GetMeshEntities().GetDiff().NeedsUpdate())
        {
            // no need to render it just yet if values have not changed -- return early
            return;
        }

        // cache it to save on rendering later
        pd->cachedLightDirIntensity = lightProxy->bufferData.positionIntensity;
    }

    RenderProxyEnvProbe* envProbeProxy = static_cast<RenderProxyEnvProbe*>(GetRenderProxy(envProbe));
    AssertDebug(envProbeProxy != nullptr);

    if (envProbe->IsA(ReflectionProbe::StaticClass())
        && !rpl.GetMeshEntities().GetDiff().NeedsUpdate()
        && !rpl.GetLights().GetDiff().NeedsUpdate()
        && pd->cachedProbeOrigin == envProbeProxy->bufferData.worldPosition.GetXYZ())
    {
        return;
    }

    pd->cachedProbeOrigin = envProbeProxy->bufferData.worldPosition.GetXYZ();

    RenderCollector& renderCollector = GetRenderCollector(view);

#if HYP_DEBUG_MODE
    HYP_LOG(Rendering, Verbose, "Render EnvProbe {} with {} mesh entities (shared: {}), num total draw calls: {}", envProbe->Id(), rpl.GetMeshEntities().NumCurrent(),
        rpl.isShared,
        renderCollector.NumDrawCallsCollected());
#endif

    renderCollector.ExecuteDrawCalls(frame, renderSetup, RenderBucketMask<RenderBucket::Opaque, RenderBucket::Translucent>);

    const ViewOutputTarget& outputTarget = view->GetOutputTarget();
    AssertDebug(outputTarget.IsValid());

    const FramebufferRef& framebuffer = outputTarget.GetFramebuffer();
    AssertDebug(framebuffer.IsValid());

    const GpuImageRef& framebufferImage = framebuffer->GetAttachment(0)->GetGpuImage();

    if (envProbe->ShouldComputePrefilteredEnvMap())
    {
        ComputePrefilteredEnvMap(frame, renderSetup, envProbe);
    }

    if (envProbe->ShouldComputeSphericalHarmonics())
    {
        ComputeSH(frame, envProbe);
    }

    /*if (SkyProbe* skyProbe = DynamicCast<SkyProbe>(envProbe))
    {
        Assert(skyProbe->GetSkyboxCubemap().IsValid());

        const GpuImageRef& dstImage = skyProbe->GetSkyboxCubemap()->GetGpuImage();
        Assert(dstImage.IsValid());
        Assert(dstImage->IsCreated());

        frame->cr << InsertBarrier(framebufferImage, RS_COPY_SRC);
        frame->cr << InsertBarrier(dstImage, RS_COPY_DST);

        frame->cr << Blit(framebufferImage, dstImage);

        if (dstImage->HasMipMaps())
        {
            frame->cr << GenerateMipmaps(dstImage);
        }

        frame->cr << InsertBarrier(framebufferImage, RS_SHADER_RESOURCE);
        frame->cr << InsertBarrier(dstImage, RS_SHADER_RESOURCE);
    }*/
}

void ReflectionProbePass::ComputePrefilteredEnvMap(Frame* frame, const RenderSetup& renderSetup, EnvProbe* envProbe)
{
    HYP_SCOPE;

    AssertDebug(renderSetup.world && renderSetup.view && envProbe);

    View* view = renderSetup.view;
    AssertDebug(view != nullptr);

    RenderProxyEnvProbe* envProbeProxy = static_cast<RenderProxyEnvProbe*>(GetRenderProxy(envProbe));
    AssertDebug(envProbeProxy != nullptr);

    RenderProxyList& rpl = GetConsumerProxyList(view);
    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    const ViewOutputTarget& outputTarget = view->GetOutputTarget();
    AssertDebug(outputTarget.IsValid());

    const FramebufferRef& framebuffer = outputTarget.GetFramebuffer();
    AssertDebug(framebuffer.IsValid());

    AttachmentBase* colorAttachment = framebuffer->GetAttachment(0);
    AssertDebug(colorAttachment != nullptr);

    ConvolveProbe::ConvolveEnvProbeCubemap(
        MakeStrongRef(colorAttachment),
        *envProbe);
}

void ReflectionProbePass::ComputeSH(Frame* frame, EnvProbe* envProbe)
{
    HYP_SCOPE;

    const ViewOutputTarget& outputTarget = envProbe->GetView()->GetOutputTarget();

    const FramebufferRef& framebuffer = outputTarget.GetFramebuffer();
    AssertDebug(framebuffer.IsValid() && framebuffer->IsCreated());

    AttachmentBase* colorAttachment = framebuffer->GetAttachment(0);
    Assert(colorAttachment != nullptr && colorAttachment->IsCreated());

    ComputeSH::ComputeEnvProbeSphericalHarmonics(*envProbe, *colorAttachment);
}

#pragma endregion ReflectionProbePass

} // namespace Hyperion
