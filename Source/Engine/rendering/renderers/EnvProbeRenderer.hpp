/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/reflection/Handle.hpp>

#include <Core/math/Mat4f.hpp>

#include <rendering/RendererBase.hpp>
#include <rendering/RenderObject.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

class EnvProbe;
class Texture;

HYP_CLASS(NoScriptBindings)
class HYP_API EnvProbeRendererPassData : public PassData
{
    HYP_OBJECT_BODY(EnvProbeRendererPassData);

public:
    virtual ~EnvProbeRendererPassData() override = default;

    // for sky
    Vec4f cachedLightDirIntensity;
    Vec3f cachedProbeOrigin;
};

struct EnvProbeRendererPassDataExt : PassDataExt
{
    EnvProbe* envProbe = nullptr;

    EnvProbeRendererPassDataExt()
        : PassDataExt(TypeId::ForType<EnvProbeRendererPassDataExt>())
    {
    }

    virtual ~EnvProbeRendererPassDataExt() override = default;

    virtual PassDataExt* Clone() override
    {
        EnvProbeRendererPassDataExt* clone = new EnvProbeRendererPassDataExt;
        *clone = *this;

        return clone;
    }
};

class EnvProbeRenderer : public RendererBase
{
public:
    virtual ~EnvProbeRenderer() override;

    virtual void Initialize() override;
    virtual void Shutdown() override;

    virtual void RenderFrame(Frame* frame, const RenderSetup& renderSetup) override final;

protected:
    EnvProbeRenderer();

    virtual void RenderProbe(Frame* frame, const RenderSetup& renderSetup, EnvProbe* envProbe) = 0;

    PassData* CreateViewPassData(View* view, PassDataExt& ext) override;
};

class ReflectionProbeRenderer : public EnvProbeRenderer
{
public:
    ReflectionProbeRenderer();
    virtual ~ReflectionProbeRenderer() override;

    virtual void Initialize() override;
    virtual void Shutdown() override;

protected:
    virtual void RenderProbe(Frame* frame, const RenderSetup& renderSetup, EnvProbe* envProbe) override;

    void ComputePrefilteredEnvMap(Frame* frame, const RenderSetup& renderSetup, EnvProbe* envProbe);
    void ComputeSH(Frame* frame, const RenderSetup& renderSetup, EnvProbe* envProbe);
};

} // namespace Hyperion
