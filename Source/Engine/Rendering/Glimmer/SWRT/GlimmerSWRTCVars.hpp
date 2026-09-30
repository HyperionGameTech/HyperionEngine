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

// Must match the modes GlimmerSystem draws for Rendering.Glimmer.SWRT.DebugProbes
enum class GlimmerSWRTDebugProbes : int
{
    None = 0,

    // green where every ray saw the front of something, through yellow to magenta once a quarter of its rays hit back faces
    // (the probe is inside a solid); red where it's under the ground, grey where it hasn't been traced for its column yet
    Status,

    // the probe's irradiance averaged over all directions
    Irradiance,

    // the probe's irradiance for a surface facing the viewer, so walking around a probe shows which way its light comes from
    IrradianceTowardViewer,

    Max
};

// Draws the SWRT probes with the DebugDrawer, read back from the GPU a few frames late: 0 = off, else GlimmerSWRTDebugProbes
extern CVar<int> g_cvGlimmerSWRTDebugProbes;
extern CVar<int> g_cvGlimmerSWRTDebugProbesCascade;    // which cascade's probes to draw, -1 for all
extern CVar<float> g_cvGlimmerSWRTDebugProbesRadius;   // only probes this close to the viewer
extern CVar<float> g_cvGlimmerSWRTDebugProbesExposure; // scales irradiance before it's tonemapped for display


} // namespace Hyperion
