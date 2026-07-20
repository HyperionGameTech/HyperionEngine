/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Passes/FogVolumePass.hpp>
#include <Rendering/Passes/DeferredPass.hpp>
#include <Rendering/Passes/DeferredPassShared.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/GBuffer.hpp>
#include <Rendering/Mesh.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/TextureViewCache.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/RenderTypes.hpp>
#include <Rendering/RenderProxyList.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/GpuBuffer.hpp>
#include <Rendering/CBufferAllocator.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/DepthPyramidRenderer.hpp>

#include <Rendering/Shadows/ShadowMapCache.hpp>
#include <Rendering/Shadows/ShadowMap.hpp>

#include <Rendering/Util/DeletionQueue.hpp>
#include <Rendering/Util/MeshBuilder.hpp>

#include <Scene/View.hpp>
#include <Scene/FogVolume.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Utilities/DeferredScope.hpp>

#include <Framework/EngineDriver.hpp>

namespace Hyperion {

static constexpr uint32 MaxBoundLightsPerFogVolume = 4;

#pragma region FogVolumePass

FogVolumePass::FogVolumePass(Vec2u extent, GBuffer* gbuffer)
    : FullScreenPass(TextureFormat::RGBA16F, MathUtil::Max(extent / 4, Vec2u::One()), gbuffer)
{
    SetPassName(NAME("FogVolume"));
}

FogVolumePass::~FogVolumePass()
{
}

void FogVolumePass::Create()
{
    AssertOnThread(g_renderThread);

    m_volumeMesh = MeshBuilder::Cube(true);
    m_volumeMesh->SetIsTransient(true);
    m_volumeMesh->SetFlags(MeshFlags::ViewIndependent);
    m_volumeMesh->SetName(NAME("FogVolumeMesh"));
    m_volumeMesh->UploadGpuData();

    m_shaderDesc = ShaderDesc(NAME("ApplyFogVolume"));

    FullScreenPass::Create();

    // Upsampling
    for (uint32 i = 0; i < NumUpsamplePasses; i++)
    {
        const bool isLast = i == NumUpsamplePasses - 1;

        Vec2u targetExtent = m_gbuffer->GetExtent();

        if (!isLast)
        {
            targetExtent /= (2 * (NumUpsamplePasses - i - 1));
        }

        targetExtent = MathUtil::Max(targetExtent, Vec2u::One());

        const TextureFormat format = GetFormat();

        m_upsamplePasses[i] = MakeUnique<FullScreenPass>(
            format,
            targetExtent,
            nullptr,
            isLast ? FSP_EXTERNAL_RENDERTARGET : FSP_NONE);

        m_upsamplePasses[i]->SetShaderDesc(ShaderDesc(NAME("Upsample"), ShaderPropertySet {}));
        m_upsamplePasses[i]->Create();
    }
}

void FogVolumePass::Resize_Internal(Vec2u newSize)
{
    FullScreenPass::Resize_Internal(newSize);
}

void FogVolumePass::Render(Frame* frame, const RenderSetup& renderSetup)
{
    AssertDebug(renderSetup.world && renderSetup.view && renderSetup.framebuffer);

    RenderProxyList& rpl = GetConsumerProxyList(renderSetup.view);
    rpl.BeginRead();

    HYP_DEFER({ rpl.EndRead(); });

    if (rpl.GetFogVolumes().NumCurrent() == 0)
    {
        return;
    }

    RenderProxyCamera* cameraProxy = static_cast<RenderProxyCamera*>(GetRenderProxy(renderSetup.view->GetCamera()));
    if (!cameraProxy)
    {
        return;
    }

    DeferredPassData* dpd = DynamicCast<DeferredPassData>(renderSetup.passData);
    AssertDebug(dpd != nullptr);

    Attachment* normalsAttachment = m_gbuffer->GetPass(GBufferPass::Opaque).GetAttachment(GBufferTarget::Normals);

    CommandRecorder& cr = frame->cr;

    cr << SetFillMode(FM_FILL);
    cr << SetDepthWrite(false);
    cr << SetDepthTest(false);
    cr << SetStencilTest(false);
    cr << SetCurrentBlendFunction(BlendFunction::None());

    cr << SetCurrentViewport(Viewport { m_extent, renderSetup.viewport.position });

    cr << SetCurrentFramebuffer(m_framebuffer);

    cr << SetTopology(m_volumeMesh->GetMeshAttributes().topology);
    cr << SetInputLayout(m_volumeMesh->GetMeshAttributes().inputLayout);

    cr << SetFaceCullMode(FCM_FRONT); // cull front faces to render inside of the volume

    cr << SetCurrentShader(m_shaderDesc);

    cr << SetShaderUniform(0, "SamplerLinear"_sh, RI.placeholderData->GetSamplerLinearMipmap());
    cr << SetShaderUniform(1, "SamplerNearest"_sh, RI.placeholderData->GetSamplerNearest());

    cr << SetShaderUniform(2, "CamerasBuffer"_sh, RI.namedBuffers[NamedBuffer::Cameras], Resources::GetBinding(renderSetup.view->GetCamera()));

    cr << SetShaderUniform(3, "ShadowMapsTextureArray"_sh, RI.shadowMapCache->GetAtlasImageView());
    cr << SetShaderUniform(4, "PointLightShadowMapsTextureArray"_sh, RI.shadowMapCache->GetPointLightShadowMapImageView());

    // Select second mip - since we're quarter res
    cr << SetShaderUniform(5, "DepthTexture"_sh, RI.textureViewCache->GetOrCreate(dpd->depthPyramidRenderer->GetHZBTexture(), 2, 1));

    cr << SetShaderUniform(6, "BlueNoiseBuffer"_sh, RI.blueNoiseBuffer);

    cr << SetShaderUniform(7, "LightsBuffer"_sh, RI.namedBuffers[NamedBuffer::Lights]);
    cr << SetShaderUniform(8, "EnvProbesBuffer"_sh, RI.namedBuffers[NamedBuffer::EnvProbes]);

    cr << SetShaderUniform(9, "ClusterGridBuffer"_sh, *dpd->gridTilesBuffer);
    cr << SetShaderUniform(10, "ClusterIndexBuffer"_sh, *dpd->gridIndexBuffer);

    for (FogVolume* volume : rpl.GetFogVolumes())
    {

        RenderProxyFogVolume* proxy = static_cast<RenderProxyFogVolume*>(GetRenderProxy(volume));
        Assert(proxy != nullptr);

        FogVolumePassData& data = GetFogVolumePassData(volume);
        data.noiseTexture = proxy->noiseTexture;
        data.volumeTexture = proxy->volumeTexture;

        {
            if (data.volumeTexture)
            {
                cr << SetShaderUniform(11, "DataMap"_sh, RI.textureViewCache->GetOrCreate(data.volumeTexture));
            }

            if (data.noiseTexture)
            {
                cr << SetShaderUniform(12, "NoiseMap"_sh, RI.textureViewCache->GetOrCreate(data.noiseTexture));
            }

            // Set constants
            FogVolumeShaderData shaderData = proxy->bufferData;

            uint32& numBoundLights = shaderData.numBoundLights;
            numBoundLights = 0;

            uint32* lightIndicesU32 = reinterpret_cast<uint32*>(shaderData.lightIndices);

            RenderProxyList& rpl = GetConsumerProxyList(renderSetup.view);

            Array<Pair<Light*, LightShaderData*>, RenderAllocator> tempLightsArray;

            for (Light* light : rpl.GetLights())
            {
                const LightType lightType = light->GetLightType();

                if (lightType != LightType::Directional && lightType != LightType::Point)
                {
                    continue;
                }

                if (numBoundLights >= MaxBoundLightsPerFogVolume)
                {
                    break;
                }

                RenderProxyLight* lightProxy = static_cast<RenderProxyLight*>(GetRenderProxy(light));
                Assert(lightProxy != nullptr);

                tempLightsArray.EmplaceBack(light, &lightProxy->bufferData);

                lightIndicesU32[numBoundLights++] = Resources::GetBinding(light);
            }

            GpuBuffer* cbuffer = nullptr;
            size_t cbufferOffset = 0;
            size_t cbufferSize = 0;

            RI.cbufferAllocator->Write(&shaderData);

            for (uint32 i = 0; i < MaxBoundLightsPerFogVolume; i++)
            {
                if (i < uint32(tempLightsArray.Size()))
                {
                    RI.cbufferAllocator->Write(tempLightsArray[i].second);
                    continue;
                }

                LightShaderData dummy {};
                RI.cbufferAllocator->Write(&dummy);
            }

            // @TODO Change to use cluster grid instead.
            // Then we can have the same (point) lights we have for deferred rendering,
            // as well as env probes for sampling irradiance for the fog.
            for (uint32 i = 0; i < MaxBoundLightsPerFogVolume; i++)
            {
                ShadowMapData shadowMapData {};

                if (i < uint32(tempLightsArray.Size()))
                {
                    View* shadowMapViewDynamic;
                    View* shadowMapViewStatic;

                    Light* light = tempLightsArray[i].first;

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

            const Vec2i screenDimensions = Vec2i(m_extent);
            RI.cbufferAllocator->Write(&screenDimensions);

            const float stepSize = 0.125f;
            RI.cbufferAllocator->Write(&stepSize);

            const uint32 maxSteps = 256;
            RI.cbufferAllocator->Write(&maxSteps);

            const uint32 frameCounter = GetFrameCounter();
            RI.cbufferAllocator->Write(&frameCounter);

            RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

            cr << SetShaderUniform(13, "FogVolumeConstants"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

            cr << CommitDrawState();

            cr << BindVertexBuffer(m_volumeMesh->GetVertexBuffer());
            cr << BindIndexBuffer(m_volumeMesh->GetIndexBuffer());
            cr << DrawIndexed(36); // draw cube
        }
    }

    cr << SetFaceCullMode(FCM_NONE);

    // Now upsampling passes
    for (uint32 i = 0; i < NumUpsamplePasses; i++)
    {
        const bool isFirst = i == 0;
        const bool isLast = i == NumUpsamplePasses - 1;

        FullScreenPass* pass = m_upsamplePasses[i].Get();

        const Vec2f sourceResolution = MathUtil::Max(Vec2f(pass->GetExtent()) / 2, Vec2f::One());

        // Need new cbuffer
        GpuBuffer* cbuffer = nullptr;
        size_t cbufferSize = 0;
        size_t cbufferOffset = 0;

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
            upsampleConstants.depthThreshold = 0.1f;
            upsampleConstants.normalThreshold = 1.0f;

            RI.cbufferAllocator->Write(&upsampleConstants);
            RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);
        }

        cr << SetCurrentShader(pass->GetShaderDesc());

        // if last - direct to framebuffer
        if (isLast)
        {
            cr << SetCurrentBlendFunction(BlendFunction::Additive());
            cr << SetCurrentFramebuffer(renderSetup.framebuffer);
            cr << SetCurrentViewport(renderSetup.viewport);

        }
        else
        {
            cr << SetCurrentFramebuffer(pass->GetFramebuffer());
            cr << SetCurrentViewport(Viewport { pass->GetExtent() });
        }

        uint32 numShaderUniforms = 0;

        // Samplers
        cr << SetShaderUniform(numShaderUniforms++, "SamplerLinear"_sh, RI.placeholderData->GetSamplerLinear());
        cr << SetShaderUniform(numShaderUniforms++, "SamplerNearest"_sh, RI.placeholderData->GetSamplerNearest());

        // GBuffer textures
        cr << SetShaderUniform(numShaderUniforms++, "NormalsTexture"_sh, normalsAttachment->GetImageView());

        cr << SetShaderUniform(
            numShaderUniforms++,
            "DepthTexture"_sh,
            RI.textureViewCache->GetOrCreate(dpd->depthPyramidRenderer->GetHZBTexture(), NumUpsamplePasses - i - 1, 1));

        cr << SetShaderUniform(
            numShaderUniforms++,
            "PrevPassTexture"_sh,
            isFirst ? m_framebuffer->GetAttachment(0)->GetImageView() : m_upsamplePasses[i - 1]->GetAttachment(0)->GetImageView());

        cr << SetShaderUniform(numShaderUniforms++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

        cr << CommitDrawState();

        // Draw quad
        pass->RenderFullScreenQuad(frame, renderSetup);
    }

    // reset states
    cr << SetCurrentBlendFunction(BlendFunction::None());
    cr << SetDepthTest(true);
    cr << SetDepthWrite(true);

    m_isFirstFrame = false;
}

#pragma endregion FogVolumePass

} // namespace Hyperion
