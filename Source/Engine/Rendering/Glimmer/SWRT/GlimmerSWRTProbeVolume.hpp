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

class CloudPass;

class Texture;
class EnvProbe;
class GlimmerBLASCache;
class GlimmerTLAS;
class GlimmerFootprintMask;
class GlimmerSHVolume;
class GlimmerRelight;
struct GlimmerSHOccupancyShaderData;

static constexpr uint32 GlimmerProbeLevels = 4;
static constexpr uint32 GlimmerProbeBlock = 4;
static constexpr uint32 GlimmerProbesPerBlock = GlimmerProbeBlock * GlimmerProbeBlock * GlimmerProbeBlock;
static constexpr uint32 GlimmerProbeWindow = 16;
static constexpr uint32 GlimmerProbeWindowBlocks = GlimmerProbeWindow * GlimmerProbeWindow * GlimmerProbeWindow;
static constexpr uint32 GlimmerProbePoolBlocks = 1024;
static constexpr uint32 GlimmerProbePoolProbes = GlimmerProbePoolBlocks * GlimmerProbesPerBlock;

static constexpr uint32 GlimmerProbeRays = 32;

static constexpr uint32 GlimmerProbeVisibilityTexels = 64;
static constexpr uint32 GlimmerMaxProbesPerFrame = 4096;

enum GlimmerProbeState : uint32
{
    GPS_FREE = 0,
    GPS_ACTIVE = 1,
    GPS_BURIED = 2,
    GPS_INSIDE = 3,
    GPS_IDLE = 4
};

struct GlimmerProbeLevelShaderData
{
    Vec4i windowOrigin; // xyz = absolute block of the window's first block, w = 1 once it has been allocated
    Vec4f params;       // x = probe spacing, y = 1 / spacing
};

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
    GlimmerRelight* relight = nullptr;                   // lit here, before the trace shades heightfield hits from it; may be nullptr

    // the solids the probe blocks are placed around
    const GlimmerSHOccupancyShaderData* occupancy = nullptr;
    GpuImageViewRef occupancyImageView;

    EnvProbe* skyProbe = nullptr;

    const CloudPass* cloudPass = nullptr; // its cloud shadows dim the sun at hits; may be nullptr

    EnumFlags<GlimmerLightingChangeFlags> lightingChanges = GlimmerLightingChangeFlags::None;
};

class GlimmerSWRTProbeVolume final
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
    GpuBufferRef m_blockWakeBuffer;  // per slot: the frame one of its probes last saw its light change
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
    uint32 m_seenTLASGeneration;

    EnumFlags<GlimmerLightingChangeFlags> m_pendingLightingChanges;

    GlimmerProbeVolumeShaderData m_shaderData;
};

} // namespace Hyperion
