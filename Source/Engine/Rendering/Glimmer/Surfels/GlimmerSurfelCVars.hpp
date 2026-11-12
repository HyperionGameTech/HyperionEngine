/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Framework/CVarManager.hpp>

namespace Hyperion {

extern CVar<bool> g_cvGlimmerSurfelsEnabled;
extern CVar<float> g_cvGlimmerSurfelsSpacing;      // average distance between surfels (m)
extern CVar<float> g_cvGlimmerSurfelsRadius;       // surfels are placed this far around the viewer (m)
extern CVar<int> g_cvGlimmerSurfelsRaysPerFrame;   // surfels traced a frame = this / GlimmerSurfelRays
extern CVar<int> g_cvGlimmerSurfelsHistory;        // most updates a surfel's light averages; they stop being traced once each has had this many
extern CVar<bool> g_cvGlimmerSurfelsTracedSun;     // the sun bounce is traced with shadow rays at hits, and traced again as the sun turns, rather than taken from the SH sun
extern CVar<float> g_cvGlimmerSurfelsNearReach;    // how far surfel rays trace the near field (m), at most to where they leave the BVH
extern CVar<bool> g_cvGlimmerSurfelsLogStats;
extern CVar<float> g_cvGlimmerSurfelsFoliageVoxelSize;  // foliage is lit from voxels this size (m); 0 leaves it to the SH
extern CVar<float> g_cvGlimmerSurfelsFoliageMinDensity; // leaf area (m^2) per m^3 a foliage voxel needs to be kept

enum class GlimmerSurfelDebugMode : int
{
    None = 0,

    Irradiance, // the surfels' alone, magenta where they don't cover
    Foliage,    // the foliage voxels' alone
    SHOnly,     // the SH grid's alone, as every pixel would get without surfels or foliage voxels

    Max
};

extern CVar<int> g_cvGlimmerSurfelsDebug;

} // namespace Hyperion
