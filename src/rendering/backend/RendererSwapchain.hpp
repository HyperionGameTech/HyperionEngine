#ifndef HYPERION_V2_BACKEND_RENDERER_SWAPCHAIN_H
#define HYPERION_V2_BACKEND_RENDERER_SWAPCHAIN_H

#include <util/Defines.hpp>

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererSwapchain.hpp>
#else
#error Unsupported rendering backend
#endif

#endif