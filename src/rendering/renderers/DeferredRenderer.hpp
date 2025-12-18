/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/RendererBase.hpp>
#include <rendering/FullScreenPass.hpp>
#include <rendering/PostFX.hpp>
#include <rendering/ParticleSystem.hpp>
#include <rendering/IndirectDraw.hpp>
#include <rendering/TemporalAA.hpp>
#include <rendering/GraphicsPipelineCache.hpp>
#include <rendering/RenderObject.hpp>

#include <rendering/raytracing/RaytracingReflections.hpp>

#include <core/reflection/ObjectBase.hpp>
#include <core/reflection/Handle.hpp>

#include <scene/Light.hpp> // For LightType

namespace hyperion {

class IndirectDrawState;
class RenderEnvironment;
class GBuffer;
class Texture;
class DepthPyramidRenderer;
class SSRRenderer;
class SSGI;
class ShaderProperties;
class View;
class DeferredRenderer;
class GBuffer;
class EnvGrid;
class EnvProbe;
class FullScreenPass;
class TemporalAA;
class PostProcessing;
class HBAO;
class DOFBlur;
class Texture;
class RaytracingReflections;
class DDGI;
struct RenderSetup;
class RenderGroup;
class EntityBatchAllocatorBase;
class RenderProxyList;
class RenderCollector;
enum LightType : uint32;
enum EnvProbeType : uint32;

using DeferredFlagBits = uint32;

enum DeferredFlags : DeferredFlagBits
{
    DEFERRED_FLAGS_NONE = 0x0,
    DEFERRED_FLAGS_VCT_ENABLED = 0x2,
    DEFERRED_FLAGS_ENV_PROBE_ENABLED = 0x4,
    DEFERRED_FLAGS_HBAO_ENABLED = 0x8,
    DEFERRED_FLAGS_HBIL_ENABLED = 0x10,
    DEFERRED_FLAGS_RT_RADIANCE_ENABLED = 0x20,
    DEFERRED_FLAGS_DDGI_ENABLED = 0x40
};

enum DeferredPassMode : uint32
{
    DPM_INDIRECT_LIGHTING,
    DPM_DIRECT_LIGHTING
};

enum CubemapType : uint32
{
    CMT_DEFAULT = 0,
    CMT_PARALLAX_CORRECTED,

    CMT_MAX
};

HYP_CLASS(NoScriptBindings)
class DeferredPass final : public FullScreenPass
{
    HYP_OBJECT_BODY(DeferredPass);

    friend class DeferredRenderer;

public:
    DeferredPass(DeferredPassMode mode, Vec2u extent, GBuffer* gbuffer, const FramebufferRef& framebuffer);
    DeferredPass(const DeferredPass& other) = delete;
    DeferredPass& operator=(const DeferredPass& other) = delete;
    virtual ~DeferredPass() override;

    virtual void Create() override;

protected:
    GraphicsPipelineCacheHandle CreatePipeline(const ShaderProperties& shaderProperties);
    virtual void RenderToFramebuffer_Internal(Frame* frame, const RenderSetup& rs, Framebuffer* framebuffer) override;

    virtual void Resize_Internal(Vec2u newSize) override;

private:
    const DeferredPassMode m_mode;

    FixedArray<GraphicsPipelineCacheHandle, LT_MAX> m_directLightGraphicsPipelines;

    Handle<Texture> m_ltcMatrixTexture;
    Handle<Texture> m_ltcBrdfTexture;
    SamplerRef m_ltcSampler;
};

enum EnvGridPassMode : uint8
{
    EGPM_RADIANCE,
    EGPM_IRRADIANCE,

    EGPM_MAX
};

enum EnvGridApplyMode : uint8
{
    EGAM_SH,
    EGAM_VOXEL,
    EGAM_LIGHT_FIELD,

    EGAM_MAX
};

HYP_CLASS(NoScriptBindings)
class TonemapPass final : public FullScreenPass
{
    HYP_OBJECT_BODY(TonemapPass);

public:
    TonemapPass(Vec2u extent, GBuffer* gbuffer);
    TonemapPass(const TonemapPass& other) = delete;
    TonemapPass& operator=(const TonemapPass& other) = delete;
    virtual ~TonemapPass() override;

