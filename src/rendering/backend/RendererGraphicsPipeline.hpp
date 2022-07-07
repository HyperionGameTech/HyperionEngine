#ifndef HYPERION_V2_BACKEND_RENDERER_GRAPHICS_PIPELINE_H
#define HYPERION_V2_BACKEND_RENDERER_GRAPHICS_PIPELINE_H

#include <util/Defines.hpp>

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererGraphicsPipeline.hpp>
#else
#error Unsupported rendering backend
#endif

#endif