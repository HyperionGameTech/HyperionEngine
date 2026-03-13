#pragma once

#ifdef __cplusplus

#include <HyperionPch.hpp>

#include <vulkan/vulkan.h>
#if HYP_WINDOWS
#include <vulkan/vulkan_win32.h>
#elif HYP_APPLE
#include <vulkan/vulkan_metal.h>
#elif HYP_LINUX && !HYP_ANDROID
#include <vulkan/vulkan_xlib.h>
#endif

#include <rendering/RenderResult.hpp>
#include <rendering/Shared.hpp>

#include <rendering/vulkan/VulkanMemory.hpp>
#include <rendering/vulkan/VulkanResult.hpp>
#include <rendering/vulkan/VulkanHelpers.hpp>
#include <rendering/vulkan/VulkanMemoryAllocator.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineMemory.hpp>

#endif // __cplusplus