    virtual void Create() override;
    virtual void Render(Frame* frame, const RenderSetup& rs) override;

protected:
    virtual void CreatePipeline() override;

private:
    virtual bool UsesTemporalBlending() const override
    {
        return false;
    }

    virtual bool ShouldRenderHalfRes() const override
    {
        return false;
    }

    virtual void Resize_Internal(Vec2u newSize) override;
};

HYP_CLASS(NoScriptBindings)
class LightmapPass final : public FullScreenPass
{
    HYP_OBJECT_BODY(LightmapPass);

public:
    LightmapPass();
    LightmapPass(const LightmapPass& other) = delete;
    LightmapPass& operator=(const LightmapPass& other) = delete;
    virtual ~LightmapPass() override;

    virtual void Create() override;

protected:
    struct LightmapVolumePassData
    {
        class LightmapVolume* volume = nullptr;
        Array<Texture*> atlasIrradianceTextures;
        Array<Texture*> atlasRadianceTextures;
        GraphicsPipelineCacheHandle graphicsPipeline;
        Array<DescriptorSetRef> descriptorSets;
    };

    virtual void RenderToFramebuffer_Internal(Frame* frame, const RenderSetup& renderSetup, Framebuffer* framebuffer) override;

    const GraphicsPipelineRef& GetGraphicsPipeline(Framebuffer* framebuffer, LightmapVolumePassData& data);

    LightmapVolumePassData& GetLightmapVolumePassData(LightmapVolume* lightmapVolume)
    {
        auto it = m_lightmapVolumePassData.FindIf([lightmapVolume](auto& item)
            {
                return item.volume == lightmapVolume;
            });

        if (it != m_lightmapVolumePassData.End())
        {
            return *it;
        }

        it = &m_lightmapVolumePassData.EmplaceBack();
        it->volume = lightmapVolume;

        return *it;
    }

    Array<LightmapVolumePassData, RenderAllocator> m_lightmapVolumePassData;

private:
    virtual bool UsesTemporalBlending() const override
    {
        return false;
    }

    virtual bool ShouldRenderHalfRes() const override
    {
        return false;
    }

    virtual void Resize_Internal(Vec2u newSize) override;
};

HYP_CLASS(NoScriptBindings)
class FogVolumePass final : public FullScreenPass
{
    HYP_OBJECT_BODY(FogVolumePass);

public:
    FogVolumePass();
    FogVolumePass(const FogVolumePass& other) = delete;
    FogVolumePass& operator=(const FogVolumePass& other) = delete;
    virtual ~FogVolumePass() override;

    virtual void Create() override;

protected:
    struct FogVolumePassData
    {
        class FogVolume* volume = nullptr;
        Texture* volumeTexture = nullptr;
        Texture* noiseTexture = nullptr;
        DescriptorTableRef descriptorTable;
        GpuBufferRef uniformBuffer;
        GraphicsPipelineCacheHandle graphicsPipeline;
    };

    virtual void RenderToFramebuffer_Internal(Frame* frame, const RenderSetup& renderSetup, Framebuffer* framebuffer) override;

    const GraphicsPipelineRef& GetGraphicsPipeline(Framebuffer* framebuffer, FogVolumePassData& data);

    void UpdateUniforms(Frame* frame, const RenderSetup& renderSetup, FogVolumePassData& data);

    FogVolumePassData& GetFogVolumePassData(FogVolume* fogVolume)
    {
        auto it = m_fogVolumePassData.FindIf([fogVolume](auto& item)
            {
                return item.volume == fogVolume;
            });

        if (it != m_fogVolumePassData.End())
        {
            return *it;
        }

        it = &m_fogVolumePassData.EmplaceBack();
        it->volume = fogVolume;

        return *it;
    }

    Array<FogVolumePassData, RenderAllocator> m_fogVolumePassData;
    Handle<Mesh> m_volumeMesh;

private:
    virtual bool UsesTemporalBlending() const override
    {
        return false;
    }

