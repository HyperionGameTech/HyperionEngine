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

    // Glimmer's irradiance on its own, from the deferred indirect pass
    Irradiance,

    // values from here on are the active technique's own views, shown in place of the final image
    TechniqueFirst
};

extern CVar<bool> g_cvGlimmerEnabled;
extern CVar<float> g_cvGlimmerIntensity;
extern CVar<int> g_cvGlimmerDebugView;

extern CVar<float> g_cvGlimmerGroundAlbedo; // where the terrain's own albedo isn't known

/*! \brief Whether the Glimmer scene (scene view, surface cache, technique) needs to be kept up to date. */
bool IsGlimmerSceneRequired();

} // namespace Hyperion
