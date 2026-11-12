/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/RenderTypes.hpp>
#include <Rendering/Glimmer/GlimmerHelpers.hpp>
#include <Rendering/Glimmer/GlimmerMemory.hpp>

#include <Core/Containers/FixedArray.hpp>
#include <Core/Containers/Array.hpp>

#include <Core/Constants.hpp>

#include <Core/Utilities/Time.hpp>

#include <Core/Math/BoundingBox.hpp>
#include <Core/Math/Vector3.hpp>
#include <Core/Math/Vector4.hpp>

namespace Hyperion {

class EnvProbe;
class CloudPass;
class GlimmerSurfaceCache;
class GlimmerSpanCache;
class GlimmerSceneInstances;
class GlimmerTriangleCache;
class GlimmerLocalLights;
class GlimmerRelight;
class GlimmerSHVolume;
class GlimmerBrickWorld;

// NOTE must match GlimmerSurfelTypes.hlsli
static constexpr uint32 GlimmerSurfelCapacity = 1u << 18;
static constexpr uint32 GlimmerSurfelHashEntries = 1u << 19;
static constexpr uint32 GlimmerSurfelRays = 32;
static constexpr uint32 GlimmerSurfelLightUints = GlimmerLightParts * 2 + (GlimmerLightTerms + 1) / 2;
static constexpr uint32 GlimmerSurfelCounters = 4;
static constexpr uint32 GlimmerSurfelHashUints = GlimmerSurfelCounters + GlimmerSurfelHashEntries * 2;
static constexpr uint32 GlimmerSunKeys = 14; // GLIMMER_SUN_KEYS
static constexpr uint32 GlimmerSurfelDataUints = 3 + GlimmerSurfelLightUints + 3 + GlimmerSunKeys + 1 + GlimmerSunKeys + (GlimmerSunKeys + 4) / 4; // next, composed (2), light, sun bounce (2), sun state, sun keys, sun key state, smoothed sun keys, sun visibility

static constexpr uint32 GlimmerFoliageCapacity = 1u << 16;
static constexpr uint32 GlimmerFoliageHashEntries = 1u << 17;
static constexpr uint32 GlimmerFoliageUints = 103; // GLIMMER_FOLIAGE_UINTS
static constexpr uint32 GlimmerFoliageBufferUints = GlimmerFoliageHashEntries * 4 + GlimmerFoliageCapacity * GlimmerFoliageUints;

static constexpr uint32 GlimmerMaxSurfelsPerFrame = 8192;

struct GlimmerSurfelShaderData
{
    Vec4u info;    // x = surfels and foliage voxels traced this frame at most, y = first of them traced this frame, z = frame, w = 1 when they can be sampled
    Vec4f params;  // x = hash cell size, y = 1 / cell size, z = surfel radius, w = how far rays trace the near field (m)
    Vec4f foliage; // x = foliage voxel size, y = 1 / voxel size, z = the sun epoch the sun last jumped at, w = 1 when the sun bounce is traced
    Vec4f sun;     // xyz = toward the sun the sun bounce is traced for, w = that direction's epoch
    Vec4f sunArc;  // x = where the sun is now along the keys (< 0 off the arc), y = 1 when surfels keep their sun bounce at the keys, z = the arc's epoch
    Vec4f sunKeys[GlimmerSunKeys]; // xyz = toward the sun at each key
};

struct GlimmerSurfelUpdateInputs
{
    Vec3f viewerPosition;
    float budgetScale = 1.0f; // GlimmerChannelState::budgetScale

    const GlimmerSurfaceCache* surfaceCache = nullptr;
    const GlimmerSpanCache* spanCache = nullptr;
    const GlimmerTriangleCache* triangleCache = nullptr;
    const GlimmerSceneInstances* sceneInstances = nullptr;
    const GlimmerSHVolume* shVolume = nullptr;     // the far field, for the bounce at hits no surfel covers; may be nullptr
    GlimmerRelight* relight = nullptr;             // lit here, before the trace shades heightfield hits from it; may be nullptr
    const GlimmerBrickWorld* brickWorld = nullptr; // may be nullptr
    const GlimmerLocalLights* localLights = nullptr;

    EnvProbe* skyProbe = nullptr;
    const CloudPass* cloudPass = nullptr; // its cloud shadows dim the sun bounce as it is composed; may be nullptr

    GlimmerLightShaderData light;

    GpuBuffer* scratchBuffer = nullptr;
};

class GlimmerSurfelCache final
{
public:
    GlimmerSurfelCache();

    GlimmerSurfelCache(const GlimmerSurfelCache& other) = delete;
    GlimmerSurfelCache& operator=(const GlimmerSurfelCache& other) = delete;

    ~GlimmerSurfelCache();

