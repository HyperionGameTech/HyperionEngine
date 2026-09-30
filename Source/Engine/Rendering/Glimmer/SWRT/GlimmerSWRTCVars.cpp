/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/SWRT/GlimmerSWRTCVars.hpp>

namespace Hyperion {

CVar<int> g_cvGlimmerSWRTNearFieldCascades("Rendering.Glimmer.SWRT.NearField.Cascades", 2);
CVar<float> g_cvGlimmerSWRTNearFieldRadius("Rendering.Glimmer.SWRT.NearField.Radius", 96.0f);

CVar<float> g_cvGlimmerSWRTSpansRadius("Rendering.Glimmer.SWRT.Spans.Radius", 384.0f);

CVar<float> g_cvGlimmerSWRTFoliageExtinction("Rendering.Glimmer.SWRT.FoliageExtinction", 1.0f);
CVar<float> g_cvGlimmerSWRTFoliageClumping("Rendering.Glimmer.SWRT.FoliageClumping", 0.7f);

CVar<int> g_cvGlimmerSWRTDebugProbes("Rendering.Glimmer.SWRT.DebugProbes", 0);
CVar<int> g_cvGlimmerSWRTDebugProbesCascade("Rendering.Glimmer.SWRT.DebugProbes.Cascade", 0);
CVar<float> g_cvGlimmerSWRTDebugProbesRadius("Rendering.Glimmer.SWRT.DebugProbes.Radius", 32.0f);
CVar<float> g_cvGlimmerSWRTDebugProbesExposure("Rendering.Glimmer.SWRT.DebugProbes.Exposure", 1.0f);


} // namespace Hyperion
