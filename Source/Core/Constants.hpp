/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

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

constexpr uint32 MaxBoundReflectionProbes = 16;
constexpr uint32 MaxBoundEnvGrids = 16;
constexpr uint32 MaxBoundAmbientProbes = 4096;
constexpr uint32 MaxBoundOmniShadowMaps = 8;
constexpr uint32 MaxBoundTextures = 16;
constexpr uint32 MaxBoundLightmapVolumes = 4;

constexpr uint32 MaxShadowMapCascades = 4;

constexpr uint32 NumGBufferTargets = 5;

constexpr uint32 MaxEntitiesPerBatch = 60;

template <class... T>
constexpr bool ResolutionFailureV = false;

template <class T>
using NormalizedType = std::conditional_t<std::is_function_v<T>, std::add_pointer_t<T>, std::remove_cvref_t<T>>;

template <class T>
constexpr bool IsPodTypeV = std::is_standard_layout_v<T>
    && std::is_trivially_copyable_v<T>
    && std::is_trivially_copy_assignable_v<T>
    && std::is_trivially_move_constructible_v<T>
    && std::is_trivially_move_assignable_v<T>
    && std::is_trivially_destructible_v<T>;

template <class T, SizeType = sizeof(T)>
std::true_type ImplementationExistsImpl(T*);

std::false_type ImplementationExistsImpl(...);

template <class T>
constexpr bool ImplementationExistsV = decltype(ImplementationExistsImpl(std::declval<T*>()))::value;

template <class T>
constexpr bool IsConstPointerV = std::is_pointer_v<T> && std::is_const_v<std::remove_pointer_t<T>>;

template <class T>
using RemoveConstPointerT = std::add_pointer_t<std::remove_const_t<std::remove_pointer_t<T>>>;

} // namespace Hyperion
