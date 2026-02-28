/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <rendering/RendererBase.hpp>

#include <Core/math/Mat4f.hpp>
#include <Core/math/Vector2.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

class FullScreenPass;
class ShadowMap;

HYP_CLASS(NoScriptBindings)
class HYP_API ShadowRendererPassData : public PassData
{
    HYP_OBJECT_BODY(ShadowRendererPassData);

public:
    virtual ~ShadowRendererPassData() override;

    Array<Mat4f, InlineAllocator<2>> prevCameraMatrices;
};

struct ShadowRendererPassDataExt : PassDataExt
{
    Light* light = nullptr;

    ShadowRendererPassDataExt()
        : PassDataExt(TypeId::ForType<ShadowRendererPassDataExt>())
    {
    }

    virtual ~ShadowRendererPassDataExt() override = default;

    virtual PassDataExt* Clone() override
    {
        ShadowRendererPassDataExt* clone = new ShadowRendererPassDataExt;
        *clone = *this;

        return clone;
    }
};

class ShadowRendererBase : public RendererBase
{
public:
    virtual ~ShadowRendererBase() override = default;

    virtual void Initialize() override;
    virtual void Shutdown() override;

    virtual void RenderFrame(Frame* frame, const RenderSetup& renderSetup) override final;

protected:
    ShadowRendererBase();

    virtual int RunCleanupCycle(int maxIter) override;

    virtual PassData* CreateViewPassData(View* view, PassDataExt&) override;

    virtual ShadowMap* AllocateShadowMap(Light* light) = 0;

private:
    // Shadow maps cached per-light.
    // Since Lights can have multiple shadow views that blit into one final shadow map
    // we store the shadow maps here rather than on the per-view PassData
    struct CachedShadowMapData
    {
        ShadowMap* shadowMap = nullptr;
        UniquePtr<FullScreenPass> combineShadowMapsPass; // Pass to combine shadow maps for this light (optional)
        GpuImageRef combinedShadowMapsBlurred;
        FixedArray<GpuBufferRef, NumFramesInFlight> blurUniformBuffers;
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

} // namespace Hyperion
