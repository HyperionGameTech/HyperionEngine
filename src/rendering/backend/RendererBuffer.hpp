#ifndef HYPERION_V2_BACKEND_RENDERER_BUFFER_H
#define HYPERION_V2_BACKEND_RENDERER_BUFFER_H

#include <util/Defines.hpp>
#include <Types.hpp>

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererBuffer.hpp>
#else
#error Unsupported rendering backend
#endif

#endif