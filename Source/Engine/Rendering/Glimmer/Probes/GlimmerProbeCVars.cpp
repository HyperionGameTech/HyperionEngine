/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/Probes/GlimmerProbeCVars.hpp>

namespace Hyperion {

CVar<int> g_cvGlimmerProbesNearFieldCascades("Rendering.Glimmer.Probes.NearField.Cascades", 4);

CVar<bool> g_cvGlimmerProbesEnabled("Rendering.Glimmer.Probes.Enabled", true);
CVar<int> g_cvGlimmerProbesTracer("Rendering.Glimmer.Probes.Tracer", 1);
CVar<int> g_cvGlimmerProbesRaysPerFrame("Rendering.Glimmer.Probes.RaysPerFrame", 65536);
CVar<int> g_cvGlimmerProbesPoolBlocks("Rendering.Glimmer.Probes.PoolBlocks", 1024);
CVar<bool> g_cvGlimmerProbesLogStats("Rendering.Glimmer.Probes.LogStats", false);
CVar<bool> g_cvGlimmerProbesSleep("Rendering.Glimmer.Probes.Sleep", true);
CVar<float> g_cvGlimmerProbesEmitterRadius("Rendering.Glimmer.Probes.EmitterRadius", 0.5f);

CVar<int> g_cvGlimmerProbesDebug("Rendering.Glimmer.Probes.Debug", 0);
CVar<int> g_cvGlimmerProbesDebugLevel("Rendering.Glimmer.Probes.Debug.Level", -1);
CVar<float> g_cvGlimmerProbesDebugRadius("Rendering.Glimmer.Probes.Debug.Radius", 32.0f);
CVar<float> g_cvGlimmerProbesDebugExposure("Rendering.Glimmer.Probes.Debug.Exposure", 1.0f);


} // namespace Hyperion
