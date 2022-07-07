#ifndef HYPERION_V2_BACKEND_RENDERER_INSTANCE_H
#define HYPERION_V2_BACKEND_RENDERER_INSTANCE_H

#include <util/Defines.hpp>

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererInstance.hpp>
#else
#error Unsupported rendering backend
#endif

#endif