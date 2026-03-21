/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/renderers/EnvProbeRenderer.hpp>
#include <rendering/renderers/DeferredRenderer.hpp>

#include <rendering/ShaderManager.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/Frame.hpp>
#include <rendering/GpuImage.hpp>
#include <rendering/GpuImageView.hpp>
#include <rendering/GpuBuffer.hpp>
#include <rendering/DescriptorSet.hpp>
#include <rendering/ComputePipeline.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/RenderProxyList.hpp>
#include <rendering/RenderProxy.hpp>
#include <rendering/AsyncCompute.hpp>
#include <rendering/Texture.hpp>
#include <rendering/TextureViewCache.hpp>
#include <rendering/ShaderInstance.hpp>
#include <rendering/RenderCollection.hpp>
#include <rendering/RenderHelpers.hpp>

#include <rendering/shadows/ShadowMapCache.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <scene/View.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/Light.hpp>

#include <Core/math/MathUtil.hpp>

#include <Core/utilities/DeferredScope.hpp>

#include <HyperionEngine.hpp>

#include <EnvProbeRenderer.generated.inl>

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
    CommandRecorder& cr = g_renderInterface->commandRecorderAllocator.GetCommandRecorder();
    HYP_DEFER({ cr.Done(); });
    
    Handle<Texture> prefilteredEnvMap = envProbe.GetPrefilteredEnvMap();
    Assert(prefilteredEnvMap.IsValid() && prefilteredEnvMap->IsCreated());
    
    Handle<Texture> srcTexture;
    bool needsMipMapGeneration = false;
    
    if (inTexture->HasMipMaps())
    {
        srcTexture = inTexture;
    }
    else
    {
        needsMipMapGeneration = true;

        // copy into new texture, we need to generate mips on it before convolving
        srcTexture = MakeHandle<Texture>(
            TextureDesc {
                TextureType::Cubemap,
                inTexture->GetTextureDesc().format,
                inTexture->GetExtent(),
                TFM_LINEAR_MIPMAP,
                TFM_LINEAR,
                TWM_CLAMP_TO_EDGE
            });

        srcTexture->SetName(NAME("EnvProbeRenderer_SrcColorTexture"));
        CheckResult(srcTexture->Create());
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
        const GpuImageRef& dstImage = srcTexture->GetGpuImage();
        Assert(dstImage.IsValid());

        const GpuImageRef& srcImage = inTexture->GetGpuImage();
        Assert(srcImage.IsValid());

        ImageSubResource subResource {};
        subResource.baseMipLevel = 0;
        subResource.numLevels = 1;
        subResource.baseArrayLayer = 0;
        subResource.numLayers = 6;

        cr << InsertBarrier(srcImage, RS_COPY_SRC, subResource);
        cr << InsertBarrier(dstImage, RS_COPY_DST, subResource);

        cr << Blit(
            srcImage,
            dstImage,
            Rect<uint32> { 0, 0, inTexture->GetExtent().x, inTexture->GetExtent().y },
            Rect<uint32> { 0, 0, srcTexture->GetExtent().x, srcTexture->GetExtent().y },
            subResource,
            subResource);

        // back to shader resource state
        cr << InsertBarrier(srcImage, RS_SHADER_RESOURCE, subResource);
        cr << InsertBarrier(dstImage, RS_SHADER_RESOURCE, subResource);

        // generate mips on src texture before running convolve shader using it as a source
        cr << GenerateMipmaps(srcTexture->GetGpuImage());
        cr << InsertBarrier(srcTexture->GetGpuImage(), RS_SHADER_RESOURCE);
    }

    for (uint8 mipIndex = 0; mipIndex < numMips; mipIndex++)
    {
        const float roughness = float(mipIndex) / float(numMips - 1);

        ShaderPropertySet shaderProperties;
        // we have to round otherwise we'll potentially make too many permutations for *almost* the same values.
        shaderProperties.Add(InternShaderProperty(ShaderProperty(NAME("LOBE_SIZE"), MathUtil::Round(roughness, 3))));
        shaderProperties.Add(InternShaderProperty(ShaderProperty(NAME("NUM_SAMPLES"), 2048)));

        const Vec2u mipExtent = mipIndex == 0
            ? extent
            : Vec2u(MathUtil::Max(extent.x >> mipIndex, 1u), MathUtil::Max(extent.y >> mipIndex, 1u));

        GpuBufferRef& uniformBuffer = buffers[mipIndex];

        uniformBuffer = g_renderInterface->MakeGpuBuffer(GpuBufferType::CONSTANT_BUFFER, sizeof(uniforms));
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
        GpuImageViewRef dstImageView = g_renderInterface->textureViewCache->GetOrCreate(
            prefilteredEnvMap, subResource, TextureType::Texture2DArray);

        GpuImageViewRef srcImageView = g_renderInterface->textureViewCache->GetOrCreate(srcTexture);
        
        Assert(dstImageView.IsValid() && srcImageView.IsValid());

        cr << InsertBarrier(prefilteredEnvMap->GetGpuImage(), RS_UNORDERED_ACCESS, subResource);

        const Frame* currFrame = g_renderInterface->GetCurrentFrame();
        const uint32 frameIndex = currFrame ? currFrame->GetFrameIndex() : 0;

        // @TODO Just write the env probe to constant buffer?
        cr << SetShaderUniform(0, "CurrentEnvProbe"_sh, g_renderInterface->gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frameIndex), TShaderDataOffset<EnvProbeShaderData>(&envProbe));
        cr << SetShaderUniform(1, "SphereSamplesBuffer"_sh, g_renderInterface->sphereSamplesBuffer);
        cr << SetShaderUniform(2, "ColorTexture"_sh, srcImageView);
        cr << SetShaderUniform(3, "SamplerLinear"_sh, g_renderInterface->placeholderData->GetSamplerLinear());
        cr << SetShaderUniform(4, "SamplerNearest"_sh, g_renderInterface->placeholderData->GetSamplerNearest());
        cr << SetShaderUniform(5, "OutImage"_sh, dstImageView);
        cr << SetShaderUniform(6, "UniformBuffer"_sh, uniformBuffer);

        cr << DispatchCompute(Vec3u { (mipExtent.x + 7) / 8, (mipExtent.y + 7) / 8, 6 });

        cr << InsertBarrier(prefilteredEnvMap->GetGpuImage(), RS_SHADER_RESOURCE, subResource);
    }

    // Update in env probes texture array if bound
    if (envProbe.IsA(SkyProbe::StaticClass()) || envProbe.IsA(ReflectionProbe::StaticClass()))
    {
        const uint32 boundIndex = RetrieveResourceBinding(&envProbe);

        if (boundIndex != ~0u)
        {
            // blit to the array texture
            const GpuImageRef& srcImage = prefilteredEnvMap->GetGpuImage();
            AssertDebug(srcImage.IsValid());

            const GpuImageRef& dstImage = g_renderInterface->envProbesTexture->GetGpuImage();
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

                cr << Blit(
                    srcImage,
                    dstImage,
                    Rect<uint32> {
                        0, 0,
                        srcMipExtent.x, srcMipExtent.y
                    },
                    Rect<uint32> {
                        0, 0,
                        dstMipExtent.x, dstMipExtent.y
                    },
                    srcSubResource,
                    dstSubResource);
            }

            cr << InsertBarrier(srcImage, RS_SHADER_RESOURCE);
            cr << InsertBarrier(dstImage, RS_SHADER_RESOURCE);
        }
    }

    // keep some resources around until we know we're done with them from this pass
    EnqueueDeletion(std::move(buffers));
    EnqueueDeletion(std::move(prefilteredEnvMap));

    if (needsMipMapGeneration)
    {
        EnqueueDeletion(std::move(srcTexture));
    }
}

} // namespace ConvolveProbe

