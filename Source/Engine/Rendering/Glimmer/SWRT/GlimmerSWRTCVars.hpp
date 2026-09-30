/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Framework/CVarManager.hpp>

namespace Hyperion {

// Rendering.Glimmer.DebugView values from GlimmerDebugView::TechniqueFirst on, in this order
enum class GlimmerSWRTDebugView : int
{
    None = 0,
    Shaded,
    InstanceId,
    TraversalCost,
    DepthCompare,
    FootprintMask,
    Spans,
    GroundAlbedo,

    Max
};

extern CVar<int> g_cvGlimmerSWRTNearFieldCascades;

// Horizontal half extent of the region traced exactly by SWRT, centred near the viewer
extern CVar<float> g_cvGlimmerSWRTNearFieldRadius;

// how far around the viewer static solids and foliage are splatted into the heightfield
extern CVar<float> g_cvGlimmerSWRTSpansRadius;

extern CVar<float> g_cvGlimmerSWRTFoliageExtinction;
// leaves bunch up in crowns with gaps between them, so a texel's leaf area blocks less than if it were spread evenly
extern CVar<float> g_cvGlimmerSWRTFoliageClumping;

} // namespace Hyperion
