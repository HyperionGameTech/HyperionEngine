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

#include <Rendering/vulkan/VulkanMemory.hpp>
#include <Rendering/vulkan/VulkanResult.hpp>
#include <Rendering/vulkan/VulkanHelpers.hpp>
#include <Rendering/vulkan/VulkanMemoryAllocator.hpp>

#include <Framework/EngineGlobals.hpp>
#include <Framework/EngineMemory.hpp>

#endif // __cplusplus
