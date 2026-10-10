/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/GlimmerCVars.hpp>

namespace Hyperion {

CVar<bool> g_cvGlimmerEnabled("Rendering.Glimmer.Enabled", true);
CVar<float> g_cvGlimmerIntensity("Rendering.Glimmer.Intensity", 1.0f);
CVar<int> g_cvGlimmerDebugView("Rendering.Glimmer.DebugView", 0);
CVar<bool> g_cvGlimmerHalfRes("Rendering.Glimmer.HalfRes", true);
CVar<bool> g_cvGlimmerSpecularOcclusion("Rendering.Glimmer.SpecularOcclusion", true);
CVar<int> g_cvGlimmerDebugSH("Rendering.Glimmer.DebugSH", 0);
CVar<float> g_cvGlimmerVisibility("Rendering.Glimmer.Visibility", 1.0f);

CVar<float> g_cvGlimmerGroundAlbedo("Rendering.Glimmer.Ground.Albedo", 0.25f);

CVar<bool> g_cvGlimmerRunOnChange("Rendering.Glimmer.RunOnChange", true);
CVar<bool> g_cvGlimmerSkipLodOnlyChanges("Rendering.Glimmer.SkipLodOnlyChanges", true);
CVar<float> g_cvGlimmerSunAngleThreshold("Rendering.Glimmer.SunAngleThreshold", 0.5f);
CVar<int> g_cvGlimmerSHRefreshVoxels("Rendering.Glimmer.SH.RefreshVoxels", 64);
CVar<int> g_cvGlimmerSHRefreshVoxelsBurst("Rendering.Glimmer.SH.RefreshVoxelsBurst", 1024);
CVar<int> g_cvGlimmerRelightTexels("Rendering.Glimmer.Relight.Texels", 512);
CVar<int> g_cvGlimmerRelightTexelsBurst("Rendering.Glimmer.Relight.TexelsBurst", 8192);

bool IsGlimmerSceneRequired()
{
    return g_cvGlimmerEnabled.Get() || g_cvGlimmerDebugView.Get() >= int(GlimmerDebugView::TechniqueFirst);
}

} // namespace Hyperion
