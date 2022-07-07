#ifndef HYPERION_V2_BACKEND_RENDERER_FENCE_H
#define HYPERION_V2_BACKEND_RENDERER_FENCE_H

#include <util/Defines.hpp>

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererFence.hpp>
#else
#error Unsupported rendering backend
#endif

#endif