#ifndef HYPERION_V2_BACKEND_RENDERER_RESULT_H
#define HYPERION_V2_BACKEND_RENDERER_RESULT_H

#include <util/Defines.hpp>

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererResult.hpp>
#else
#error Unsupported rendering backend
#endif

#endif