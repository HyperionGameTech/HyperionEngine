/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/GlimmerCVars.hpp>

namespace Hyperion {

CVar<bool> g_cvGlimmerEnabled("Rendering.Glimmer.Enabled", true);

CVar<float> g_cvGlimmerSpansRadius("Rendering.Glimmer.Spans.Radius", 384.0f);
CVar<float> g_cvGlimmerFoliageExtinction("Rendering.Glimmer.Foliage.Extinction", 0.3f);

CVar<float> g_cvGlimmerIntensity("Rendering.Glimmer.Intensity", 1.0f);
CVar<int> g_cvGlimmerDebugView("Rendering.Glimmer.DebugView", 0);
CVar<bool> g_cvGlimmerHalfRes("Rendering.Glimmer.HalfRes", true);
CVar<int> g_cvGlimmerDebugSH("Rendering.Glimmer.DebugSH", 0);
CVar<bool> g_cvGlimmerSHNearestHitLight("Rendering.Glimmer.SH.NearestHitLight", true);
CVar<int> g_cvGlimmerSHRefreshVoxelsPerFrame("Rendering.Glimmer.SH.RefreshVoxelsPerFrame", 0); // Not needed
CVar<int> g_cvGlimmerSHPasses("Rendering.Glimmer.SH.Passes", 10);
CVar<float> g_cvGlimmerSHHysteresis("Rendering.Glimmer.SH.Hysteresis", 0.7f);
CVar<float> g_cvGlimmerSHLightEaseSeconds("Rendering.Glimmer.SH.LightEaseSeconds", 0.5f);
CVar<float> g_cvGlimmerVisibility("Rendering.Glimmer.Visibility", 1.0f);
CVar<bool> g_cvGlimmerLookAhead("Rendering.Glimmer.LookAhead", true);
CVar<float> g_cvGlimmerStartupBurstSeconds("Rendering.Glimmer.StartupBurstSeconds", 8.0f);
CVar<int> g_cvGlimmerBuildThreads("Rendering.Glimmer.BuildThreads", 0);
CVar<int> g_cvGlimmerBVHNodePoolMB("Rendering.Glimmer.BVH.NodePoolMB", 96);
CVar<float> g_cvGlimmerBVHRadius("Rendering.Glimmer.BVH.Radius", 128.0f);
CVar<float> g_cvGlimmerBVHCellSize("Rendering.Glimmer.BVH.CellSize", 64.0f);

} // namespace Hyperion
