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

// 0 = off
// 1 = irradiance
// 2 = sky visibility
// 3 = sky visibility of the single voxel (no interpolation)
// 4 = what their blockers bounce: red = lit by the sky, green = lit by the sun (as a share of the sun's irradiance / pi)
extern CVar<int> g_cvGlimmerDebugSH;

// how much the probes' and SH voxels' depth moments cut off light from behind walls: 1 = fully, 0 = not at all (for comparing)
extern CVar<float> g_cvGlimmerVisibility;

extern CVar<float> g_cvGlimmerGroundAlbedo; // where the terrain's own albedo isn't known

/// @TODO Move to GlimmerHelpers?
bool IsGlimmerSceneRequired();

} // namespace Hyperion
