/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Framework/CVarManager.hpp>

namespace Hyperion {

extern CVar<int> g_cvGlimmerProbesNearFieldCascades;

extern CVar<bool> g_cvGlimmerProbesEnabled;
extern CVar<int> g_cvGlimmerProbesTracer;       // 1 = triangle BVH, 0 = bricks
extern CVar<int> g_cvGlimmerProbesRaysPerFrame; // probes traced a frame = this / rays per probe
extern CVar<int> g_cvGlimmerProbesPoolBlocks;   // blocks of 64 probes the pool holds at most (up to GlimmerProbePoolBlocks)
extern CVar<bool> g_cvGlimmerProbesLogStats;    // logs how many probes there are (per level, by state, and on screen) once a second
extern CVar<bool> g_cvGlimmerProbesSleep;
extern CVar<float> g_cvGlimmerProbesEmitterRadius; // small emitters are enlarged by this much to ensure they contribute their fair share.

enum class GlimmerProbeDebugMode : int
{
    None = 0,

    Status,
    Irradiance,
    IrradianceTowardViewer,

    Max
};

extern CVar<int> g_cvGlimmerProbesDebug;
extern CVar<int> g_cvGlimmerProbesDebugLevel;      // which level's probes to draw, -1 for all
extern CVar<float> g_cvGlimmerProbesDebugRadius;   // only probes this close to the viewer
extern CVar<float> g_cvGlimmerProbesDebugExposure; // scales irradiance before it's tonemapped for display


} // namespace Hyperion
