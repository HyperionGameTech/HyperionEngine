/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/SWRT/GlimmerSWRTCVars.hpp>

namespace Hyperion {

CVar<int> g_cvGlimmerSWRTNearFieldCascades("Rendering.Glimmer.SWRT.NearField.Cascades", 4);
CVar<float> g_cvGlimmerSWRTNearFieldRadius("Rendering.Glimmer.SWRT.NearField.Radius", 96.0f);

CVar<float> g_cvGlimmerSWRTSpansRadius("Rendering.Glimmer.SWRT.Spans.Radius", 384.0f);

CVar<float> g_cvGlimmerSWRTFoliageExtinction("Rendering.Glimmer.SWRT.FoliageExtinction", 1.0f);
CVar<float> g_cvGlimmerSWRTFoliageClumping("Rendering.Glimmer.SWRT.FoliageClumping", 0.7f);

CVar<bool> g_cvGlimmerSWRTProbesEnabled("Rendering.Glimmer.SWRT.Probes.Enabled", true);
CVar<int> g_cvGlimmerSWRTProbesRaysPerFrame("Rendering.Glimmer.SWRT.Probes.RaysPerFrame", 65536);
CVar<int> g_cvGlimmerSWRTProbesPoolBlocks("Rendering.Glimmer.SWRT.Probes.PoolBlocks", 512);
CVar<float> g_cvGlimmerSWRTProbesBlockMargin("Rendering.Glimmer.SWRT.Probes.BlockMargin", 1.0f);
CVar<float> g_cvGlimmerSWRTProbesMinHeightAboveGround("Rendering.Glimmer.SWRT.Probes.MinHeightAboveGround", 0.5f);
CVar<int> g_cvGlimmerSWRTProbesBlockReleaseFrames("Rendering.Glimmer.SWRT.Probes.BlockReleaseFrames", 60);
CVar<float> g_cvGlimmerSWRTProbesTau("Rendering.Glimmer.SWRT.Probes.Tau", 1.0f);
CVar<float> g_cvGlimmerSWRTProbesTauChanging("Rendering.Glimmer.SWRT.Probes.TauChanging", 0.25f);
CVar<float> g_cvGlimmerSWRTProbesRelocateMargin("Rendering.Glimmer.SWRT.Probes.RelocateMargin", 0.3f);
CVar<int> g_cvGlimmerSWRTProbesReclassifyInterval("Rendering.Glimmer.SWRT.Probes.ReclassifyInterval", 64);

CVar<int> g_cvGlimmerSWRTDebugProbes("Rendering.Glimmer.SWRT.DebugProbes", 0);
CVar<int> g_cvGlimmerSWRTDebugProbesLevel("Rendering.Glimmer.SWRT.DebugProbes.Level", -1);
CVar<float> g_cvGlimmerSWRTDebugProbesRadius("Rendering.Glimmer.SWRT.DebugProbes.Radius", 32.0f);
CVar<float> g_cvGlimmerSWRTDebugProbesExposure("Rendering.Glimmer.SWRT.DebugProbes.Exposure", 1.0f);


} // namespace Hyperion
