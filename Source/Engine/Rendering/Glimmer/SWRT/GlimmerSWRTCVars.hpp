/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Framework/CVarManager.hpp>

namespace Hyperion {

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
    Occupancy, // the SH occupancy clipmap, ray marched from the camera: the solids as the SH voxels' rays see them

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
extern CVar<int> g_cvGlimmerSWRTProbesMaxHistory;              // updates a probe's history averages at most this
extern CVar<int> g_cvGlimmerSWRTProbesMinHistory;              // min of the above
extern CVar<float> g_cvGlimmerSWRTProbesRelocateMargin;        // how far past a back face a probe inside a solid moves (m)
extern CVar<int> g_cvGlimmerSWRTProbesInsideRetryInterval;     // updates between checks on probes stuck inside solids
extern CVar<int> g_cvGlimmerSWRTProbesClassifyPeriod;          // frames between looks at the solids around a block

enum class GlimmerSWRTDebugProbes : int
{
    None = 0,

    Status,
    Irradiance,
    IrradianceTowardViewer,

    Max
};

extern CVar<int> g_cvGlimmerSWRTDebugProbes;
extern CVar<int> g_cvGlimmerSWRTDebugProbesLevel;      // which level's probes to draw, -1 for all
extern CVar<float> g_cvGlimmerSWRTDebugProbesRadius;   // only probes this close to the viewer
extern CVar<float> g_cvGlimmerSWRTDebugProbesExposure; // scales irradiance before it's tonemapped for display


} // namespace Hyperion
