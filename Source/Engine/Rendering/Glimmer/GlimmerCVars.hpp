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

enum class GlimmerHeightfieldDebugView : int
{
    None = 0,
    Spans,
    GroundAlbedo,
    Occupancy,

    Max
};

extern CVar<bool> g_cvGlimmerEnabled;

// how far around the viewer static solids and foliage are splatted into the heightfield
extern CVar<float> g_cvGlimmerSpansRadius;

extern CVar<float> g_cvGlimmerFoliageExtinction;

extern CVar<float> g_cvGlimmerIntensity;
extern CVar<int> g_cvGlimmerDebugView;

extern CVar<bool> g_cvGlimmerHalfRes; // irradiance evaluated for one pixel of each 2x2 quad, then upsampled

// 0 = off
// 1 = irradiance
// 2 = sky visibility
// 3 = sky visibility of the single voxel (no interpolation)
extern CVar<int> g_cvGlimmerDebugSH;
extern CVar<bool> g_cvGlimmerSHNearestHitLight;              // SH rays light their hits from the nearest voxel (cheap) rather than the filtered volume
extern CVar<int> g_cvGlimmerSHRefreshVoxelsPerFrame;         // SH voxels traced again a frame while nothing changes, in slices of half a cascade column
extern CVar<int> g_cvGlimmerSHPasses;                        // traces a voxel gets once something changes it, the first from the budget and the rest from what it leaves
extern CVar<float> g_cvGlimmerSHHysteresis;                  // how much of a voxel's history (or for one without, the coarser cascades') each trace keeps
extern CVar<float> g_cvGlimmerSHLightEaseSeconds;            // time constant the sun and sky the SH and surfels are composed with ease toward a change with

extern CVar<float> g_cvGlimmerVisibility;
extern CVar<bool> g_cvGlimmerLookAhead;
extern CVar<float> g_cvGlimmerStartupBurstSeconds;
extern CVar<int> g_cvGlimmerBuildThreads;
extern CVar<float> g_cvGlimmerBVHRadius;                     // how far from the viewer's cell instances go into the TLAS
extern CVar<float> g_cvGlimmerBVHCellSize;                   // the TLAS and its LODs follow the viewer a cell (xz) at a time
extern CVar<int> g_cvGlimmerBVHNodePoolMB;                   // BLAS nodes for tracing the triangle cache; 0 builds no BVH
extern CVar<int> g_cvGlimmerTracer;                          // what near-field rays trace: 1 = triangle BVH, 0 = bricks
extern CVar<float> g_cvGlimmerEmitterRadius;                 // small emitters are enlarged by this much to ensure they contribute their fair share

// 0 = off
// 1 = the reference path tracer's irradiance
// 2 = Glimmer's, averaged over the same frames
// 3 = Glimmer's error against the reference (red brighter, blue darker)
// 4 = the reference on the left, Glimmer on the right
// 5 = the frame as it is, lit by the reference's irradiance in place of Glimmer's
extern CVar<int> g_cvGlimmerReference;
extern CVar<int> g_cvGlimmerReferencePaths;                  // paths each pixel adds a frame
extern CVar<float> g_cvGlimmerReferenceOffset;                // metres out along the normal both the reference and the SH alone (Surfels.Debug 3) are measured at, as for an object near the surface
extern CVar<int> g_cvGlimmerReferenceStride;                 // pixels apart the reference traces along each axis; the stats count only those
extern CVar<int> g_cvGlimmerReferenceBounces;
extern CVar<float> g_cvGlimmerReferenceExposure;
extern CVar<float> g_cvGlimmerReferenceErrorScale;           // relative error the error view saturates at
extern CVar<bool> g_cvGlimmerReferenceFurnace;               // white sky and surfaces, no sun or emitters: the reference should come out 1 wherever nothing absorbs
extern CVar<bool> g_cvGlimmerReferenceShaded;                // the views show diffuse shading (the sun behind a traced shadow ray, plus the irradiance) rather than the irradiance alone
extern CVar<bool> g_cvGlimmerReferenceSkySH;                 // paths that escape take the sky SH Glimmer is lit by rather than the sky probe's cubemap

} // namespace Hyperion
