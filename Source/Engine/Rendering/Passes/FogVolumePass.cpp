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
#include <Rendering/CommandRecorder.hpp>

#include <Rendering/Shadows/ShadowMapCache.hpp>
#include <Rendering/Shadows/ShadowMap.hpp>

#include <Rendering/Util/DeletionQueue.hpp>
#include <Rendering/Util/MeshBuilder.hpp>
#include <Rendering/Util/ShaderPropertyDictionary.hpp>

#include <Scene/View.hpp>
#include <Scene/FogVolume.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Utilities/DeferredScope.hpp>

#include <Framework/EngineDriver.hpp>
#include <Framework/CVarManager.hpp>

namespace Hyperion {

static constexpr uint32 MaxFogLights = 4;

static StaticShaderPropertyId s_propUseClusteredLights { ShaderProperty(NAME("CLUSTERED_LIGHTS")) };
static StaticShaderPropertyId s_propFogVolumeUseSDF { ShaderProperty(NAME("FOG_VOLUME_USE_SDF")) };

extern CVar<bool> g_cvFogVolumesClusteredLights;

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

    m_volumeMesh = MeshBuilder::Cube();
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

    // Directional light
    LightShaderData directionalLightShaderData {};
    DirectionalLightCSMData directionalCSMData {};

    for (Light* light : rpl.GetLights())
    {
        if (light->GetLightType() == LightType::Directional)
        {
            RenderProxyLight* lightProxy = static_cast<RenderProxyLight*>(GetRenderProxy(light));
            AssertDebug(lightProxy != nullptr);

            directionalLightShaderData = lightProxy->bufferData;

            ShadowMap* shadowMaps[MaxShadowMapCascades] {};
            View* shadowMapViewsDynamic[MaxShadowMapCascades] {};
            View* shadowMapViewsStatic[MaxShadowMapCascades] {};

            uint32 numCascades = MathUtil::Clamp(lightProxy->numCascades, 1u, MaxShadowMapCascades);

            for (uint32 cascadeIndex = 0; cascadeIndex < numCascades; cascadeIndex++)
            {
                shadowMaps[cascadeIndex] = RI.shadowMapCache->GetShadowMap(
                    light,
                    renderSetup.view,
                    cascadeIndex,
                    shadowMapViewsDynamic[cascadeIndex],
                    shadowMapViewsStatic[cascadeIndex]);
            }

            DeferredRendererHelpers::FillShadowMapDataCSM(
                &directionalCSMData,
                shadowMapViewsDynamic,
                shadowMaps,
                numCascades);

            break;
        }
    }

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

    //if (rpl.GetFogVolumes().NumCurrent() == 1)
    //{
    //    // We don't want to cull front faces inside volume
    //    cr << SetFaceCullMode(FCM_FRONT);
    //}
    //else
    //{
        // Because multiple vols can overlap, we don't want to skip drawing backfaces
        cr << SetFaceCullMode(FCM_NONE);
    //}

    const bool useClusteredLights = g_cvFogVolumesClusteredLights.Get();

    {
        ShaderPropertySet fogShaderProperties;

        // fogShaderProperties.Add(s_propFogVolumeUseSDF);

        if (useClusteredLights)
        {
            fogShaderProperties.Add(s_propUseClusteredLights);
        }

        cr << SetCurrentShader(ShaderDesc(NAME("ApplyFogVolume"), fogShaderProperties));
    }

    cr << SetShaderUniform(0, "SamplerLinear"_sh, RI.placeholderData->GetSamplerLinearMipmap());
    cr << SetShaderUniform(1, "SamplerNearest"_sh, RI.placeholderData->GetSamplerNearest());

    cr << SetShaderUniform(2, "CamerasBuffer"_sh, RI.namedBuffers[NamedBuffer::Cameras], Resources::GetBinding(renderSetup.view->GetCamera()));

    cr << SetShaderUniform(3, "ShadowMapsTextureArray"_sh, RI.shadowMapCache->GetAtlasImageView());
    cr << SetShaderUniform(4, "PointLightShadowMapsTextureArray"_sh, RI.shadowMapCache->GetPointLightShadowMapImageView());

