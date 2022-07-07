#ifndef HYPERION_V2_BACKEND_RENDERER_ACCELERATION_STRUCTURE_H
#define HYPERION_V2_BACKEND_RENDERER_ACCELERATION_STRUCTURE_H

#include <util/Defines.hpp>

#if HYP_VULKAN
#include <rendering/backend/vulkan/rt/RendererAccelerationStructure.hpp>
#else
#error Unsupported rendering backend
#endif

#endif