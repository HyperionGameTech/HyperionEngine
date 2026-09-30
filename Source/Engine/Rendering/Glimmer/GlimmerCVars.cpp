/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/GlimmerCVars.hpp>
#include <Rendering/Glimmer/GlimmerTechnique.hpp>

namespace Hyperion {

CVar<bool> g_cvGlimmerEnabled("Rendering.Glimmer.Enabled", false);
CVar<int> g_cvGlimmerTechnique("Rendering.Glimmer.Technique", int(GlimmerTechniqueType::SH));
CVar<float> g_cvGlimmerIntensity("Rendering.Glimmer.Intensity", 1.0f);
CVar<int> g_cvGlimmerDebugView("Rendering.Glimmer.DebugView", 0);

CVar<float> g_cvGlimmerGroundAlbedo("Rendering.Glimmer.Ground.Albedo", 0.25f);

bool IsGlimmerSceneRequired()
{
    return g_cvGlimmerEnabled.Get() || g_cvGlimmerDebugView.Get() >= int(GlimmerDebugView::TechniqueFirst);
}

} // namespace Hyperion
