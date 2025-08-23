/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>
#include <core/Types.hpp>

#include <type_traits>
#include <utility>

namespace hyperion {

#if !defined(HYP_VERSION_MAJOR) || !defined(HYP_VERSION_MINOR) || !defined(HYP_VERSION_PATCH)

#ifndef HYP_BUILDTOOL
#error "HYP_VERSION_MAJOR, HYP_VERSION_MINOR, and HYP_VERSION_PATCH must be defined"
#endif

// Define to let build continue
#define HYP_VERSION_MAJOR 0
#define HYP_VERSION_MINOR 0
#define HYP_VERSION_PATCH 0

#endif

static constexpr bool g_tripleBuffer = true;
static constexpr uint32 g_numMultiBuffers = g_tripleBuffer ? 3 : 2;

constexpr uint8 g_engineMajorVersion = HYP_VERSION_MAJOR;
constexpr uint8 g_engineMinorVersion = HYP_VERSION_MINOR;
constexpr uint8 g_enginePatchVersion = HYP_VERSION_PATCH;
constexpr uint32 g_engineVersion = (g_engineMajorVersion << 16) | (g_engineMinorVersion << 8) | g_enginePatchVersion;
constexpr uint64 g_engineBinaryMagicNumber = (uint64(0x505948) << 32) | g_engineVersion;

constexpr uint32 g_framesInFlight = 2;
constexpr uint32 g_numAsyncRenderingCommandBuffers = 4;

constexpr uint32 g_maxBoundReflectionProbes = 16;
constexpr uint32 g_maxBoundAmbientProbes = 4096;
constexpr uint32 g_maxBoundPointShadowMaps = 8;
constexpr uint32 g_maxBoundTextures = 16;

constexpr uint32 g_maxBindlessResources = 4096;

constexpr uint32 g_numGbufferTargets = 7;

template <class... T>
constexpr bool resolutionFailure = false;

template <class T>
using NormalizedType = std::conditional_t<std::is_function_v<T>, std::add_pointer_t<T>, std::remove_cvref_t<T>>;

template <class T>
constexpr bool isPodType = std::is_standard_layout_v<T>
    && std::is_trivially_copyable_v<T>
    && std::is_trivially_copy_assignable_v<T>
    && std::is_trivially_move_constructible_v<T>
    && std::is_trivially_move_assignable_v<T>
    && std::is_trivially_destructible_v<T>;

template <class T, SizeType = sizeof(T)>
std::true_type implementationExistsImpl(T*);

std::false_type implementationExistsImpl(...);

template <class T>
constexpr bool implementationExists = decltype(implementationExistsImpl(std::declval<T*>()))::value;

template <class T>
constexpr bool isConstPointer = std::is_pointer_v<T> && std::is_const_v<std::remove_pointer_t<T>>;
template <class T>
using RemoveConstPointer = std::add_pointer_t<std::remove_const_t<std::remove_pointer_t<T>>>;

} // namespace hyperion
