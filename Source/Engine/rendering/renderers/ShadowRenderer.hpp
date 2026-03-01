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

    Array<Mat4f, RenderAllocator> prevCameraMatrices;
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
        Array<ShadowMap*, RenderAllocator> shadowMaps;
        
        FixedArray<View*, MaxShadowMapCascades> shadowViewsDynamic;
        FixedArray<View*, MaxShadowMapCascades> shadowViewsStatic;

        UniquePtr<FullScreenPass> combineShadowMapsPass; // Pass to combine shadow maps for this light (optional)

        GpuImageRef combinedShadowMapsBlurred;
        FixedArray<GpuBufferRef, NumFramesInFlight> blurUniformBuffers;

        uint32 lastFrameUsed;
    };

    struct CacheKey
    {
        Light* light;
        View* view;

        HYP_FORCE_INLINE bool operator==(const CacheKey& other)
        {
            return light == other.light
                && view == other.view;
        }

        HYP_FORCE_INLINE HashCode GetHashCode() const
        {
            return HashCode::GetHashCode(light)
                .Combine(view);
        }
    };

    /// Cached (per-light/view combination) shadow map rendering data that is cleaned up when no longer used
    HashMap<CacheKey, CachedShadowMapData, NodeAllocator<RenderAllocator>> m_cachedShadowMapData;
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
