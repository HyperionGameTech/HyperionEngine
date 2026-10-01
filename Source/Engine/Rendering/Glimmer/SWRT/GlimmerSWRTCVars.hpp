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

// the probe blocks; off leaves the near field to the SH voxels
extern CVar<bool> g_cvGlimmerSWRTProbesEnabled;
extern CVar<int> g_cvGlimmerSWRTProbesRaysPerFrame;            // probes traced a frame = this / rays per probe
extern CVar<int> g_cvGlimmerSWRTProbesPoolBlocks;              // blocks of 64 probes the pool holds at most (up to GlimmerProbePoolBlocks)
extern CVar<float> g_cvGlimmerSWRTProbesBlockMargin;           // how far (in probe spacings) around a block solids make it wanted
extern CVar<float> g_cvGlimmerSWRTProbesMinHeightAboveGround;  // solids lower than this on the ground (roads, rocks) want no probes
extern CVar<int> g_cvGlimmerSWRTProbesBlockReleaseFrames;      // frames an unwanted block keeps its probes
extern CVar<float> g_cvGlimmerSWRTProbesTau;                   // seconds for a probe's history to fade while its light holds
extern CVar<float> g_cvGlimmerSWRTProbesTauChanging;           // ...and while its light changes
extern CVar<float> g_cvGlimmerSWRTProbesRelocateMargin;        // how far past a back face a probe inside a solid moves (m)
extern CVar<int> g_cvGlimmerSWRTProbesReclassifyInterval;      // updates between checks on probes stuck inside solids

// Must match the modes GlimmerSystem draws for Rendering.Glimmer.SWRT.DebugProbes
enum class GlimmerSWRTDebugProbes : int
{
    None = 0,

    // green where every ray saw the front of something, through yellow as more of them hit back faces; cyan where it was moved off
    // its grid point, magenta where it's stuck inside a solid, red where it's under the ground, grey before its first update
    Status,

    // the probe's irradiance averaged over all directions
    Irradiance,

    // the probe's irradiance for a surface facing the viewer, so walking around a probe shows which way its light comes from
    IrradianceTowardViewer,

    Max
};

// Draws the SWRT probes with the DebugDrawer, read back from the GPU a few frames late: 0 = off, else GlimmerSWRTDebugProbes
extern CVar<int> g_cvGlimmerSWRTDebugProbes;
extern CVar<int> g_cvGlimmerSWRTDebugProbesLevel;      // which level's probes to draw, -1 for all
extern CVar<float> g_cvGlimmerSWRTDebugProbesRadius;   // only probes this close to the viewer
extern CVar<float> g_cvGlimmerSWRTDebugProbesExposure; // scales irradiance before it's tonemapped for display


} // namespace Hyperion