    /// places the surfels when they need to be, then traces and blends the ones due this frame
    void Update(Frame* frame, const GlimmerSurfelUpdateInputs& inputs);

    HYP_FORCE_INLINE bool IsReady() const
    {
        return m_shaderData.info.w != 0;
    }

    HYP_FORCE_INLINE const GlimmerSurfelShaderData& GetShaderData() const
    {
        return m_shaderData;
    }

    HYP_FORCE_INLINE const GpuBufferRef& GetSurfelsBuffer() const
    {
        return m_surfelsBuffer;
    }

    HYP_FORCE_INLINE const GpuBufferRef& GetHashBuffer() const
    {
        return m_hashBuffer;
    }

    HYP_FORCE_INLINE const GpuBufferRef& GetDataBuffer() const
    {
        return m_dataBuffer;
    }

    HYP_FORCE_INLINE const GpuBufferRef& GetFoliageBuffer() const
    {
        return m_foliageBuffer;
    }

private:
    void CreateResources();
    void ReleaseResources();

    bool ShouldPlace(const GlimmerSurfelUpdateInputs& inputs, float spacing, float radius, float foliageVoxelSize);
    void Place(Frame* frame, const GlimmerSurfelUpdateInputs& inputs, float spacing, float radius, float foliageVoxelSize);
    void Transfer(Frame* frame, const GlimmerSurfelShaderData& previousShaderData);

    bool HasChangedAround(const GlimmerSurfelUpdateInputs& inputs);

    void Trace(Frame* frame, const GlimmerSurfelUpdateInputs& inputs, bool hasBVH, bool hasBricks);
    void Blend(Frame* frame, GpuBuffer* raysBuffer, const GlimmerLightShaderData& light, const CloudPass* cloudPass);

    void ReadCounters(Frame* frame);
    void CopyCounters(Frame* frame);

    const GpuBufferRef& GetPlaceholderBuffer();

    GpuBufferRef m_surfelsBuffer; // per surfel: position, then its packed normal
    GpuBufferRef m_hashBuffer;    // the counters, then per entry: a cell's key, then its first surfel
    GpuBufferRef m_dataBuffer;    // GlimmerSurfelDataUints per surfel
    GpuBufferRef m_statesBuffer;  // per surfel: updates, and how much of it can be seen
    GpuBufferRef m_foliageBuffer; // the foliage voxels' hash, then GlimmerFoliageUints per voxel
    GpuBufferRef m_placeholderBuffer;

    // the surfels before the last placement, which the new ones take their light from
    GpuBufferRef m_previousSurfelsBuffer;
    GpuBufferRef m_previousHashBuffer;
    GpuBufferRef m_previousDataBuffer;
    GpuBufferRef m_previousStatesBuffer;

    Array<BoundingBox> m_placeChanges; // where the scene changed since the last placement; surfels there start over
    bool m_isPlaceFromScratch = true;  // nothing the previous surfels hold carries over

    FixedArray<GpuBufferRef, NumFramesInFlight> m_countersReadbackBuffers;
    FixedArray<bool, NumFramesInFlight> m_isCountersReadbackPending {};

    GlimmerSurfelShaderData m_shaderData;

    Vec3f m_placedCenter;
    float m_placedSpacing;
    float m_placedRadius;
    float m_placedFoliageVoxelSize;
    uint32 m_seenInstancesGeneration;
    uint32 m_numPlacements;

    uint32 m_cursor;
    uint32 m_frameIndex;

    uint64 m_numTracedSinceWoken; // they settle once each has been traced as often as the history averages
    uint32 m_wakeEpoch = 0;
    uint32 m_feedbackPassesLeft = 0;
    Vec3f m_sunDirection;         // the sun the sun bounce is traced for
    uint32 m_sunEpoch;            // counts the times it turned, as the shaders know it (16 bits, never 0)
    uint32 m_sunJumpEpoch;        // the epoch the sun last jumped at; sun bounces from before it are dropped
    bool m_isSunTraced;
    Vec4f m_sunArc;               // the arc the keys were baked along
    uint32 m_sunArcEpoch;         // counts the arcs the keys were baked along (16 bits, never 0)
    bool m_hasSunKeys;
    uint32 m_seenLightsGeneration;
    uint64 m_seenEmittersHash;
    float m_seenFoliageExtinction;
    float m_seenNearReach;
    uint32 m_nearTracer;

    uint32 m_numSurfels;
    uint32 m_numDropped;
    uint32 m_numFoliageVoxels;
    uint32 m_numFoliageDropped;
    Time m_startTime;
    Time m_lastLogTime;

    GlimmerLightShaderData m_composedLight;
    bool m_hasComposedLight;
    bool m_wasComposedWithClouds;
};

} // namespace Hyperion
