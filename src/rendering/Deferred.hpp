/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/Renderer.hpp>
#include <rendering/FullScreenPass.hpp>
#include <rendering/PostFX.hpp>
#include <rendering/ParticleSystem.hpp>
#include <rendering/IndirectDraw.hpp>
#include <rendering/rt/RaytracingReflections.hpp>
#include <rendering/TemporalAA.hpp>

#include <rendering/RenderObject.hpp>

#include <core/object/HypObject.hpp>

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
class EnvGridPass;
class EnvProbe;
class ReflectionsPass;
class TonemapPass;
class LightmapPass;
class FullScreenPass;
class TemporalAA;
class PostProcessing;
class DeferredPass;
class HBAO;
class DOFBlur;
class Texture;
class RaytracingReflections;
class DDGI;
struct RenderSetup;
class RenderGroup;
class IDrawCallCollectionImpl;
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

enum class DeferredPassMode
{
    INDIRECT_LIGHTING,
    DIRECT_LIGHTING
};

void GetDeferredShaderProperties(ShaderProperties& outShaderProperties);

HYP_CLASS(NoScriptBindings)
class DeferredPass final : public FullScreenPass
{
    HYP_OBJECT_BODY(DeferredPass);
    
    friend class DeferredRenderer;

public:
    DeferredPass(DeferredPassMode mode, Vec2u extent, GBuffer* gbuffer);
    DeferredPass(const DeferredPass& other) = delete;
    DeferredPass& operator=(const DeferredPass& other) = delete;
    virtual ~DeferredPass() override;

    virtual void Create() override;
    virtual void Render(FrameBase* frame, const RenderSetup& rs) override;

protected:
    virtual void CreatePipeline(const RenderableAttributeSet& renderableAttributes) override;

    virtual void Resize_Internal(Vec2u newSize) override;

private:
    const DeferredPassMode m_mode;

    FixedArray<GraphicsPipelineRef, LT_MAX> m_directLightGraphicsPipelines;

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
    virtual void Render(FrameBase* frame, const RenderSetup& rs) override;

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
    LightmapPass(const FramebufferRef& framebuffer, Vec2u extent, GBuffer* gbuffer);
    LightmapPass(const LightmapPass& other) = delete;
    LightmapPass& operator=(const LightmapPass& other) = delete;
    virtual ~LightmapPass() override;

    virtual void Create() override;
    virtual void Render(FrameBase* frame, const RenderSetup& rs) override;

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
class EnvGridPass final : public FullScreenPass
{
    HYP_OBJECT_BODY(EnvGridPass);
    
public:
    EnvGridPass(EnvGridPassMode mode, Vec2u extent, GBuffer* gbuffer);
    EnvGridPass(const EnvGridPass& other) = delete;
    EnvGridPass& operator=(const EnvGridPass& other) = delete;
    virtual ~EnvGridPass() override;

    virtual void Create() override;
    virtual void Render(FrameBase* frame, const RenderSetup& rs) override;

    virtual void RenderToFramebuffer(FrameBase* frame, const RenderSetup& rs, const FramebufferRef& framebuffer) override
    {
        HYP_NOT_IMPLEMENTED();
    }

protected:
    virtual void CreatePipeline() override;

private:
    virtual bool UsesTemporalBlending() const override
    {
        return m_mode == EGPM_RADIANCE;
    }

    virtual bool ShouldRenderHalfRes() const override
    {
        return false;
    }

    virtual void Resize_Internal(Vec2u newSize) override;

    const EnvGridPassMode m_mode;
    FixedArray<GraphicsPipelineRef, EGAM_MAX> m_graphicsPipelines;
    bool m_isFirstFrame;
};

HYP_CLASS(NoScriptBindings)
class ReflectionsPass final : public FullScreenPass
{
    HYP_OBJECT_BODY(ReflectionsPass);
    
    enum CubemapType : uint32
    {
        CMT_DEFAULT = 0,
        CMT_PARALLAX_CORRECTED,

