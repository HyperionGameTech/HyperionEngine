/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Framework/CVarManager.hpp>

namespace Hyperion {

enum class GlimmerDebugView : int
{
    None = 0,

    // SWRT views, shown in place of the final image
    Shaded,
    InstanceId,
    TraversalCost,
    DepthCompare,
    FootprintMask,
    Spans,
    GroundAlbedo,

    // Glimmer's irradiance on its own, from the deferred indirect pass
    Irradiance,

    Max
};

extern CVar<bool> g_cvGlimmerEnabled;
extern CVar<float> g_cvGlimmerIntensity;
extern CVar<int> g_cvGlimmerDebugView;

extern CVar<float> g_cvGlimmerGroundAlbedo; // where the terrain's own albedo isn't known

extern CVar<int> g_cvGlimmerNearFieldCascades;

// Horizontal half extent of the region traced exactly by SWRT, centred near the viewer
extern CVar<float> g_cvGlimmerNearFieldRadius;

// how far around the viewer static solids and foliage are splatted into the heightfield
extern CVar<float> g_cvGlimmerSpansRadius;

extern CVar<float> g_cvGlimmerFoliageExtinction;
// leaves bunch up in crowns with gaps between them, so a texel's leaf area blocks less than if it were spread evenly
extern CVar<float> g_cvGlimmerFoliageClumping;

/*! \brief Whether the Glimmer scene (BLAS/TLAS/footprint mask) needs to be kept up to date. */
bool IsGlimmerSceneRequired();

} // namespace Hyperion
