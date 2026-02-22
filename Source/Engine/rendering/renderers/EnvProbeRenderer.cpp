/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

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
#include <rendering/shadows/ShadowMapAllocator.hpp>

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

    RenderSetup rs = renderSetup;
    rs.view = envProbe->GetView();
    rs.passData = FetchViewPassData(rs.view);
    rs.envProbe = renderSetup.prev ? renderSetup.prev->envProbe : nullptr;

    RenderProbe(frame, rs, envProbe);
}

PassData* EnvProbeRenderer::CreateViewPassData(View* view, PassDataExt& ext)
{
    EnvProbeRendererPassData* pd = new EnvProbeRendererPassData();
    pd->view = MakeWeakRef(view);
    pd->viewport = view->GetViewport();

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

        if (renderSetup.light->GetLightType() != LT_DIRECTIONAL)
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

    HYP_LOG(Rendering, Info, "Render EnvProbe {} with {} mesh entities (shared: {}), num total draw calls: {}", envProbe->Id(), rpl.GetMeshEntities().NumCurrent(),
        rpl.isShared,
        renderCollector.NumDrawCallsCollected());

    renderCollector.ExecuteDrawCalls(frame, renderSetup, ((1u << RB_OPAQUE) | (1u << RB_TRANSLUCENT)));

    const ViewOutputTarget& outputTarget = view->GetOutputTarget();
    AssertDebug(outputTarget.IsValid());

    const FramebufferRef& framebuffer = outputTarget.GetFramebuffer();
    AssertDebug(framebuffer.IsValid());

    const GpuImageRef& framebufferImage = framebuffer->GetAttachment(0)->GetImage();

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

        frame->renderQueue << InsertBarrier(framebufferImage, RS_COPY_SRC);
        frame->renderQueue << InsertBarrier(dstImage, RS_COPY_DST);

        frame->renderQueue << Blit(framebufferImage, dstImage);

        if (dstImage->HasMipMaps())
        {
            frame->renderQueue << GenerateMipmaps(dstImage);
        }

        frame->renderQueue << InsertBarrier(framebufferImage, RS_SHADER_RESOURCE);
        frame->renderQueue << InsertBarrier(dstImage, RS_SHADER_RESOURCE);
    }*/
}