    virtual bool ShouldRenderHalfRes() const override
    {
        return false;
    }

    virtual void Resize_Internal(Vec2u newSize) override;
};

HYP_CLASS(NoScriptBindings)
class EnvGridPass final : public FullScreenPass
{
    HYP_OBJECT_BODY(EnvGridPass);

public:
    EnvGridPass(EnvGridPassMode mode, Vec2u extent, GBuffer* gbuffer);
    EnvGridPass(const EnvGridPass& other) = delete;
    EnvGridPass& operator=(const EnvGridPass& other) = delete;
    virtual ~EnvGridPass() override;

    virtual void Create() override;
    virtual void Render(Frame* frame, const RenderSetup& rs) override;

protected:
    virtual void CreatePipeline() override;
    virtual void RenderToFramebuffer_Internal(Frame* frame, const RenderSetup& rs, Framebuffer* framebuffer) override
    {
        HYP_NOT_IMPLEMENTED();
    }

private:
    virtual bool UsesTemporalBlending() const override
    {
        return false;
        // m_mode == EGPM_RADIANCE;
    }

    virtual bool ShouldRenderHalfRes() const override
    {
        return false;
    }

    virtual void Resize_Internal(Vec2u newSize) override;

    const EnvGridPassMode m_mode;
    FixedArray<GraphicsPipelineCacheHandle, EGAM_MAX> m_graphicsPipelines;
    bool m_isFirstFrame;
};

HYP_CLASS(NoScriptBindings)
class ReflectionsPass final : public FullScreenPass
{
    HYP_OBJECT_BODY(ReflectionsPass);

public:
    ReflectionsPass(Vec2u extent, GBuffer* gbuffer, const GpuImageViewRef& mipChainImageView, const GpuImageViewRef& deferredResultImageView);
    ReflectionsPass(const ReflectionsPass& other) = delete;
    ReflectionsPass& operator=(const ReflectionsPass& other) = delete;
    virtual ~ReflectionsPass() override;

    HYP_FORCE_INLINE const GpuImageViewRef& GetMipChainImageView() const
    {
        return m_mipChainImageView;
    }

    HYP_FORCE_INLINE const GpuImageViewRef& GetDeferredResultImageView() const
    {
        return m_deferredResultImageView;
    }

    HYP_FORCE_INLINE SSRRenderer* GetSSRRenderer() const
    {
        return m_ssrRenderer.Get();
    }

    bool ShouldRenderSSR() const;

    virtual void Create() override;
    virtual void Render(Frame* frame, const RenderSetup& rs) override;

private:
    virtual bool UsesTemporalBlending() const override
    {
        return false;
    }

    virtual bool ShouldRenderHalfRes() const override
    {
        return false;
    }

    virtual void CreatePipeline() override;
    virtual void CreatePipeline(const RenderableAttributeSet& renderableAttributes) override;

    void CreateSSRRenderer();

    virtual void RenderToFramebuffer_Internal(Frame* frame, const RenderSetup& rs, Framebuffer* framebuffer) override
    {
        HYP_NOT_IMPLEMENTED();
    }

    virtual void Resize_Internal(Vec2u newSize) override;

    GpuImageViewRef m_mipChainImageView;
    GpuImageViewRef m_deferredResultImageView;

    FixedArray<GraphicsPipelineCacheHandle, CMT_MAX> m_cubemapGraphicsPipelines;

    UniquePtr<SSRRenderer> m_ssrRenderer;

    Handle<FullScreenPass> m_renderSsrToScreenPass;
    Texture* m_cachedSsrTexture;

    bool m_isFirstFrame;
};

HYP_CLASS(NoScriptBindings)
class HYP_API DeferredRendererPassData : public PassData
{
    HYP_OBJECT_BODY(DeferredRendererPassData);

public:
    virtual ~DeferredRendererPassData() override;

    int priority = 0;

    // Descriptor set used when rendering the View in FinalPass.
    DescriptorSetRef finalPassDescriptorSet;

