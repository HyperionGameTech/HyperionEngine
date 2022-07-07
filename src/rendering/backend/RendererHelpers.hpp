#ifndef HYPERION_V2_BACKEND_RENDERER_HELPERS_H
#define HYPERION_V2_BACKEND_RENDERER_HELPERS_H

#include <util/Defines.hpp>

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererHelpers.hpp>
#else
#error Unsupported rendering backend
#endif

#endif