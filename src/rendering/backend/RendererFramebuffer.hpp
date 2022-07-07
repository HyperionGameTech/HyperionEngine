#ifndef HYPERION_V2_BACKEND_RENDERER_FBO_H
#define HYPERION_V2_BACKEND_RENDERER_FBO_H

#include <util/Defines.hpp>

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererFramebuffer.hpp>
#else
#error Unsupported rendering backend
#endif

#endif