    Handle<Texture> mipChain;

    Handle<DeferredPass> indirectPass;
    Handle<DeferredPass> directPass;

    FramebufferRef deferredShadingFramebuffer;

    Handle<EnvGridPass> envGridRadiancePass;
    Handle<EnvGridPass> envGridIrradiancePass;

    Handle<ReflectionsPass> reflectionsPass;

    Handle<LightmapPass> lightmapPass;

    Handle<FogVolumePass> fogVolumePass;

    Handle<TonemapPass> tonemapPass;

    Handle<HBAO> hbao;
    Handle<FullScreenPass> combinePass;
    UniquePtr<PostProcessing> postProcessing;
    UniquePtr<TemporalAA> temporalAa;
    UniquePtr<SSGI> ssgi;
    UniquePtr<DepthPyramidRenderer> depthPyramidRenderer;
    UniquePtr<DOFBlur> dofBlur;

    UniquePtr<RaytracingReflections> raytracingReflections;
    UniquePtr<DDGI> ddgi;

    Texture* cachedSsrTexture = nullptr;
};

HYP_CLASS(NoScriptBindings)
class HYP_API RaytracingPassData : public PassData
{
    HYP_OBJECT_BODY(RaytracingPassData);

public:
    // Set only while rendering to this pass
    DeferredRendererPassData* parentPass = nullptr;

    FixedArray<GpuTlasRef, NumFramesInFlight> raytracingTlases;

    virtual ~RaytracingPassData() override;
};

class DeferredRenderer final : public RendererBase
{
public:
    struct LastFrameData
    {
        // View pass data from the most recent frame, sorted by View priority
        uint8 frameId = uint8(-1);

        // The pass data for the last frame (per-View), sorted by View priority.
        Array<Pair<View*, DeferredRendererPassData*>> passData;

        DeferredRendererPassData* GetPassDataForView(const View* view) const
        {
            for (const auto& pair : passData)
            {
                if (pair.first == view)
                {
                    return pair.second;
                }
            }

            return nullptr;
        }
    };

    DeferredRenderer();
    DeferredRenderer(const DeferredRenderer& other) = delete;
    DeferredRenderer& operator=(const DeferredRenderer& other) = delete;
    virtual ~DeferredRenderer() override;

    HYP_FORCE_INLINE const LastFrameData& GetLastFrameData() const
    {
        return m_lastFrameData;
    }

    HYP_FORCE_INLINE const RendererConfig& GetRendererConfig() const
    {
        return m_rendererConfig;
    }

    virtual void Initialize() override;
    virtual void Shutdown() override;

    virtual void RenderFrame(Frame* frame, const RenderSetup& rs) override;

private:
    void RenderFrameForView(Frame* frame, const RenderSetup& rs);
    void UpdateRaytracingView(Frame* frame, const RenderSetup& rs);

    // Called on initialization or when the view changes
    virtual Handle<PassData> CreateViewPassData(View* view, PassDataExt&) override;

    void CreateViewFinalPassDescriptorSet(View* view, DeferredRendererPassData& passData);
    void CreateViewDescriptorSets(View* view, DeferredRendererPassData& passData);
    void CreateViewCombinePass(View* view, DeferredRendererPassData& passData);
    void CreateViewRaytracingPasses(View* view, DeferredRendererPassData& passData);

    void CreateViewTopLevelAccelerationStructures(View* view, RaytracingPassData& passData);

    void ResizeView(Viewport viewport, View* view, DeferredRendererPassData& passData);

    void PerformOcclusionCulling(Frame* frame, const RenderSetup& rs, RenderCollector& renderCollector);
    void ExecuteDrawCalls(Frame* frame, const RenderSetup& rs, RenderCollector& renderCollector, uint32 bucketMask);
    void GenerateMipChain(Frame* frame, const RenderSetup& rs, RenderCollector& renderCollector, const GpuImageRef& srcImage);

    LastFrameData m_lastFrameData;

    RendererConfig m_rendererConfig;
};

} // namespace hyperion
