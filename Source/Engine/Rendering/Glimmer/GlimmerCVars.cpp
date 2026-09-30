/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/GlimmerCVars.hpp>

namespace Hyperion {

CVar<bool> g_cvGlimmerEnabled("Rendering.Glimmer.Enabled", false);
CVar<float> g_cvGlimmerIntensity("Rendering.Glimmer.Intensity", 1.0f);
CVar<int> g_cvGlimmerDebugView("Rendering.Glimmer.DebugView", 0);

CVar<float> g_cvGlimmerGroundAlbedo("Rendering.Glimmer.Ground.Albedo", 0.25f);

CVar<int> g_cvGlimmerNearFieldCascades("Rendering.Glimmer.NearField.Cascades", 2);
CVar<float> g_cvGlimmerNearFieldRadius("Rendering.Glimmer.NearField.Radius", 96.0f);

CVar<float> g_cvGlimmerSpansRadius("Rendering.Glimmer.Spans.Radius", 384.0f);

CVar<float> g_cvGlimmerFoliageExtinction("Rendering.Glimmer.FoliageExtinction", 1.0f);
CVar<float> g_cvGlimmerFoliageClumping("Rendering.Glimmer.FoliageClumping", 0.7f);

bool IsGlimmerSceneRequired()
{
    const int debugView = g_cvGlimmerDebugView.Get();

    return g_cvGlimmerEnabled.Get() || (debugView > int(GlimmerDebugView::None) && debugView < int(GlimmerDebugView::Irradiance));
}

} // namespace Hyperion
