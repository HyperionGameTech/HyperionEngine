#ifndef HYPERION_V2_BACKEND_RENDERER_OCCLUSION_QUERY_H
#define HYPERION_V2_BACKEND_RENDERER_OCCLUSION_QUERY_H

#include <util/Defines.hpp>

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererOcclusionQuery.hpp>
#else
#error Unsupported rendering backend
#endif

#endif
