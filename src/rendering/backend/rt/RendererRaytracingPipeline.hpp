#ifndef HYPERION_V2_BACKEND_RENDERER_RAYTRACING_PIPELINE_H
#define HYPERION_V2_BACKEND_RENDERER_RAYTRACING_PIPELINE_H

#include <util/Defines.hpp>

#if HYP_VULKAN
#include <rendering/backend/vulkan/rt/RendererRaytracingPipeline.hpp>
#else
#error Unsupported rendering backend
#endif

#endif