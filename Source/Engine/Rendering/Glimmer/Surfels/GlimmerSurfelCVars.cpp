/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/Surfels/GlimmerSurfelCVars.hpp>

namespace Hyperion {

CVar<bool> g_cvGlimmerSurfelsEnabled("Rendering.Glimmer.Surfels.Enabled", true);
CVar<float> g_cvGlimmerSurfelsSpacing("Rendering.Glimmer.Surfels.Spacing", 0.25f);
CVar<float> g_cvGlimmerSurfelsRadius("Rendering.Glimmer.Surfels.Radius", 64.0f);
CVar<int> g_cvGlimmerSurfelsRaysPerFrame("Rendering.Glimmer.Surfels.RaysPerFrame", 65536);
CVar<int> g_cvGlimmerSurfelsHistory("Rendering.Glimmer.Surfels.History", 64);
CVar<bool> g_cvGlimmerSurfelsTracedSun("Rendering.Glimmer.Surfels.TracedSun", true);
CVar<float> g_cvGlimmerSurfelsNearReach("Rendering.Glimmer.Surfels.NearReach", 128.0f);
CVar<bool> g_cvGlimmerSurfelsLogStats("Rendering.Glimmer.Surfels.LogStats", false);
CVar<float> g_cvGlimmerSurfelsFoliageVoxelSize("Rendering.Glimmer.Surfels.FoliageVoxelSize", 1.0f);
CVar<float> g_cvGlimmerSurfelsFoliageMinDensity("Rendering.Glimmer.Surfels.FoliageMinDensity", 0.05f);

CVar<int> g_cvGlimmerSurfelsDebug("Rendering.Glimmer.Surfels.Debug", 0);

} // namespace Hyperion
