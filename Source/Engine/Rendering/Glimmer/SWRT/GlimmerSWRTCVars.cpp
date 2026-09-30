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

} // namespace Hyperion
