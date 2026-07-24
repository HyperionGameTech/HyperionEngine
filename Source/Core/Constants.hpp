/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <type_traits>
#include <utility>

namespace Hyperion {

#if !defined(HYP_VERSION_MAJOR) || !defined(HYP_VERSION_MINOR) || !defined(HYP_VERSION_PATCH)

#if defined(HYP_BUILD_LIBRARY) && !defined(HYP_TOOL)
#error "HYP_VERSION_MAJOR, HYP_VERSION_MINOR, and HYP_VERSION_PATCH must be defined"
#endif

// Define to let build continue
#define HYP_VERSION_MAJOR 0
#define HYP_VERSION_MINOR 0
#define HYP_VERSION_PATCH 0

#endif

/// Use a ring buffer for writing proxy data from simulation thread which the render thread reads after N frames.
static constexpr bool UseRingBuffer = false;
static constexpr uint32 RingBufferDepth = UseRingBuffer ? 3 : 1;

static constexpr uint8 EngineVersionMajor = HYP_VERSION_MAJOR;
static constexpr uint8 EngineVersionMinor = HYP_VERSION_MINOR;
static constexpr uint8 EngineVersionPatch = HYP_VERSION_PATCH;
static constexpr uint32 EngineVersion = (EngineVersionMajor << 16) | (EngineVersionMinor << 8) | EngineVersionPatch;
static constexpr uint64 EngineBinaryMagicNumber = (uint64(0x505948) << 32) | EngineVersion;

static constexpr uint32 NumFramesInFlight = 3;

static constexpr uint32 NumRendererWorkerThreads = 3;
static constexpr uint32 MaxBackgroundWorkerThreads = 4;

// Constants for types that have a global structured buffer.
// These are the maximum number of elements that can be stored in the corresponding global structured buffer or bound for rendering at any given time.
static constexpr uint32 MaxBoundEntities = 1u << 14;  // 16384
static constexpr uint32 MaxBoundMaterials = 1u << 10; // 1024
static constexpr uint32 MaxBoundSprites = 1u << 14;   // 16384
static constexpr uint32 MaxBoundWorlds = 16;
static constexpr uint32 MaxBoundCameras = 64;
static constexpr uint32 MaxBoundLights = 4096;
static constexpr uint32 MaxBoundEnvProbes = 16;
static constexpr uint32 MaxBoundReflectionProbes = 16;
static constexpr uint32 MaxBoundAmbientProbes = 16;
static constexpr uint32 MaxBoundSkeletons = 64;
static constexpr uint32 MaxBoundProbeVolumes = 16;
static constexpr uint32 MaxBoundOmniShadowMaps = 16;
static constexpr uint32 MaxBoundLightmapVolumes = 16;

// Types that do not have a global structured buffer.
static constexpr uint32 MaxBoundMeshes = 1u << 15;           // 32768
static constexpr uint32 MaxBoundResourceTextures = 1u << 14; // 16384
static constexpr uint32 MaxBoundParticleVolumes = 256;
static constexpr uint32 MaxBoundFogVolumes = 256;

// Per-material texture slots.
static constexpr uint32 MaxBoundTextures = 16;
static constexpr uint32 MaxBoundLightsForwardShading = 4;

static constexpr uint32 MaxShadowMapCascades = 4;
static constexpr uint32 MaxClusteredShadowMaps = 16;

static constexpr uint32 NumGBufferTargets = 5;

static constexpr uint32 MaxEntitiesPerBatch = 16;
static constexpr uint32 MaxEntityInstanceBatches = 4096;

static constexpr uint32 MaxBonesPerSkeleton = 64;

static constexpr uint32 MaxGpuTimers = 64;
static constexpr uint32 MaxGpuTimestampQueriesPerFrame = MaxGpuTimers * 2;

static constexpr uint32 MaxAtlasesPerLightmapVolume = 4;

#if HYP_ANDROID
static constexpr const char AndroidAssetPathPrefix[] = "$Android";
#endif

} // namespace Hyperion
