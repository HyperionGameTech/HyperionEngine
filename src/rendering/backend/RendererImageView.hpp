#ifndef HYPERION_V2_BACKEND_RENDERER_IMAGE_VIEW_H
#define HYPERION_V2_BACKEND_RENDERER_IMAGE_VIEW_H

#include <rendering/backend/Platform.hpp>
#include <util/Defines.hpp>

namespace hyperion {
namespace renderer {
namespace platform {

template <PlatformType PLATFORM>
class ImageView
{
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererImageView.hpp>
#else
#error Unsupported rendering backend
#endif

namespace hyperion {
namespace renderer {

using ImageView = platform::ImageView<Platform::CURRENT>;

} // namespace renderer
} // namespace hyperion

#endif