#ifndef HYPERION_V2_BACKEND_RENDERER_SEMAPHORE_H
#define HYPERION_V2_BACKEND_RENDERER_SEMAPHORE_H

#include <util/Defines.hpp>

namespace hyperion {
namespace renderer {

enum class SemaphoreType
{
    WAIT,
    SIGNAL
};

} // namespace renderer
} // namespace hyperion

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererSemaphore.hpp>
#else
#error Unsupported rendering backend
#endif

#endif