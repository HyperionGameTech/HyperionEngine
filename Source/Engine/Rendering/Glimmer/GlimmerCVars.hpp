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

    // Glimmer's irradiance on its own, drawn by GlimmerIrradiancePass and shown by the deferred indirect pass
    Irradiance,

    // which volume and cascade lighting takes Glimmer's irradiance from, drawn like Irradiance: probes (SWRT, the near field)
    // yellow to red, SH voxels (the far field) cyan to violet, magenta where neither covers
    Coverage,

    // values from here on are the technique's own views, shown in place of the final image
    TechniqueFirst
};

extern CVar<bool> g_cvGlimmerEnabled;

extern CVar<float> g_cvGlimmerIntensity;
extern CVar<int> g_cvGlimmerDebugView;

// the SH voxels on their own: 0 = off, 1 = irradiance, 2 = sky visibility, 3 = sky visibility of the single voxel (no interpolation),
// 4 = what their blockers bounce: red = lit by the sky, green = lit by the sun (as a share of the sun's irradiance / pi)
extern CVar<int> g_cvGlimmerDebugSH;

// how much the probes' and SH voxels' depth moments cut off light from behind walls: 1 = fully, 0 = not at all (for comparing)
extern CVar<float> g_cvGlimmerVisibility;

extern CVar<float> g_cvGlimmerGroundAlbedo; // where the terrain's own albedo isn't known

/*! \brief Whether the Glimmer scene (scene view, surface cache, technique) needs to be kept up to date. */
bool IsGlimmerSceneRequired();

} // namespace Hyperion
