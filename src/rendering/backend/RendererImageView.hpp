#ifndef HYPERION_V2_BACKEND_RENDERER_IMAGE_VIEW_H
#define HYPERION_V2_BACKEND_RENDERER_IMAGE_VIEW_H

#include <util/Defines.hpp>

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererImageView.hpp>
#else
#error Unsupported rendering backend
#endif

#endif