        CMT_MAX
    };

public:
    ReflectionsPass(Vec2u extent, GBuffer* gbuffer, const ImageViewRef& mipChainImageView, const ImageViewRef& deferredResultImageView);
    ReflectionsPass(const ReflectionsPass& other) = delete;
    ReflectionsPass& operator=(const ReflectionsPass& other) = delete;
    virtual ~ReflectionsPass() override;

    HYP_FORCE_INLINE const ImageViewRef& GetMipChainImageView() const
    {
        return m_mipChainImageView;
    }

    HYP_FORCE_INLINE const ImageViewRef& GetDeferredResultImageView() const
    {
        return m_deferredResultImageView;
    }

    HYP_FORCE_INLINE SSRRenderer* GetSSRRenderer() const
    {
        return m_ssrRenderer.Get();
    }

    bool ShouldRenderSSR() const;

    virtual void Create() override;
    virtual void Render(FrameBase* frame, const RenderSetup& rs) override;

    virtual void RenderToFramebuffer(FrameBase* frame, const RenderSetup& rs, const FramebufferRef& framebuffer) override
    {
        HYP_NOT_IMPLEMENTED();
    }

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

    virtual void Resize_Internal(Vec2u newSize) override;

    ImageViewRef m_mipChainImageView;
    ImageViewRef m_deferredResultImageView;

    FixedArray<GraphicsPipelineRef, CMT_MAX> m_cubemapGraphicsPipelines;

    UniquePtr<SSRRenderer> m_ssrRenderer;

    Handle<FullScreenPass> m_renderSsrToScreenPass;

    bool m_isFirstFrame;
};

HYP_CLASS(NoScriptBindings)
class HYP_API DeferredPassData : public PassData
{
    HYP_OBJECT_BODY(DeferredPassData);

public:
    virtual ~DeferredPassData() override;

    int priority = 0;

    // Descriptor set used when rendering the View in FinalPass.
    DescriptorSetRef finalPassDescriptorSet;

    Handle<Texture> mipChain;

    Handle<DeferredPass> indirectPass;
    Handle<DeferredPass> directPass;
    Handle<EnvGridPass> envGridRadiancePass;
    Handle<EnvGridPass> envGridIrradiancePass;
    Handle<ReflectionsPass> reflectionsPass;
    Handle<LightmapPass> lightmapPass;
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
};

HYP_CLASS(NoScriptBindings)
class HYP_API RaytracingPassData : public PassData
{
    HYP_OBJECT_BODY(RaytracingPassData);

public:
    // Set only while rendering to this pass
    DeferredPassData* parentPass = nullptr;

    FixedArray<TLASRef, g_framesInFlight> raytracingTlases;

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
        Array<Pair<View*, DeferredPassData*>> passData;

        DeferredPassData* GetPassDataForView(const View* view) const
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

    virtual void RenderFrame(FrameBase* frame, const RenderSetup& rs) override;

private:
    void RenderFrameForView(FrameBase* frame, const RenderSetup& rs);
    void UpdateRaytracingView(FrameBase* frame, const RenderSetup& rs);

    // Called on initialization or when the view changes
    virtual Handle<PassData> CreateViewPassData(View* view, PassDataExt&) override;
    void CreateViewFinalPassDescriptorSet(View* view, DeferredPassData& passData);
    void CreateViewDescriptorSets(View* view, DeferredPassData& passData);
    void CreateViewCombinePass(View* view, DeferredPassData& passData);
    void CreateViewRaytracingPasses(View* view, DeferredPassData& passData);

    void CreateViewTopLevelAccelerationStructures(View* view, RaytracingPassData& passData);

    void ResizeView(Viewport viewport, View* view, DeferredPassData& passData);

    void PerformOcclusionCulling(FrameBase* frame, const RenderSetup& rs, RenderCollector& renderCollector);
    void ExecuteDrawCalls(FrameBase* frame, const RenderSetup& rs, RenderCollector& renderCollector, uint32 bucketMask);
    void GenerateMipChain(FrameBase* frame, const RenderSetup& rs, RenderCollector& renderCollector, const ImageRef& srcImage);

    LastFrameData m_lastFrameData;

    RendererConfig m_rendererConfig;
};

} // namespace hyperion
