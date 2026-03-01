/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <rendering/RendererBase.hpp>
#include <rendering/FullScreenPass.hpp>
#include <rendering/PostFX.hpp>
#include <rendering/IndirectDraw.hpp>
#include <rendering/TAAPass.hpp>
#include <rendering/GraphicsPipelineCache.hpp>
#include <rendering/RenderObject.hpp>

#include <Core/reflection/ObjectBase.hpp>
#include <Core/reflection/Handle.hpp>

#include <scene/Light.hpp> // For LightType

namespace Hyperion {

class IndirectDrawState;
class GBuffer;
class Texture;
class DepthPyramidRenderer;
class SSRRenderer;
class SSGI;
class View;
class DeferredRenderer;
class GBuffer;
class EnvGrid;
class EnvProbe;
class FullScreenPass;
class TAAPass;
class PostProcessing;
class HBAO;
class DOFBlur;
class Texture;
class RayTracingReflections;
class DDGI;
struct RenderSetup;
class RenderGroup;
class EntityBatchAllocatorBase;
class RenderProxyList;
class RenderCollector;

enum class LightType : uint32;
enum EnvProbeType : uint32;

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

class DeferredPass final : public FullScreenPass
{
    friend class DeferredRenderer;

public:
    DeferredPass(DeferredPassMode mode, Vec2u extent, GBuffer* gbuffer, const FramebufferRef& framebuffer);
    DeferredPass(const DeferredPass& other) = delete;
    DeferredPass& operator=(const DeferredPass& other) = delete;
    virtual ~DeferredPass() override;

    virtual void Create() override;

protected:
    virtual void RenderToFramebuffer_Internal(Frame* frame, const RenderSetup& rs, Framebuffer* framebuffer) override;

    virtual void Resize_Internal(Vec2u newSize) override;

private:
    const DeferredPassMode m_mode;

    Handle<Texture> m_ltcMatrixTexture;
    Handle<Texture> m_ltcBrdfTexture;
    SamplerRef m_ltcSampler;
};

class TonemapPass final : public FullScreenPass
{
public:
    TonemapPass(Vec2u extent, GBuffer* gbuffer);
    TonemapPass(const TonemapPass& other) = delete;
    TonemapPass& operator=(const TonemapPass& other) = delete;
    virtual ~TonemapPass() override;

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

    virtual void Resize_Internal(Vec2u newSize) override;
};

class LightmapPass final : public FullScreenPass
{
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
        Array<GpuBufferRef> uniformBuffers;
    };

    virtual void RenderToFramebuffer_Internal(Frame* frame, const RenderSetup& renderSetup, Framebuffer* framebuffer) override;

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

class FogVolumePass final : public FullScreenPass
{
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
        GpuBufferRef cBuffer;
    };

    virtual void RenderToFramebuffer_Internal(Frame* frame, const RenderSetup& renderSetup, Framebuffer* framebuffer) override;

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

class ReflectionsPass final : public FullScreenPass
{
public:
    ReflectionsPass(Vec2u extent, GBuffer* gbuffer, const GpuImageViewRef& mipChainImageView);
    ReflectionsPass(const ReflectionsPass& other) = delete;
    ReflectionsPass& operator=(const ReflectionsPass& other) = delete;
    virtual ~ReflectionsPass() override;

    HYP_FORCE_INLINE const GpuImageViewRef& GetMipChainImageView() const
    {
        return m_mipChainImageView;
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

    void CreateSSRRenderer();

    virtual void RenderToFramebuffer_Internal(Frame* frame, const RenderSetup& rs, Framebuffer* framebuffer) override
    {
        HYP_NOT_IMPLEMENTED();
    }

    virtual void Resize_Internal(Vec2u newSize) override;

    GpuImageViewRef m_mipChainImageView;

    UniquePtr<SSRRenderer> m_ssrRenderer;

    bool m_isFirstFrame;
};

HYP_CLASS(NoScriptBindings)
class HYP_API DeferredRendererPassData : public PassData
{
    HYP_OBJECT_BODY(DeferredRendererPassData);

public:
    virtual ~DeferredRendererPassData() override;

    int priority = 0;

    Handle<Texture> mipChain;

    UniquePtr<DeferredPass> indirectPass;
    UniquePtr<DeferredPass> directPass;

    FramebufferRef deferredShadingFramebuffer;

    UniquePtr<ReflectionsPass> reflectionsPass;

    UniquePtr<LightmapPass> lightmapPass;

    UniquePtr<FogVolumePass> fogVolumePass;

    UniquePtr<TonemapPass> tonemapPass;

    UniquePtr<HBAO> hbao;
    UniquePtr<FullScreenPass> combinePass;
    UniquePtr<PostProcessing> postProcessing;
    UniquePtr<TAAPass> taaPass;
    UniquePtr<SSGI> ssgi;
    UniquePtr<DepthPyramidRenderer> depthPyramidRenderer;
    UniquePtr<DOFBlur> dofBlur;

    UniquePtr<RayTracingReflections> rayTracingReflections;
    UniquePtr<DDGI> ddgi;

    mutable Texture* cachedSsrTexture = nullptr;
};

HYP_CLASS(NoScriptBindings)
class HYP_API RayTracingPassData : public PassData
{
    HYP_OBJECT_BODY(RayTracingPassData);

public:
    // Set only while rendering to this pass
    DeferredRendererPassData* parentPass = nullptr;
    
    GpuBufferRef cBuffer;
    GpuBufferRef lightsBuffer;
    FixedArray<GpuTlasRef, NumFramesInFlight> rayTracingTlases;

    virtual ~RayTracingPassData() override;
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
    void UpdateRayTracingView(Frame* frame, const RenderSetup& rs);

    // Called on initialization or when the view changes
    virtual PassData* CreateViewPassData(View* view, PassDataExt&) override;

    void CreateViewRayTracingPasses(View* view, DeferredRendererPassData& passData);

    void CreateViewTopLevelAccelerationStructures(View* view, RayTracingPassData& passData);

    void ResizeView(Viewport viewport, View* view, DeferredRendererPassData& passData);

    void PerformOcclusionCulling(Frame* frame, const RenderSetup& rs, RenderCollector& renderCollector);
    void ExecuteDrawCalls(Frame* frame, const RenderSetup& rs, RenderCollector& renderCollector, uint32 bucketMask);
    void GenerateMipChain(Frame* frame, const RenderSetup& rs, RenderCollector& renderCollector, const GpuImageRef& srcImage);

    LastFrameData m_lastFrameData;

    RendererConfig m_rendererConfig;

    Handle<Mesh> m_quadMesh;
};

} // namespace Hyperion
