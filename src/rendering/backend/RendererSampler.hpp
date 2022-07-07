#ifndef HYPERION_V2_BACKEND_RENDERER_SAMPLER_H
#define HYPERION_V2_BACKEND_RENDERER_SAMPLER_H

#include <util/Defines.hpp>

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererSampler.hpp>
#else
#error Unsupported rendering backend
#endif

#endif