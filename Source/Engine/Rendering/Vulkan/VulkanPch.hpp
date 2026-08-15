#pragma once

#ifdef __cplusplus

#include <HyperionPch.hpp>

#include <vulkan/vulkan.h>
#if HYP_WINDOWS
#include <vulkan/vulkan_win32.h>
#elif HYP_APPLE
#include <vulkan/vulkan_metal.h>
#elif HYP_ANDROID
#include <vulkan/vulkan_android.h>
#elif HYP_LINUX
#include <vulkan/vulkan_xlib.h>
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
