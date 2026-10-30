/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/RenderTypes.hpp>
#include <Rendering/RawBuffer.hpp>
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
class GlimmerTriangleCache;
class GlimmerSceneInstances;
class GlimmerSHVolume;
class GlimmerRelight;
class GlimmerLocalLights;
class GlimmerBrickWorld;
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
    Vec4f center;       // xyz = the window's center before snapping, in probe spacings
};

struct GlimmerProbeVolumeShaderData
{
    GlimmerProbeLevelShaderData levels[GlimmerProbeLevels];
    Vec4u info;      // x = number of levels, y = rays per probe, z = frame, w = 1 when the volume can be sampled
    Vec4f params;    // x = seconds since the volume started, y = how much visibility counts (Rendering.Glimmer.Visibility), z = escape radiance clamp, w = max ray distance
    Vec4f nearField; // x = levels that trace the bricks, y = their reach in spacings, z = 1 when there are bricks to trace, w = ground albedo
};

struct GlimmerProbeUpdateInputs
{
    Vec3f viewerPosition;
    Vec3f lookAhead;          // GlimmerChannelState::lookAhead
    float budgetScale = 1.0f; // GlimmerChannelState::budgetScale

    const GlimmerSurfaceCache* surfaceCache = nullptr;
    const GlimmerSpanCache* spanCache = nullptr;
    const GlimmerTriangleCache* triangleCache = nullptr;
    const GlimmerSceneInstances* sceneInstances = nullptr;
    const GlimmerSHVolume* shVolume = nullptr;           // the far field, for the bounce at hits past the probes; may be nullptr
    GlimmerRelight* relight = nullptr;                   // lit here, before the trace shades heightfield hits from it; may be nullptr
    const GlimmerBrickWorld* brickWorld = nullptr;       // may be nullptr

    // the solids the probe blocks are placed around
    const GlimmerSHOccupancyShaderData* occupancy = nullptr;
    GpuImageViewRef occupancyImageView;

    EnvProbe* skyProbe = nullptr;

    const CloudPass* cloudPass = nullptr; // its cloud shadows dim the sun at hits; may be nullptr

    GlimmerLightShaderData light;

    const GlimmerLocalLights* localLights = nullptr;

    RWStructuredBuffer* scratchBuffer = nullptr;
};

class GlimmerProbeVolume final
{
public:
    GlimmerProbeVolume();

    GlimmerProbeVolume(const GlimmerProbeVolume& other) = delete;
    GlimmerProbeVolume& operator=(const GlimmerProbeVolume& other) = delete;

    ~GlimmerProbeVolume();

    void Update(Frame* frame, const GlimmerProbeUpdateInputs& inputs);

    HYP_FORCE_INLINE const GlimmerProbeVolumeShaderData& GetShaderData() const
    {
        return m_shaderData;
    }

    HYP_FORCE_INLINE bool IsReady() const
    {
        return m_shaderData.info.w != 0;
    }

    HYP_FORCE_INLINE uint32 GetPoolBlocks() const
    {
        return m_poolBlocks;
    }

    const RWStructuredBuffer& GetBlockTableBuffer() const;
    const RWStructuredBuffer& GetSHBuffer() const;
    const RWStructuredBuffer& GetLightBuffer() const;
    const RWStructuredBuffer& GetStatesBuffer() const;
    const RWStructuredBuffer& GetVisibilityBuffer() const;
    const RWStructuredBuffer& GetSlotsBuffer() const;

private:
    void CreateResources(Frame* frame);
    void ReleaseResources();
    void UpdateWindows(const Vec3f& viewerPosition, const Vec3f& lookAhead);

    // elementSize is the stride the shaders declare for what the placeholder stands in for
    const RWStructuredBuffer& GetPlaceholderBuffer(size_t elementSize) const;
    const RWStructuredBuffer& GetBufferOrPlaceholder(const RWStructuredBuffer& buffer, size_t elementSize) const;

    RWStructuredBuffer m_blockTableBuffer; // a slot (or ~0) per block of each level's window
    RWStructuredBuffer m_cellsBuffer;      // per block table entry: the block it was classified for, and whether it wants a slot
    RWStructuredBuffer m_slotsBuffer;      // per slot: the block's absolute coordinate and level (-1 when free)
    RWStructuredBuffer m_slotTimesBuffer;  // per slot: frames since its block was last wanted, and the frame one of its probes last saw its light change
    RWStructuredBuffer m_statesBuffer;
    RWStructuredBuffer m_shBuffer;         // 3 per probe: the light put together
    RWStructuredBuffer m_lightBuffer;      // 3 per part of the light per probe
    RWStructuredBuffer m_visibilityBuffer; // GlimmerProbeVisibilityTexels per probe
    RWStructuredBuffer m_trendBuffer;
    RWStructuredBuffer m_countersBuffer;   // NumProbeCounters, then the list of probes this frame updates

    mutable RWStructuredBuffer m_placeholderBuffers[2]; // uint and 16 byte elements

    Time m_startTime;
    uint32 m_frameIndex;
    uint32 m_seenInstancesGeneration;
    uint32 m_seenLightsGeneration = 0;
    uint32 m_nearTracer = 0;
    uint32 m_poolBlocks;
    uint32 m_maxProbesPerFrame;

    GlimmerProbeVolumeShaderData m_shaderData;

    GlimmerLightShaderData m_composedLight;
    bool m_hasComposedLight;
};

} // namespace Hyperion
