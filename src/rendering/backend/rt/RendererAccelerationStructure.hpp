/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_V2_BACKEND_RENDERER_ACCELERATION_STRUCTURE_H
#define HYPERION_V2_BACKEND_RENDERER_ACCELERATION_STRUCTURE_H

#include <core/Defines.hpp>
#include <Types.hpp>

namespace hyperion {
namespace renderer {

enum class AccelerationStructureType
{
    BOTTOM_LEVEL,
    TOP_LEVEL
};

using RTUpdateStateFlags = uint;

enum RTUpdateStateFlagBits : RTUpdateStateFlags
{
    RT_UPDATE_STATE_FLAGS_NONE                          = 0x0,
    RT_UPDATE_STATE_FLAGS_UPDATE_ACCELERATION_STRUCTURE = 0x1,
    RT_UPDATE_STATE_FLAGS_UPDATE_MESH_DESCRIPTIONS      = 0x2,
    RT_UPDATE_STATE_FLAGS_UPDATE_INSTANCES              = 0x4,
    RT_UPDATE_STATE_FLAGS_UPDATE_TRANSFORM              = 0x8
};

using AccelerationStructureFlags = uint;

enum AccelerationStructureFlagBits : AccelerationStructureFlags
{
    ACCELERATION_STRUCTURE_FLAGS_NONE               = 0x0,
    ACCELERATION_STRUCTURE_FLAGS_NEEDS_REBUILDING   = 0x1,
    ACCELERATION_STRUCTURE_FLAGS_TRANSFORM_UPDATE   = 0x2,
    ACCELERATION_STRUCTURE_FLAGS_MATERIAL_UPDATE    = 0x4
};

} // namespace renderer
} // namespace hyperion

#if HYP_VULKAN
#include <rendering/backend/vulkan/rt/RendererAccelerationStructure.hpp>
#else
#error Unsupported rendering backend
#endif

namespace hyperion {
namespace renderer {

using AccelerationGeometry = platform::AccelerationGeometry<Platform::CURRENT>;
using AccelerationStructure = platform::AccelerationStructure<Platform::CURRENT>;
using TopLevelAccelerationStructure = platform::TopLevelAccelerationStructure<Platform::CURRENT>;
using BottomLevelAccelerationStructure = platform::BottomLevelAccelerationStructure<Platform::CURRENT>;

} // namespace renderer
} // namespace hyperion

#endif