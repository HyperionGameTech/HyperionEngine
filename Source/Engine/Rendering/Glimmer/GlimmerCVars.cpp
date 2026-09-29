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
CVar<bool> g_cvGlimmerFreeze("Rendering.Glimmer.Freeze", false);
CVar<int> g_cvGlimmerDebugVis("Rendering.Glimmer.DebugVis", 0);

CVar<int> g_cvGlimmerGroundSamplesPerFrame("Rendering.Glimmer.Ground.SamplesPerFrame", 65536);
CVar<float> g_cvGlimmerGroundAlbedo("Rendering.Glimmer.Ground.Albedo", 0.25f);

CVar<int> g_cvGlimmerProbesRays("Rendering.Glimmer.Probes.Rays", 32);
CVar<float> g_cvGlimmerProbesHysteresis("Rendering.Glimmer.Probes.Hysteresis", 0.95f);
CVar<float> g_cvGlimmerProbesEscapeClamp("Rendering.Glimmer.Probes.EscapeClamp", 64.0f);
CVar<float> g_cvGlimmerProbesMaxDistance("Rendering.Glimmer.Probes.MaxDistance", 2000.0f);

CVar<int> g_cvGlimmerNearFieldCascades("Rendering.Glimmer.NearField.Cascades", 2);

CVar<float> g_cvGlimmerSpansRadius("Rendering.Glimmer.Spans.Radius", 384.0f);
CVar<int> g_cvGlimmerSpansMaxInstances("Rendering.Glimmer.Spans.MaxInstances", 262144);
CVar<float> g_cvGlimmerSpansFoliageLodErrorMeters("Rendering.Glimmer.Spans.FoliageLodErrorMeters", 1.0f);
CVar<float> g_cvGlimmerSpansMinFoliageHeight("Rendering.Glimmer.Spans.MinFoliageHeight", 1.5f);
CVar<float> g_cvGlimmerFoliageExtinction("Rendering.Glimmer.FoliageExtinction", 3.0f);
CVar<float> g_cvGlimmerNearFieldReachSpacings("Rendering.Glimmer.NearField.ReachSpacings", 8.0f);

CVar<float> g_cvGlimmerNearFieldRadius("Rendering.Glimmer.NearField.Radius", 96.0f);

CVar<int> g_cvGlimmerSWRTPoolMB("Rendering.Glimmer.SWRT.PoolMB", 64);
CVar<float> g_cvGlimmerSWRTMaxLodErrorMeters("Rendering.Glimmer.SWRT.MaxLodErrorMeters", 0.25f);
CVar<float> g_cvGlimmerSWRTMaskCellSize("Rendering.Glimmer.SWRT.MaskCellSize", 1.0f);
CVar<int> g_cvGlimmerSWRTRebuildDebounceMs("Rendering.Glimmer.SWRT.RebuildDebounceMs", 250);
CVar<int> g_cvGlimmerSWRTMaxInstances("Rendering.Glimmer.SWRT.MaxInstances", 131072);
CVar<int> g_cvGlimmerSWRTMaxBLASBuildsInFlight("Rendering.Glimmer.SWRT.MaxBLASBuildsInFlight", 4);
CVar<int> g_cvGlimmerSWRTUploadBudgetKB("Rendering.Glimmer.SWRT.UploadBudgetKB", 8192);
CVar<int> g_cvGlimmerSWRTDebugView("Rendering.Glimmer.SWRT.DebugView", 0);
CVar<bool> g_cvGlimmerSWRTLogStats("Rendering.Glimmer.SWRT.LogStats", false);
CVar<int> g_cvGlimmerSWRTCaptureDebugView("Rendering.Glimmer.SWRT.CaptureDebugView", 0);
CVar<int> g_cvGlimmerCaptureFrame("Rendering.Glimmer.CaptureFrame", 0);

bool IsGlimmerSceneRequired()
{
    return g_cvGlimmerEnabled.Get() || g_cvGlimmerSWRTDebugView.Get() != 0;
}

} // namespace Hyperion
