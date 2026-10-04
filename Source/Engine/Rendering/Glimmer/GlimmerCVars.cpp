/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/GlimmerCVars.hpp>
#include <Rendering/RenderInterface.hpp>

#include <Framework/DeviceDetails.hpp>

namespace Hyperion {

CVar<bool> g_cvGlimmerEnabled("Rendering.Glimmer.Enabled", true);
CVar<float> g_cvGlimmerIntensity("Rendering.Glimmer.Intensity", 1.0f);
CVar<int> g_cvGlimmerDebugView("Rendering.Glimmer.DebugView", 0);
CVar<bool> g_cvGlimmerHalfRes("Rendering.Glimmer.HalfRes", true);
CVar<bool> g_cvGlimmerSpecularOcclusion("Rendering.Glimmer.SpecularOcclusion", true);
CVar<int> g_cvGlimmerDebugSH("Rendering.Glimmer.DebugSH", 0);
CVar<float> g_cvGlimmerVisibility("Rendering.Glimmer.Visibility", 1.0f);

CVar<float> g_cvGlimmerGroundAlbedo("Rendering.Glimmer.Ground.Albedo", 0.25f);

// Stopgap: integrated GPUs hit a device timeout building Glimmer's BLAS pool
static bool IsGlimmerSupportedOnDevice()
{
    const DeviceDetails& device = RI.deviceDetails;

    if (device.IsDiscreteGpu())
    {
        return true;
    }

    static bool s_hasLogged = false;

    if (!s_hasLogged)
    {
        s_hasLogged = true;
        HYP_LOG(Rendering, Warning, "Glimmer GI is disabled on integrated GPU '{}'", device.GetGpuModel());
    }

    return false;
}

bool IsGlimmerSceneRequired()
{
    return IsGlimmerSupportedOnDevice()
        && (g_cvGlimmerEnabled.Get() || g_cvGlimmerDebugView.Get() >= int(GlimmerDebugView::TechniqueFirst));
}

} // namespace Hyperion
