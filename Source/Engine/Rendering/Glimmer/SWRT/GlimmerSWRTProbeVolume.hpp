/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/RenderTypes.hpp>
#include <Rendering/Glimmer/GlimmerSurfaceCache.hpp>
#include <Rendering/Glimmer/GlimmerSpanCache.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Utilities/Time.hpp>

#include <Core/Math/Vector2.hpp>
#include <Core/Math/Vector3.hpp>
#include <Core/Math/Vector4.hpp>

namespace Hyperion {

class Texture;
class EnvProbe;
class GlimmerBLASCache;
class GlimmerTLAS;
class GlimmerFootprintMask;
class GlimmerSHVolume;
struct GlimmerSHOccupancyShaderData;

// Must match the defines in Shaders/Glimmer/SWRT/GlimmerProbeTypes.hlsli: 2, 4, 8 and 16 m probes, in blocks of 4x4x4, each level
// keeping a window of 16x16x16 blocks around the viewer (+-64 m up to +-512 m, as far as the SH occupancy reaches at each spacing).
// How far probes actually go is up to the pool (Rendering.Glimmer.SWRT.Probes.PoolBlocks), which goes to the nearest blocks in blocks
// of their own level; the SH voxels cover open ground and what's past them
static constexpr uint32 GlimmerProbeLevels = 4;
static constexpr uint32 GlimmerProbeBlock = 4;
static constexpr uint32 GlimmerProbesPerBlock = GlimmerProbeBlock * GlimmerProbeBlock * GlimmerProbeBlock;
static constexpr uint32 GlimmerProbeWindow = 16;
static constexpr uint32 GlimmerProbeWindowBlocks = GlimmerProbeWindow * GlimmerProbeWindow * GlimmerProbeWindow;
// what the buffers are sized for (about 21 KB a block); the pool size CVar is clamped to it
static constexpr uint32 GlimmerProbePoolBlocks = 1024;
static constexpr uint32 GlimmerProbePoolProbes = GlimmerProbePoolBlocks * GlimmerProbesPerBlock;

static constexpr uint32 GlimmerProbeRays = 32;

// Must match GLIMMER_PROBE_VISIBILITY_TEXELS in Shaders/Glimmer/SWRT/GlimmerProbeTypes.hlsli: an 8x8 octahedral map of depth moments per probe,
// each texel two halves in a uint32
static constexpr uint32 GlimmerProbeVisibilityTexels = 64;

// what the per frame buffers are sized for; Rendering.Glimmer.SWRT.Probes.RaysPerFrame is clamped to it
static constexpr uint32 GlimmerMaxProbesPerFrame = 4096;

// Must match GLIMMER_PROBE_STATE_* in Shaders/Glimmer/SWRT/GlimmerProbeTypes.hlsli
enum GlimmerProbeState : uint32
{
    GPS_FREE = 0,
    GPS_ACTIVE = 1,
    GPS_BURIED = 2,
    GPS_INSIDE = 3,
    GPS_IDLE = 4
};

// Must match GlimmerProbeLevel in Shaders/Glimmer/SWRT/GlimmerProbeTypes.hlsli
struct GlimmerProbeLevelShaderData
{
    Vec4i windowOrigin; // xyz = absolute block of the window's first block, w = 1 once it has been allocated
    Vec4f params;       // x = probe spacing, y = 1 / spacing
};

// Must match GlimmerProbeVolume in Shaders/Glimmer/SWRT/GlimmerProbeTypes.hlsli
struct GlimmerProbeVolumeShaderData
{
    GlimmerProbeLevelShaderData levels[GlimmerProbeLevels];
    Vec4u info;      // x = number of levels, y = rays per probe, z = frame, w = 1 when the volume can be sampled
    Vec4f params;    // x = seconds since the volume started, y = how much visibility counts (Rendering.Glimmer.Visibility), z = escape radiance clamp, w = max ray distance
    Vec4f nearField; // x = levels traced with SWRT, y = SWRT reach in spacings, z = SWRT instances, w = ground albedo
};

struct GlimmerSWRTProbeUpdateInputs
{
    Vec3f viewerPosition;
    const GlimmerSurfaceCache* surfaceCache = nullptr;
    const GlimmerSpanCache* spanCache = nullptr;
    const GlimmerBLASCache* blasCache = nullptr;
    const GlimmerTLAS* tlas = nullptr;
    const GlimmerFootprintMask* footprintMask = nullptr; // of the tlas; tells the trace where SWRT has anything to hit
    const GlimmerSHVolume* shVolume = nullptr;           // the far field, for the bounce at hits past the probes; may be nullptr

