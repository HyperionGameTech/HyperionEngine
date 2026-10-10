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

    Irradiance,
    Coverage,
    TechniqueFirst
};

extern CVar<bool> g_cvGlimmerEnabled;

extern CVar<float> g_cvGlimmerIntensity;
extern CVar<int> g_cvGlimmerDebugView;

extern CVar<bool> g_cvGlimmerHalfRes;            // irradiance evaluated for one pixel of each 2x2 quad, then upsampled
extern CVar<bool> g_cvGlimmerSpecularOcclusion;  // sky reflections take Glimmer's sky visibility along the reflection

// 0 = off
// 1 = irradiance
// 2 = sky visibility
// 3 = sky visibility of the single voxel (no interpolation)
extern CVar<int> g_cvGlimmerDebugSH;

// how much the probes' and SH voxels' depth moments cut off light from behind walls: 1 = fully, 0 = not at all (for comparing)
extern CVar<float> g_cvGlimmerVisibility;

extern CVar<float> g_cvGlimmerGroundAlbedo; // where the terrain's own albedo isn't known

extern CVar<bool> g_cvGlimmerRunOnChange;
extern CVar<bool> g_cvGlimmerSkipLodOnlyChanges;   // an instance that only changed LOD does not retrace the SH voxels or wake the probes around it
extern CVar<float> g_cvGlimmerSunAngleThreshold;   // degrees the sun turns before the lighting counts as changed
extern CVar<int> g_cvGlimmerSHRefreshVoxels;       // per frame while the lighting holds
extern CVar<int> g_cvGlimmerSHRefreshVoxelsBurst;  // per frame for one pass over the volume after it changes
extern CVar<int> g_cvGlimmerRelightTexels;
extern CVar<int> g_cvGlimmerRelightTexelsBurst;

/// @TODO Move to GlimmerHelpers?
bool IsGlimmerSceneRequired();

} // namespace Hyperion