#pragma endregion ConvolveProbe

#pragma region EnvProbeRenderer

EnvProbeRenderer::EnvProbeRenderer()
{
}

EnvProbeRenderer::~EnvProbeRenderer()
{
}

void EnvProbeRenderer::Initialize()
{
}

void EnvProbeRenderer::Shutdown()
{
}

void EnvProbeRenderer::RenderFrame(Frame* frame, const RenderSetup& renderSetup)
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

PassData* EnvProbeRenderer::CreateViewPassData(View* view, PassDataExt& ext)
{
    EnvProbeRendererPassData* pd = new EnvProbeRendererPassData();
    pd->view = MakeWeakRef(view);

    return pd;
}

#pragma endregion EnvProbeRenderer

#pragma region ReflectionProbeRenderer

ReflectionProbeRenderer::ReflectionProbeRenderer()
{
}

ReflectionProbeRenderer::~ReflectionProbeRenderer()
{
}

void ReflectionProbeRenderer::Initialize()
{
    HYP_SCOPE;

    EnvProbeRenderer::Initialize();
}

void ReflectionProbeRenderer::Shutdown()
{
    HYP_SCOPE;

    EnvProbeRenderer::Shutdown();
}

void ReflectionProbeRenderer::RenderProbe(Frame* frame, const RenderSetup& renderSetup, EnvProbe* envProbe)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    AssertDebug(!envProbe->IsBaked());

    AssertDebug(renderSetup.world && renderSetup.view);

    View* view = renderSetup.view;
    AssertDebug(view != nullptr);

    EnvProbeRendererPassData* pd = ObjCast<EnvProbeRendererPassData>(renderSetup.passData);
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
        AssertDebug(RetrieveResourceBinding(renderSetup.light) != ~0u);

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
        ComputeSH(frame, renderSetup, envProbe);
    }

    /*if (SkyProbe* skyProbe = ObjCast<SkyProbe>(envProbe))
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

void ReflectionProbeRenderer::ComputePrefilteredEnvMap(Frame* frame, const RenderSetup& renderSetup, EnvProbe* envProbe)
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

void ReflectionProbeRenderer::ComputeSH(Frame* frame, const RenderSetup& renderSetup, EnvProbe* envProbe)
{
    HYP_SCOPE;

    View* view = renderSetup.view;
    AssertDebug(view != nullptr);

    RenderProxyEnvProbe* envProbeProxy = static_cast<RenderProxyEnvProbe*>(GetRenderProxy(envProbe));
    Assert(envProbeProxy != nullptr);

    RenderProxyList& rpl = GetConsumerProxyList(view);
    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    const ViewOutputTarget& outputTarget = envProbe->GetView()->GetOutputTarget();

    const FramebufferRef& framebuffer = outputTarget.GetFramebuffer();
    AssertDebug(framebuffer.IsValid());

    AttachmentBase* colorAttachment = framebuffer->GetAttachment(0);
    Assert(colorAttachment != nullptr);

    AttachmentBase* normalsAttachment = framebuffer->GetAttachment(1);
    AttachmentBase* depthAttachment = framebuffer->GetAttachment(2);

    Array<GpuBufferRef, FixedAllocator<ShNumLevels>> shTilesBuffers;
    shTilesBuffers.Resize(ShNumLevels);

    for (uint32 i = 0; i < ShNumLevels; i++)
    {
        const size_t size = sizeof(SHTile) * (ShNumTiles.x >> i) * (ShNumTiles.y >> i);

        shTilesBuffers[i] = g_renderInterface->MakeGpuBuffer(GpuBufferType::STORAGE_BUFFER, size);
        shTilesBuffers[i]->SetIsCpuAccessible(true);
        Assert(shTilesBuffers[i]->Create());
    }

    ShaderPropertySet shaderProperties;

    if (!envProbe->IsSkyProbe())
    {
        shaderProperties.Add(InternShaderProperty(ShaderProperty(NAME("LIGHTING"))));
    }

    // Bind a directional light and sky envprobe if available
    EnvProbe* skyProbe = nullptr;
    Light* directionalLight = nullptr;

    for (Light* light : rpl.GetLights())
    {
        if (light->GetLightType() == LightType::Directional)
        {
            AssertDebug(RetrieveResourceBinding(light) != ~0u, "Light not bound!");

            directionalLight = light;

            break;
        }
    }

    if (const auto& skyProbes = rpl.GetEnvProbes().GetElements<SkyProbe>(); skyProbes.Any())
    {
        skyProbe = skyProbes.Front();
        AssertDebug(skyProbe != nullptr);
        AssertDebug(skyProbe->IsA<SkyProbe>());
    }

    const Vec2u cubemapDimensions = colorAttachment->GetExtent().GetXY();

    struct SHUniforms
    {
        Vec4u probeGridPosition;
        Vec4u cubemapDimensions;
        Vec4u levelDimensions;
        Vec4f worldPosition;
        uint32 envProbeIndex;
    } uniforms;

    uniforms.envProbeIndex = RetrieveResourceBinding(envProbe);
    uniforms.probeGridPosition = { 0, 0, 0, 0 };
    uniforms.cubemapDimensions = Vec4u { cubemapDimensions, 0, 0 };
    uniforms.worldPosition = envProbeProxy->bufferData.worldPosition;

    AssertDebug(uniforms.envProbeIndex != ~0u);

    Array<GpuBufferRef> uniformBuffers;

    AsyncCompute* asyncCompute = g_renderInterface->CreateAsyncCompute();

    CommandRecorder& asyncRecorder = asyncCompute->cr;

    asyncRecorder << InsertBarrier(shTilesBuffers[0], RS_UNORDERED_ACCESS, ShaderModuleType::Compute);
    asyncRecorder << InsertBarrier(g_renderInterface->gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frame->GetFrameIndex()), RS_UNORDERED_ACCESS, ShaderModuleType::Compute);

    // Helper to run pass
    auto RunPass = [&](Name mode, const SHUniforms& passUniforms, const Vec3u& dispatchGroupSize, const GpuBufferRef& inputBuffer, const GpuBufferRef& outputBuffer)
    {
        ShaderPropertySet passShaderProperties;
        passShaderProperties.Add(InternShaderProperty(ShaderProperty(NAME("MODE"), mode)));
        passShaderProperties = passShaderProperties | shaderProperties;

        ShaderDesc shaderDesc(NAME("ComputeSH"), passShaderProperties);
        asyncRecorder << SetCurrentShader(shaderDesc);

        GpuBufferRef ub = g_renderInterface->MakeGpuBuffer(GpuBufferType::CONSTANT_BUFFER, sizeof(SHUniforms));
        ub->Create();
        ub->Copy(sizeof(SHUniforms), &passUniforms);
        uniformBuffers.PushBack(ub);

        asyncRecorder << SetShaderUniform(0, "SamplerLinear"_sh, g_renderInterface->placeholderData->GetSamplerLinear());
        asyncRecorder << SetShaderUniform(1, "SamplerNearest"_sh, g_renderInterface->placeholderData->GetSamplerNearest());
        asyncRecorder << SetShaderUniform(2, "EnvProbesTexture"_sh, g_renderInterface->textureViewCache->GetOrCreate(g_renderInterface->envProbesTexture));
        asyncRecorder << SetShaderUniform(3, "EnvProbesBuffer"_sh, g_renderInterface->gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frame->GetFrameIndex()));

        if (skyProbe)
            asyncRecorder << SetShaderUniform(4, "CurrentEnvProbe"_sh, g_renderInterface->gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frame->GetFrameIndex()), TShaderDataOffset<EnvProbeShaderData>(skyProbe));
        else
            asyncRecorder << SetShaderUniform(4, "CurrentEnvProbe"_sh, g_renderInterface->gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frame->GetFrameIndex()), TShaderDataOffset<EnvProbeShaderData>(0));

        asyncRecorder << SetShaderUniform(5, "ShadowMapsTextureArray"_sh, g_renderInterface->shadowMapCache->GetAtlasImageView());
        asyncRecorder << SetShaderUniform(6, "PointLightShadowMapsTextureArray"_sh, g_renderInterface->shadowMapCache->GetPointLightShadowMapImageView());

        if (directionalLight)
            asyncRecorder << SetShaderUniform(7, "CurrentLight"_sh, g_renderInterface->gpuBuffers[GRB_LIGHTS]->GetBuffer(frame->GetFrameIndex()), TShaderDataOffset<LightShaderData>(directionalLight));
        else
            asyncRecorder << SetShaderUniform(7, "CurrentLight"_sh, g_renderInterface->gpuBuffers[GRB_LIGHTS]->GetBuffer(frame->GetFrameIndex()), TShaderDataOffset<LightShaderData>(0));

        asyncRecorder << SetShaderUniform(8, "InColorCubemap"_sh, colorAttachment->GetImageView());
        asyncRecorder << SetShaderUniform(9, "InNormalsCubemap"_sh, normalsAttachment ? normalsAttachment->GetImageView() : g_renderInterface->placeholderData->GetImageViewCube1x1R8());
        asyncRecorder << SetShaderUniform(10, "InDepthCubemap"_sh, depthAttachment ? depthAttachment->GetImageView() : g_renderInterface->placeholderData->GetImageViewCube1x1R8());
        asyncRecorder << SetShaderUniform(11, "InputSHTilesBuffer"_sh, inputBuffer);
        asyncRecorder << SetShaderUniform(12, "OutputSHTilesBuffer"_sh, outputBuffer);
        asyncRecorder << SetShaderUniform(13, "SHUniforms"_sh, ub);

        asyncRecorder << DispatchCompute(dispatchGroupSize);
    };

    // MODE_CLEAR
    RunPass(NAME("CLEAR"), uniforms, Vec3u { 1, 1, 1 }, shTilesBuffers[0], shTilesBuffers[1]);

    asyncRecorder << InsertBarrier(shTilesBuffers[0], RS_UNORDERED_ACCESS, ShaderModuleType::Compute);

    // MODE_BUILD_COEFFICIENTS
    RunPass(NAME("BUILD_COEFFICIENTS"), uniforms, Vec3u { 1, 1, 1 }, shTilesBuffers[0], shTilesBuffers[1]);

    // Parallel reduce
    if (ShParallelReduce)
    {
        for (uint32 i = 1; i < ShNumLevels; i++)
        {
            asyncRecorder << InsertBarrier(
                shTilesBuffers[i - 1],
                RS_UNORDERED_ACCESS,
                ShaderModuleType::Compute);

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

            RunPass(NAME("REDUCE"), reduceUniforms, Vec3u { 1, (nextDimensions.x + 3) / 4, (nextDimensions.y + 3) / 4 }, shTilesBuffers[i - 1], shTilesBuffers[i]);
        }
    }

    const uint32 finalizeShBufferIndex = ShParallelReduce ? ShNumLevels - 1 : 0;

    // Finalize - build into final buffer
    asyncRecorder << InsertBarrier(shTilesBuffers[finalizeShBufferIndex], RS_UNORDERED_ACCESS, ShaderModuleType::Compute);
    asyncRecorder << InsertBarrier(g_renderInterface->gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frame->GetFrameIndex()), RS_UNORDERED_ACCESS, ShaderModuleType::Compute);

    // MODE_FINALIZE
    RunPass(NAME("FINALIZE"), uniforms, Vec3u { 1, 1, 1 }, shTilesBuffers[finalizeShBufferIndex], shTilesBuffers[finalizeShBufferIndex]);

    asyncRecorder << InsertBarrier(g_renderInterface->gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frame->GetFrameIndex()), RS_UNORDERED_ACCESS, ShaderModuleType::Compute);

    asyncCompute->OnCompleted
        .Bind([asyncCompute,
                  envProbe = MakeStrongRef(envProbe),
                  shTilesBuffers = std::move(shTilesBuffers),
                  uniformBuffers = std::move(uniformBuffers)]() mutable
            {
                //const uint32 boundIndex = RetrieveResourceBinding(envProbe);
                //Assert(boundIndex != ~0u);

                // @TODO! Copy to cpu side data

                EnqueueDeletion(std::move(shTilesBuffers));
                EnqueueDeletion(std::move(uniformBuffers));
            })
        .Detach();

    g_renderInterface->SubmitAsyncCompute(asyncCompute);
}

#pragma endregion ReflectionProbeRenderer

} // namespace Hyperion
