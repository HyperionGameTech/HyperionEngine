#ifndef HYPERION_V2_BACKEND_RENDERER_FRAME_HANDLER_H
#define HYPERION_V2_BACKEND_RENDERER_FRAME_HANDLER_H

#include <util/Defines.hpp>

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererFrameHandler.hpp>
#else
#error Unsupported rendering backend
#endif

#endif