    cr << SetShaderUniform(5, "DepthTexture"_sh, RI.textureViewCache->GetOrCreate(dpd->depthPyramidRenderer->GetHZBTexture(), 2, 1));

    cr << SetShaderUniform(6, "BlueNoiseBuffer"_sh, RI.blueNoiseBuffer);

    cr << SetShaderUniform(7, "LightsBuffer"_sh, RI.namedBuffers[NamedBuffer::Lights]);
    cr << SetShaderUniform(8, "EnvProbesBuffer"_sh, RI.namedBuffers[NamedBuffer::EnvProbes]);

    cr << SetShaderUniform(9, "ClusterGridBuffer"_sh, *dpd->gridTilesBuffer);
    cr << SetShaderUniform(10, "ClusterIndexBuffer"_sh, *dpd->gridIndexBuffer);

    cr << SetShaderUniform(11, "ShadowMapIndexBuffer"_sh, *dpd->clusteredShadowMapIndexBuffer);

    LightShaderData fogLightData[MaxFogLights] {};
    ShadowMapData fogShadowMapData[MaxFogLights] {};
    uint32 numFogLights = 0;

    if (!useClusteredLights)
    {
        for (Light* light : rpl.GetLights())
        {
            const LightType lightType = light->GetLightType();

            if (lightType == LightType::Directional)
            {
                continue;
            }

            if (lightType != LightType::Point && lightType != LightType::Spot)
            {
                continue;
            }

            if (numFogLights >= MaxFogLights)
            {
                break;
            }

            RenderProxyLight* lightProxy = static_cast<RenderProxyLight*>(GetRenderProxy(light));
            AssertDebug(lightProxy != nullptr);

            fogLightData[numFogLights] = lightProxy->bufferData;

            View* shadowMapViewDynamic;
            View* shadowMapViewStatic;

            ShadowMap* shadowMap = RI.shadowMapCache->GetShadowMap(
                light,
                renderSetup.view,
                0,
                shadowMapViewDynamic,
                shadowMapViewStatic);

            if (shadowMap != nullptr)
            {
                DeferredRendererHelpers::FillShadowMapData(
                    fogShadowMapData[numFogLights],
                    *shadowMap,
                    0,
                    shadowMapViewDynamic,
                    shadowMapViewStatic);
            }

            ++numFogLights;
        }
    }

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
                cr << SetShaderUniform(12, "DataMap"_sh, RI.textureViewCache->GetOrCreate(data.volumeTexture));
            }

            if (data.noiseTexture)
            {
                cr << SetShaderUniform(13, "NoiseMap"_sh, RI.textureViewCache->GetOrCreate(data.noiseTexture));
            }

            FogVolumeShaderData shaderData = proxy->bufferData;

            if (!useClusteredLights)
            {
                shaderData.numBoundLights = numFogLights;
            }

            GpuBuffer* cbuffer = nullptr;
            size_t cbufferOffset = 0;
            size_t cbufferSize = 0;

            RI.cbufferAllocator->Write(&shaderData);
            RI.cbufferAllocator->Write(&directionalLightShaderData);
            RI.cbufferAllocator->Write(&directionalCSMData);

            if (useClusteredLights)
            {
                for (uint32 i = 0; i < MaxClusteredShadowMaps; i++)
                {
                    if (i < dpd->numClusteredShadowMaps)
                    {
                        RI.cbufferAllocator->Write(&dpd->clusteredShadowMaps[i]);
                    }
                    else
                    {
                        ShadowMapData dummy {};
                        RI.cbufferAllocator->Write(&dummy);
                    }
                }
            }
            else
            {
                for (uint32 i = 0; i < MaxFogLights; i++)
                {
                    RI.cbufferAllocator->Write(&fogLightData[i]);
                }

                for (uint32 i = 0; i < MaxFogLights; i++)
                {
                    RI.cbufferAllocator->Write(&fogShadowMapData[i]);
                }
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

            cr << SetShaderUniform(14, "FogVolumeConstants"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

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