void ReflectionProbeRenderer::ComputePrefilteredEnvMap(Frame* frame, const RenderSetup& renderSetup, EnvProbe* envProbe)
{
    HYP_SCOPE;

    AssertDebug(renderSetup.world && renderSetup.view);

    View* view = renderSetup.view;
    AssertDebug(view != nullptr);

    RenderProxyEnvProbe* envProbeProxy = static_cast<RenderProxyEnvProbe*>(GetRenderProxy(envProbe));
    AssertDebug(envProbeProxy != nullptr);

    RenderProxyList& rpl = GetConsumerProxyList(view);
    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    struct ConvolveProbeUniforms
    {
        Vec2u outImageDimensions;
        Vec2u inImageDimensions;
        Vec4f worldPosition;
    };

    const Handle<Texture>& prefilteredEnvMap = envProbe->GetPrefilteredEnvMap();
    Assert(prefilteredEnvMap.IsValid());

    const ViewOutputTarget& outputTarget = view->GetOutputTarget();
    AssertDebug(outputTarget.IsValid());

    const FramebufferRef& framebuffer = outputTarget.GetFramebuffer();
    AssertDebug(framebuffer.IsValid());

    AttachmentBase* colorAttachment = framebuffer->GetAttachment(0);
    AssertDebug(colorAttachment != nullptr);

    ConvolveProbeUniforms uniforms {};
    uniforms.outImageDimensions = Vec2u::Zero(); // set for each mip pass
    uniforms.inImageDimensions = colorAttachment->GetImage()->GetExtent().GetXY();
    uniforms.worldPosition = envProbeProxy->bufferData.worldPosition;

    const Vec2u extent = prefilteredEnvMap->GetExtent().GetXY();
    const uint8 numMips = uint8(MathUtil::FastLog2(MathUtil::Max(extent.x, extent.y))) + 1;

    Array<GpuBufferRef> buffers;
    buffers.Resize(numMips);

    frame->renderQueue << InsertBarrier(prefilteredEnvMap->GetGpuImage(), RS_SHADER_RESOURCE);

    for (uint8 mipIndex = 0; mipIndex < numMips; mipIndex++)
    {
        const float roughness = float(mipIndex) / float(numMips - 1);
        const float perceptualRoughness = MathUtil::Round(roughness * roughness, 3);

        ShaderPropertySet shaderProperties;
        shaderProperties.Add(InternShaderProperty(ShaderProperty(NAME("LOBE_SIZE"), perceptualRoughness)));
        shaderProperties.Add(InternShaderProperty(ShaderProperty(NAME("NUM_SAMPLES"), 2048)));

        const Vec2u mipExtent = mipIndex == 0
            ? extent
            : Vec2u(MathUtil::Max(extent.x >> mipIndex, 1u), MathUtil::Max(extent.y >> mipIndex, 1u));

        GpuBufferRef& uniformBuffer = buffers[mipIndex];

        uniformBuffer = g_renderInterface->MakeGpuBuffer(GpuBufferType::CONSTANT_BUFFER, sizeof(uniforms));
        Assert(uniformBuffer->Create());

        uniforms.outImageDimensions = mipExtent;

        uniformBuffer->Copy(sizeof(uniforms), &uniforms);

        frame->renderQueue << SetCurrentShader(ShaderDesc(NAME("ConvolveProbe"), shaderProperties));

        ImageSubResource subResource {};
        subResource.baseMipLevel = mipIndex;
        subResource.numLevels = 1;
        subResource.baseArrayLayer = 0;
        subResource.numLayers = 6;

        // create the view as 2D array instead of cubemap
        const GpuImageViewRef& imageView = g_renderInterface->textureViewCache->GetOrCreate(
            prefilteredEnvMap, subResource, TextureType::Texture2DArray);

        Assert(imageView != nullptr);

        frame->renderQueue << InsertBarrier(prefilteredEnvMap->GetGpuImage(), RS_UNORDERED_ACCESS, subResource);

        frame->renderQueue << SetShaderUniform(0, "CurrentEnvProbe"_sh, g_renderInterface->gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frame->GetFrameIndex()), TShaderDataOffset<EnvProbeShaderData>(renderSetup.envProbe));
        frame->renderQueue << SetShaderUniform(1, "SphereSamplesBuffer"_sh, g_renderInterface->sphereSamplesBuffer);
        frame->renderQueue << SetShaderUniform(2, "ColorTexture"_sh, colorAttachment->GetImageView());
        frame->renderQueue << SetShaderUniform(3, "SamplerLinear"_sh, g_renderInterface->placeholderData->GetSamplerLinear());
        frame->renderQueue << SetShaderUniform(4, "SamplerNearest"_sh, g_renderInterface->placeholderData->GetSamplerNearest());
        frame->renderQueue << SetShaderUniform(5, "OutImage"_sh, imageView);
        frame->renderQueue << SetShaderUniform(6, "UniformBuffer"_sh, uniformBuffer);

        frame->renderQueue << DispatchCompute(Vec3u { (mipExtent.x + 7) / 8, (mipExtent.y + 7) / 8, 6 });

        frame->renderQueue << InsertBarrier(prefilteredEnvMap->GetGpuImage(), RS_SHADER_RESOURCE, subResource);
    }

    // Update in env probes texture array if bound
    if (envProbe->IsA(SkyProbe::StaticClass()) || envProbe->IsA(ReflectionProbe::StaticClass()))
    {
        const uint32 boundIndex = RetrieveResourceBinding(envProbe);

        if (boundIndex != ~0u)
        {
            // blit to the array texture
            const GpuImageRef& srcImage = prefilteredEnvMap->GetGpuImage();
            AssertDebug(srcImage.IsValid());

            const GpuImageRef& dstImage = g_renderInterface->envProbesTexture->GetGpuImage();
            Assert(dstImage.IsValid());

            frame->renderQueue << InsertBarrier(srcImage, RS_COPY_SRC);
            frame->renderQueue << InsertBarrier(dstImage, RS_COPY_DST);

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

                frame->renderQueue << Blit(
                    srcImage,
                    dstImage,
                    Rect<uint32> {
                        0, 0,
                        srcMipExtent.x, srcMipExtent.y },
                    Rect<uint32> {
                        0, 0,
                        dstMipExtent.x, dstMipExtent.y },
                    srcSubResource,
                    dstSubResource);
            }

            frame->renderQueue << InsertBarrier(srcImage, RS_SHADER_RESOURCE);
            frame->renderQueue << InsertBarrier(dstImage, RS_SHADER_RESOURCE);
        }
    }

    DelegateHandler* delegateHandle = new DelegateHandler();
    *delegateHandle = frame->OnFrameEnd.Bind([delegateHandle,
                                                 buffers = std::move(buffers)](...) mutable
        {
            EnqueueDeletion(std::move(buffers));

            delete delegateHandle;
        });
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
        const SizeType size = sizeof(SHTile) * (ShNumTiles.x >> i) * (ShNumTiles.y >> i);

        shTilesBuffers[i] = g_renderInterface->MakeGpuBuffer(GpuBufferType::STORAGE_BUFFER, size);
        shTilesBuffers[i]->SetRequireCpuAccessible(true);
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
        if (light->GetLightType() == LT_DIRECTIONAL)
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

    const Vec2u cubemapDimensions = colorAttachment->GetImage()->GetExtent().GetXY();

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

    RenderQueue& asyncRenderQueue = asyncCompute->renderQueue;

    asyncRenderQueue << InsertBarrier(shTilesBuffers[0], RS_UNORDERED_ACCESS, ShaderModuleType::Compute);
    asyncRenderQueue << InsertBarrier(g_renderInterface->gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frame->GetFrameIndex()), RS_UNORDERED_ACCESS, ShaderModuleType::Compute);

    // Helper to run pass
    auto RunPass = [&](Name mode, const SHUniforms& passUniforms, const Vec3u& dispatchGroupSize, const GpuBufferRef& inputBuffer, const GpuBufferRef& outputBuffer)
    {
        ShaderPropertySet passShaderProperties;
        passShaderProperties.Add(InternShaderProperty(ShaderProperty(NAME("MODE"), mode)));
        passShaderProperties = passShaderProperties | shaderProperties;

        ShaderDesc shaderDesc(NAME("ComputeSH"), passShaderProperties);
        asyncRenderQueue << SetCurrentShader(shaderDesc);

        GpuBufferRef ub = g_renderInterface->MakeGpuBuffer(GpuBufferType::CONSTANT_BUFFER, sizeof(SHUniforms));
        ub->Create();
        ub->Copy(sizeof(SHUniforms), &passUniforms);
        uniformBuffers.PushBack(ub);

        asyncRenderQueue << SetShaderUniform(0, "SamplerLinear"_sh, g_renderInterface->placeholderData->GetSamplerLinear());
        asyncRenderQueue << SetShaderUniform(1, "SamplerNearest"_sh, g_renderInterface->placeholderData->GetSamplerNearest());
        asyncRenderQueue << SetShaderUniform(2, "EnvProbesTexture"_sh, g_renderInterface->textureViewCache->GetOrCreate(g_renderInterface->envProbesTexture));
        asyncRenderQueue << SetShaderUniform(3, "EnvProbesBuffer"_sh, g_renderInterface->gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frame->GetFrameIndex()));

        if (skyProbe)
            asyncRenderQueue << SetShaderUniform(4, "CurrentEnvProbe"_sh, g_renderInterface->gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frame->GetFrameIndex()), TShaderDataOffset<EnvProbeShaderData>(skyProbe));
        else
            asyncRenderQueue << SetShaderUniform(4, "CurrentEnvProbe"_sh, g_renderInterface->gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frame->GetFrameIndex()), TShaderDataOffset<EnvProbeShaderData>(0));

        asyncRenderQueue << SetShaderUniform(5, "ShadowMapsTextureArray"_sh, g_renderInterface->shadowMapAllocator->GetAtlasImageView());
        asyncRenderQueue << SetShaderUniform(6, "PointLightShadowMapsTextureArray"_sh, g_renderInterface->shadowMapAllocator->GetPointLightShadowMapImageView());

        if (directionalLight)
            asyncRenderQueue << SetShaderUniform(7, "CurrentLight"_sh, g_renderInterface->gpuBuffers[GRB_LIGHTS]->GetBuffer(frame->GetFrameIndex()), TShaderDataOffset<LightShaderData>(directionalLight));
        else
            asyncRenderQueue << SetShaderUniform(7, "CurrentLight"_sh, g_renderInterface->gpuBuffers[GRB_LIGHTS]->GetBuffer(frame->GetFrameIndex()), TShaderDataOffset<LightShaderData>(0));

        asyncRenderQueue << SetShaderUniform(8, "InColorCubemap"_sh, colorAttachment->GetImageView());
        asyncRenderQueue << SetShaderUniform(9, "InNormalsCubemap"_sh, normalsAttachment ? normalsAttachment->GetImageView() : g_renderInterface->placeholderData->GetImageViewCube1x1R8());
        asyncRenderQueue << SetShaderUniform(10, "InDepthCubemap"_sh, depthAttachment ? depthAttachment->GetImageView() : g_renderInterface->placeholderData->GetImageViewCube1x1R8());
        asyncRenderQueue << SetShaderUniform(11, "InputSHTilesBuffer"_sh, inputBuffer);
        asyncRenderQueue << SetShaderUniform(12, "OutputSHTilesBuffer"_sh, outputBuffer);
        asyncRenderQueue << SetShaderUniform(13, "SHUniforms"_sh, ub);

        asyncRenderQueue << DispatchCompute(dispatchGroupSize);
    };

    // MODE_CLEAR
    RunPass(NAME("CLEAR"), uniforms, Vec3u { 1, 1, 1 }, shTilesBuffers[0], shTilesBuffers[1]);

    asyncRenderQueue << InsertBarrier(shTilesBuffers[0], RS_UNORDERED_ACCESS, ShaderModuleType::Compute);

    // MODE_BUILD_COEFFICIENTS
    RunPass(NAME("BUILD_COEFFICIENTS"), uniforms, Vec3u { 1, 1, 1 }, shTilesBuffers[0], shTilesBuffers[1]);

    // Parallel reduce
    if (ShParallelReduce)
    {
        for (uint32 i = 1; i < ShNumLevels; i++)
        {
            asyncRenderQueue << InsertBarrier(
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
    asyncRenderQueue << InsertBarrier(shTilesBuffers[finalizeShBufferIndex], RS_UNORDERED_ACCESS, ShaderModuleType::Compute);
    asyncRenderQueue << InsertBarrier(g_renderInterface->gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frame->GetFrameIndex()), RS_UNORDERED_ACCESS, ShaderModuleType::Compute);

    // MODE_FINALIZE
    RunPass(NAME("FINALIZE"), uniforms, Vec3u { 1, 1, 1 }, shTilesBuffers[finalizeShBufferIndex], shTilesBuffers[finalizeShBufferIndex]);

    asyncRenderQueue << InsertBarrier(g_renderInterface->gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frame->GetFrameIndex()), RS_UNORDERED_ACCESS, ShaderModuleType::Compute);

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
