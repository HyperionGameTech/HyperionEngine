/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Rendering/Pass.hpp>

#include <Core/Math/Mat4f.hpp>
#include <Core/Math/Vector2.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

class FullScreenPass;
class ShadowMap;

HYP_CLASS(NoScriptBindings)
class ShadowsPassData : public PassData
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
        FixedArray<ShadowMap*, MaxShadowMapCascades> shadowMaps;

        TFatArray<View*, FixedAllocator<6>> shadowViewsDynamic;
        TFatArray<View*, FixedAllocator<6>> shadowViewsStatic;

        FixedArray<FramebufferRef, 6> shadowMapFramebuffers;

        Handle<Texture> cachedShadowMapTexture;

        uint32 lastFrameUsed;
    };

    TMap<uint64, CachedShadowMapData, RenderAllocator> m_cachedShadowMapData;
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