    // the solids the probe blocks are placed around
    const GlimmerSHOccupancyShaderData* occupancy = nullptr;
    GpuImageViewRef occupancyImageView;

    EnvProbe* skyProbe = nullptr;
};

/*! \brief Glimmer's near field: sparse blocks of probes around the static solids around the viewer, on a world aligned grid per level.
 *  Each frame a pass finds the blocks where the SH occupancy has solids standing off the ground that no finer level covers, and hands
 *  them slots of the pool nearest first (in blocks of their own level), taking slots back from blocks well past where the pool runs out.
 *  Then the active probes due for an update (within a per frame ray budget) are traced against the SWRT scene and the heightfield,
 *  and blended into L1 irradiance and depth moments. Probes under the ground or in open air are skipped, and probes inside solids are
 *  moved out. Where there are no blocks lighting falls through to the SH voxels. Render thread only. */
class GlimmerSWRTProbeVolume
{
public:
    GlimmerSWRTProbeVolume();
    GlimmerSWRTProbeVolume(const GlimmerSWRTProbeVolume& other) = delete;
    GlimmerSWRTProbeVolume& operator=(const GlimmerSWRTProbeVolume& other) = delete;
    ~GlimmerSWRTProbeVolume();

    void Update(Frame* frame, const GlimmerSWRTProbeUpdateInputs& inputs);

    HYP_FORCE_INLINE const GlimmerProbeVolumeShaderData& GetShaderData() const
    {
        return m_shaderData;
    }

    HYP_FORCE_INLINE bool IsReady() const
    {
        return m_shaderData.info.w != 0;
    }

    // placeholders until the volume has been updated once; all in the shader resource state after Update()
    const GpuBufferRef& GetBlockTableBuffer() const;
    const GpuBufferRef& GetSHBuffer() const;
    const GpuBufferRef& GetStatesBuffer() const;
    const GpuBufferRef& GetVisibilityBuffer() const;
    const GpuBufferRef& GetSlotsBuffer() const;

private:
    void CreateResources(Frame* frame);
    void UpdateWindows(const Vec3f& viewerPosition);

    const GpuBufferRef& GetBufferOrPlaceholder(const GpuBufferRef& buffer) const;

    GpuBufferRef m_blockTableBuffer; // a slot (or ~0) per block of each level's window
    GpuBufferRef m_cellsBuffer;      // per block table entry: the block it was classified for, and whether it wants a slot
    GpuBufferRef m_slotsBuffer;      // per slot: the block's absolute coordinate and level (-1 when free)
    GpuBufferRef m_slotAgesBuffer;   // per slot: frames since its block was last wanted
    GpuBufferRef m_statesBuffer;
    GpuBufferRef m_shBuffer;         // 3 per probe
    GpuBufferRef m_visibilityBuffer; // GlimmerProbeVisibilityTexels per probe
    GpuBufferRef m_trendBuffer;
    GpuBufferRef m_countersBuffer;
    GpuBufferRef m_updateListBuffer;

    GpuBufferRef m_raysBuffer;
    GpuBufferRef m_rayHitsBuffer; // between the trace and shade passes

    GpuBufferRef m_placeholderBuffer;

    Time m_startTime;
    uint32 m_frameIndex;

    GlimmerProbeVolumeShaderData m_shaderData;
};

} // namespace Hyperion
