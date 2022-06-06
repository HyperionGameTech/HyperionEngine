#ifndef HYPERION_V2_BACKEND_RENDERER_OCCLUSION_QUERY_H
#define HYPERION_V2_BACKEND_RENDERER_OCCLUSION_QUERY_H

#include <util/defines.h>

#if HYP_VULKAN
#include <rendering/backend/vulkan/renderer_occlusion_query.h>
#else
#error Unsupported rendering backend
#endif

#endif
