#ifndef HYPERION_V2_BACKEND_RENDERER_SHADER_H
#define HYPERION_V2_BACKEND_RENDERER_SHADER_H

#include <util/Defines.hpp>

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererShader.hpp>
#else
#error Unsupported rendering backend
#endif

#endif