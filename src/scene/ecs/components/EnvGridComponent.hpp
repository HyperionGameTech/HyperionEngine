/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_ENV_GRID_COMPONENT_HPP
#define HYPERION_ECS_ENV_GRID_COMPONENT_HPP

#include <core/Name.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <scene/camera/Camera.hpp>

#include <math/Matrix4.hpp>
#include <math/Extent.hpp>
#include <math/Vector3.hpp>

#include <rendering/EnvGrid.hpp>

#include <HashCode.hpp>

namespace hyperion {

HYP_STRUCT(Component, Label="EnvGrid Component", Description="Allows calculatation of indirect lighting in the area surrounding the entity", Editor=true)
struct EnvGridComponent
{
    HYP_FIELD(Property="EnvGridType", Serialize=true, Editor=true, Label="EnvGrid Type")
    EnvGridType env_grid_type = ENV_GRID_TYPE_SH;

    HYP_FIELD(Property="GridSize", Serialize=true, Editor=true, Label="Grid Size")
    Vec3u       grid_size = { 16, 4, 16 };

    HYP_FIELD(Property="EnvGrid", Serialize=false, Editor=true, Label="EnvGrid")
    RC<EnvGrid> env_grid;

    HYP_FIELD()
    HashCode    transform_hash_code;
};

} // namespace hyperion

#endif