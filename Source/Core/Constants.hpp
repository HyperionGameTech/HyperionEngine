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

static constexpr bool UseTripleBuffering = true;
static constexpr uint32 RingBufferDepth = UseTripleBuffering ? 3 : 2;

constexpr uint8 EngineVersionMajor = HYP_VERSION_MAJOR;
constexpr uint8 EngineVersionMinor = HYP_VERSION_MINOR;
constexpr uint8 EngineVersionPatch = HYP_VERSION_PATCH;
constexpr uint32 EngineVersion = (EngineVersionMajor << 16) | (EngineVersionMinor << 8) | EngineVersionPatch;
constexpr uint64 EngineBinaryMagicNumber = (uint64(0x505948) << 32) | EngineVersion;

constexpr uint32 NumFramesInFlight = 3;
constexpr uint32 NumAsyncCommandBuffers = 4;

constexpr uint32 NumRendererWorkerThreads = 2;
constexpr uint32 MaxBackgroundWorkerThreads = 4;

// Constants for types that have a global structured buffer.
// These are the maximum number of elements that can be stored in the corresponding global structured buffer or bound for rendering at any given time.
constexpr uint32 MaxBoundWorlds = 16;
constexpr uint32 MaxBoundCameras = 1024;
constexpr uint32 MaxBoundLights = 1024;
constexpr uint32 MaxBoundEntities = 1u << 15;  // 32768
constexpr uint32 MaxBoundMaterials = 1u << 10; // 1024
constexpr uint32 MaxBoundSprites = 1u << 14;   // 16384
constexpr uint32 MaxBoundEnvProbes = 1024;
constexpr uint32 MaxBoundSkeletons = 64;
constexpr uint32 MaxBoundReflectionProbes = 16;
constexpr uint32 MaxBoundProbeVolumes = 16;
constexpr uint32 MaxBoundAmbientProbes = 4096;
constexpr uint32 MaxBoundOmniShadowMaps = 8;
constexpr uint32 MaxBoundLightmapVolumes = 4;

// Types that do not have a global structured buffer.
constexpr uint32 MaxBoundMeshes = 1u << 15;           // 32768
constexpr uint32 MaxBoundResourceTextures = 1u << 14; // 16384
constexpr uint32 MaxBoundParticleVolumes = 256;
constexpr uint32 MaxBoundFogVolumes = 256;

// Per-material texture slots.
constexpr uint32 MaxBoundTextures = 16;
constexpr uint32 MaxBoundLightsForwardShading = 4;

constexpr uint32 MaxShadowMapCascades = 4;
constexpr uint32 MaxClusteredShadowMaps = 16;

constexpr uint32 NumGBufferTargets = 5;

constexpr uint32 MaxEntitiesPerBatch = 16;
constexpr uint32 MaxEntityInstanceBatches = 4096;

constexpr uint32 MaxBonesPerSkeleton = 64;

constexpr uint32 MaxGpuTimers = 64;
constexpr uint32 MaxGpuTimestampQueriesPerFrame = MaxGpuTimers * 2;

constexpr uint32 MaxAtlasesPerLightmapVolume = 4;

#if HYP_ANDROID
constexpr const char AndroidAssetPathPrefix[] = "$Android";
#endif

} // namespace Hyperion
