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
CVar<int> g_cvGlimmerDebugSH("Rendering.Glimmer.DebugSH", 0);

CVar<float> g_cvGlimmerGroundAlbedo("Rendering.Glimmer.Ground.Albedo", 0.25f);

bool IsGlimmerSceneRequired()
{
    return g_cvGlimmerEnabled.Get() || g_cvGlimmerDebugView.Get() >= int(GlimmerDebugView::TechniqueFirst);
}

} // namespace Hyperion
