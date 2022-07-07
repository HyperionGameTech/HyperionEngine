#ifndef HYPERION_V2_BACKEND_RENDERER_QUEUE_H
#define HYPERION_V2_BACKEND_RENDERER_QUEUE_H

#include <util/Defines.hpp>

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererQueue.hpp>
#else
#error Unsupported rendering backend
#endif

#endif