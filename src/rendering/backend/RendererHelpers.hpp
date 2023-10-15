#ifndef HYPERION_V2_BACKEND_RENDERER_HELPERS_H
#define HYPERION_V2_BACKEND_RENDERER_HELPERS_H

#include <util/Defines.hpp>
#include <Types.hpp>

namespace hyperion {
namespace renderer {
namespace helpers {

UInt MipmapSize(UInt src_size, Int lod);

} // namespace helpers
} // namespace renderer
} // namespace hyperion

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererHelpers.hpp>
#else
#error Unsupported rendering backend
#endif

#endif