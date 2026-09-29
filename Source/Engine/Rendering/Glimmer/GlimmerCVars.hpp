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

    Max
};

extern CVar<bool> g_cvGlimmerEnabled;
extern CVar<float> g_cvGlimmerIntensity;
extern CVar<bool> g_cvGlimmerFreeze;
extern CVar<int> g_cvGlimmerDebugVis;

extern CVar<int> g_cvGlimmerGroundSamplesPerFrame;
extern CVar<float> g_cvGlimmerGroundAlbedo;

extern CVar<int> g_cvGlimmerProbesRays;
extern CVar<float> g_cvGlimmerProbesHysteresis;
extern CVar<float> g_cvGlimmerProbesEscapeClamp;
extern CVar<float> g_cvGlimmerProbesMaxDistance;

extern CVar<int> g_cvGlimmerNearFieldCascades;

// how far around the viewer static solids and foliage are splatted into the heightfield
extern CVar<float> g_cvGlimmerSpansRadius;
extern CVar<int> g_cvGlimmerSpansMaxInstances;
extern CVar<float> g_cvGlimmerSpansFoliageLodErrorMeters;
extern CVar<float> g_cvGlimmerSpansMinFoliageHeight;
extern CVar<float> g_cvGlimmerFoliageExtinction;
extern CVar<float> g_cvGlimmerNearFieldReachSpacings;

// Horizontal half extent of the region traced exactly by SWRT, centred near the viewer
extern CVar<float> g_cvGlimmerNearFieldRadius;

extern CVar<int> g_cvGlimmerSWRTPoolMB;
extern CVar<float> g_cvGlimmerSWRTMaxLodErrorMeters;
extern CVar<float> g_cvGlimmerSWRTMaskCellSize;
extern CVar<int> g_cvGlimmerSWRTRebuildDebounceMs;
extern CVar<int> g_cvGlimmerSWRTMaxInstances;
extern CVar<int> g_cvGlimmerSWRTMaxBLASBuildsInFlight;
extern CVar<int> g_cvGlimmerSWRTUploadBudgetKB;
extern CVar<int> g_cvGlimmerSWRTDebugView;
extern CVar<bool> g_cvGlimmerSWRTLogStats;

// each new value captures the next SWRT debug frame to GlimmerCaptures/glimmer_swrt_<value>.png next to the executable
extern CVar<int> g_cvGlimmerSWRTCaptureDebugView;

// each new value captures the next final frame of every GBuffer view to GlimmerCaptures/glimmer_frame_<value>.png
extern CVar<int> g_cvGlimmerCaptureFrame;

/*! \brief Whether the Glimmer scene (BLAS/TLAS/footprint mask) needs to be kept up to date. */
bool IsGlimmerSceneRequired();

} // namespace Hyperion
