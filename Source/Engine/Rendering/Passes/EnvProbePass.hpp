/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Reflection/Handle.hpp>

#include <Core/Math/Mat4f.hpp>

#include <Core/Utilities/ClockTimer.hpp>

#include <Rendering/Pass.hpp>
#include <Rendering/RenderTypes.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

class EnvProbe;
class Texture;

HYP_CLASS(NoScriptBindings)
class EnvProbePassData : public PassData
{
    HYP_OBJECT_BODY(EnvProbePassData);

public:
    virtual ~EnvProbePassData() override = default;

    // for sky
    Vec4f cachedLightDirIntensity;
    Vec3f cachedProbeOrigin;

    // whether the last sky capture had clouds composited in, and how long ago that was
    bool hasCompositedClouds = false;
    ClockTimer cloudRefreshTimer;
};

class EnvProbePassBase : public PassBase
{
public:
    void Initialize()
    {
    }

    void Shutdown()
    {
    }

    void RenderFrame(Frame* frame, const RenderSetup& renderSetup) override final;

protected:
    virtual void RenderProbe(Frame* frame, const RenderSetup& renderSetup, EnvProbe* envProbe) = 0;

    PassData* CreateViewPassData(View* view, PassDataExt& ext) override;
};

class ReflectionProbePass final : public EnvProbePassBase
{
protected:
    void RenderProbe(Frame* frame, const RenderSetup& renderSetup, EnvProbe* envProbe) override;

    void RenderProbeView(Frame* frame, const RenderSetup& renderSetup, EnvProbe* envProbe);
};

class IrradianceProbePass final : public EnvProbePassBase
{
protected:
    void RenderProbe(Frame* frame, const RenderSetup& renderSetup, EnvProbe* envProbe) override;

    void RenderProbeView(Frame* frame, const RenderSetup& renderSetup, EnvProbe* envProbe);
};

} // namespace Hyperion
