#ifndef HYPERION_V2_BACKEND_RENDERER_FRAME_H
#define HYPERION_V2_BACKEND_RENDERER_FRAME_H

#include <util/Defines.hpp>

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererFrame.hpp>
#else
#error Unsupported rendering backend
#endif

#endif