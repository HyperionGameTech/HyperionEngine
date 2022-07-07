#ifndef HYPERION_V2_BACKEND_RENDERER_IMAGE_H
#define HYPERION_V2_BACKEND_RENDERER_IMAGE_H

#include <util/Defines.hpp>

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererImage.hpp>
#else
#error Unsupported rendering backend
#endif

#endif