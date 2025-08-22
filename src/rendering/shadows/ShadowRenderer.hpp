/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <rendering/Renderer.hpp>

#include <core/math/Matrix4.hpp>
#include <core/math/Vector2.hpp>

#include <core/Types.hpp>

namespace hyperion {

class FullScreenPass;
class ShadowMap;

HYP_CLASS(NoScriptBindings)
class HYP_API ShadowPassData : public PassData
{
    HYP_OBJECT_BODY(ShadowPassData);

public:
    virtual ~ShadowPassData() override;
};

struct ShadowPassDataExt : PassDataExt
{
    Light* light = nullptr;

    ShadowPassDataExt()
        : PassDataExt(TypeId::ForType<ShadowPassDataExt>())
    {
    }

    virtual ~ShadowPassDataExt() override = default;

    virtual PassDataExt* Clone() override
    {
        ShadowPassDataExt* clone = new ShadowPassDataExt;
        clone->light = light;

        return clone;
    }
};

class ShadowRendererBase : public RendererBase
{
public:
    virtual ~ShadowRendererBase() override = default;

    virtual void Initialize() override;
    virtual void Shutdown() override;

    virtual void RenderFrame(FrameBase* frame, const RenderSetup& renderSetup) override final;

protected:
    ShadowRendererBase();

    virtual int RunCleanupCycle(int maxIter) override;

    virtual Handle<PassData> CreateViewPassData(View* view, PassDataExt&) override;

    virtual ShadowMap* AllocateShadowMap(Light* light) = 0;

private:
    // Shadow maps cached per-light.
    // Since Lights can have multiple shadow views that blit into one final shadow map
    // we store the shadow maps here rather than on the per-view PassData
    struct CachedShadowMapData
    {
        ShadowMap* shadowMap = nullptr;
        Handle<FullScreenPass> combineShadowMapsPass; // Pass to combine shadow maps for this light (optional)
        GpuImageRef combinedShadowMapsBlurred;
        ComputePipelineRef csBlurShadowMap; // compute pipeline for blurring VSM shadow maps

        ~CachedShadowMapData();
    };

    /// Cached per-light shadow map rendering data that is cleaned up when no longer used
    HashMap<WeakHandle<Light>, CachedShadowMapData> m_cachedShadowMapData;
};

class PointShadowRenderer : public ShadowRendererBase
{
public:
    PointShadowRenderer() = default;
    virtual ~PointShadowRenderer() override = default;

protected:
    virtual ShadowMap* AllocateShadowMap(Light* light) override;
};

class DirectionalShadowRenderer : public ShadowRendererBase
{
public:
    DirectionalShadowRenderer() = default;
    virtual ~DirectionalShadowRenderer() override = default;

protected:
    virtual ShadowMap* AllocateShadowMap(Light* light) override;
};

} // namespace hyperion
