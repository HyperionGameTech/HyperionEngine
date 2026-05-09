/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <rendering/Pass.hpp>

#include <Core/math/Mat4f.hpp>
#include <Core/math/Vector2.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

class FullScreenPass;
class ShadowMap;

HYP_CLASS(NoScriptBindings)
class HYP_API ShadowsPassData : public PassData
{
    HYP_OBJECT_BODY(ShadowsPassData);

public:
    virtual ~ShadowsPassData() override;

    Array<Mat4f, RenderAllocator> prevCameraMatrices;
};

struct ShadowsPassDataExt : PassDataExt
{
    Light* light = nullptr;

    ShadowsPassDataExt()
        : PassDataExt(TypeId::ForType<ShadowsPassDataExt>())
    {
    }

    virtual ~ShadowsPassDataExt() override = default;

    virtual PassDataExt* Clone() override
    {
        ShadowsPassDataExt* clone = new ShadowsPassDataExt;
        *clone = *this;

        return clone;
    }
};

class ShadowsPassBase : public PassBase
{
public:
    virtual ~ShadowsPassBase() override = default;

    virtual void Initialize() override;
    virtual void Shutdown() override;

    virtual void RenderFrame(Frame* frame, const RenderSetup& renderSetup) override final;

protected:
    ShadowsPassBase();

    virtual int RunCleanupCycle(int maxIter) override;

    virtual PassData* CreateViewPassData(View* view, PassDataExt&) override;

private:
    struct CachedShadowMapData
    {
        Array<ShadowMap*, RenderAllocator> shadowMaps;

        FixedArray<View*, MaxShadowMapCascades> shadowViewsDynamic;
        FixedArray<View*, MaxShadowMapCascades> shadowViewsStatic;

        FixedArray<FramebufferRef, MaxShadowMapCascades> shadowMapFramebuffers;

        Handle<Texture> cachedShadowMapTexture;

        uint32 lastFrameUsed;
    };

    struct CacheKey
    {
        Light* light;
        View* view;

        HYP_FORCE_INLINE bool operator==(const CacheKey& other) const
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
    TMap<CacheKey, CachedShadowMapData, RenderAllocator> m_cachedShadowMapData;
};

class PointLightShadowsPass : public ShadowsPassBase
{
public:
    PointLightShadowsPass() = default;
    virtual ~PointLightShadowsPass() override = default;

protected:
};

class DirectionalLightShadowsPass : public ShadowsPassBase
{
public:
    DirectionalLightShadowsPass() = default;
    virtual ~DirectionalLightShadowsPass() override = default;

protected:
};

} // namespace Hyperion
