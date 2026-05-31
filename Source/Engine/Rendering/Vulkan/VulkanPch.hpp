#pragma once

#ifdef __cplusplus

#include <HyperionPch.hpp>

#include <Vulkan/vulkan.h>
#if HYP_WINDOWS
#include <Vulkan/vulkan_win32.h>
#elif HYP_APPLE
#include <Vulkan/vulkan_metal.h>
#elif HYP_ANDROID
#include <Vulkan/vulkan_android.h>
#elif HYP_LINUX
#include <Vulkan/vulkan_xlib.h>
#endif

#include <Rendering/RenderResult.hpp>
#include <Rendering/Shared.hpp>

#include <Rendering/Vulkan/VulkanMemory.hpp>
#include <Rendering/Vulkan/VulkanResult.hpp>
#include <Rendering/Vulkan/VulkanHelpers.hpp>
#include <Rendering/Vulkan/VulkanMemoryAllocator.hpp>

#include <Framework/EngineGlobals.hpp>
#include <Framework/EngineMemory.hpp>

#endif // __cplusplus
