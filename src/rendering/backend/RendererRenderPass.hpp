#ifndef HYPERION_V2_BACKEND_RENDERER_RENDER_PASS_H
#define HYPERION_V2_BACKEND_RENDERER_RENDER_PASS_H

#include <util/Defines.hpp>

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererRenderPass.hpp>
#else
#error Unsupported rendering backend
#endif

#endif