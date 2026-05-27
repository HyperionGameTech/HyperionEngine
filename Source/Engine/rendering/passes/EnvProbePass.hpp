/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/reflection/Handle.hpp>

#include <Core/math/Mat4f.hpp>

#include <rendering/Pass.hpp>
#include <rendering/RenderTypes.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

class EnvProbe;
class Texture;

HYP_CLASS(NoScriptBindings)
class HYP_API EnvProbePassData : public PassData
{
    HYP_OBJECT_BODY(EnvProbePassData);

public:
    virtual ~EnvProbePassData() override = default;

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

class EnvProbePassBase : public PassBase
{
public:
    virtual ~EnvProbePassBase() override;

    virtual void Initialize() override;
    virtual void Shutdown() override;

    virtual void RenderFrame(Frame* frame, const RenderSetup& renderSetup) override final;

protected:
    EnvProbePassBase();

    virtual void RenderProbe(Frame* frame, const RenderSetup& renderSetup, EnvProbe* envProbe) = 0;

    PassData* CreateViewPassData(View* view, PassDataExt& ext) override;
};

class ReflectionProbePass : public EnvProbePassBase
{
public:
    ReflectionProbePass();
    virtual ~ReflectionProbePass() override;

    virtual void Initialize() override;
    virtual void Shutdown() override;

    static void ComputePrefilteredEnvMap(Frame* frame, const RenderSetup& renderSetup, EnvProbe* envProbe);
    static void ComputeSH(Frame* frame, EnvProbe* envProbe);

protected:
    virtual void RenderProbe(Frame* frame, const RenderSetup& renderSetup, EnvProbe* envProbe) override;

    void RenderProbeView(Frame* frame, const RenderSetup& renderSetup, EnvProbe* envProbe);
};

} // namespace Hyperion
