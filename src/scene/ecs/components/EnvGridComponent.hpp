/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_ENV_GRID_COMPONENT_HPP
#define HYPERION_ECS_ENV_GRID_COMPONENT_HPP

#include <core/Name.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/memory/resource/Resource.hpp>

#include <core/utilities/EnumFlags.hpp>

#include <scene/EnvGrid.hpp>

#include <scene/camera/Camera.hpp>

#include <core/math/Extent.hpp>
#include <core/math/Matrix4.hpp>
#include <core/math/Vector3.hpp>

#include <HashCode.hpp>

namespace hyperion {

enum class EnvGridMobility : uint32
{
    STATIONARY = 0x0,

    FOLLOW_CAMERA_X = 0x1,
    FOLLOW_CAMERA_Y = 0x2,
    FOLLOW_CAMERA_Z = 0x4,

    FOLLOW_CAMERA = FOLLOW_CAMERA_X | FOLLOW_CAMERA_Y | FOLLOW_CAMERA_Z
};

HYP_MAKE_ENUM_FLAGS(EnvGridMobility)

class RenderEnvGrid;
class EnvGridRenderSubsystem;

HYP_STRUCT(Component, Label = "EnvGrid Component", Description = "Allows calculatation of indirect lighting in the area surrounding the entity", Editor = true)

struct EnvGridComponent
{
    HYP_FIELD(Property = "EnvGridType", Serialize = true, Editor = true, Label = "EnvGrid Type")
    EnvGridType env_grid_type = ENV_GRID_TYPE_SH;

    HYP_FIELD(Property = "GridSize", Serialize = true, Editor = true, Label = "Grid Size")
    Vec3u grid_size = { 24, 4, 24 };

    HYP_FIELD(Property = "Mobility", Serialize = true, Editor = true, Label = "Mobility")
    EnumFlags<EnvGridMobility> mobility = EnvGridMobility::STATIONARY;

    HYP_FIELD(Property = "EnvGrid", Serialize = false, Editor = true, Label = "EnvGrid")
    Handle<EnvGrid> env_grid;

    HYP_FIELD(Transient)
    TResourceHandle<RenderEnvGrid> env_render_grid;

    HYP_FIELD(Transient)
    RC<EnvGridRenderSubsystem> env_grid_render_subsystem;

    HYP_FIELD(Transient)
    HashCode octant_hash_code;
};

} // namespace hyperion

#endif