/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_ENV_GRID_COMPONENT_HPP
#define HYPERION_ECS_ENV_GRID_COMPONENT_HPP

#include <core/Handle.hpp>
#include <core/Name.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <scene/camera/Camera.hpp>

#include <math/Matrix4.hpp>
#include <math/Extent.hpp>
#include <math/Vector3.hpp>

#include <rendering/EnvGrid.hpp>

#include <HashCode.hpp>

namespace hyperion {

HYP_STRUCT()
struct EnvGridComponent
{
    HYP_FIELD()
    EnvGridType env_grid_type = ENV_GRID_TYPE_SH;

    HYP_FIELD()
    Vec3u       grid_size = { 16, 4, 16 };

    HYP_FIELD()
    RC<EnvGrid> render_component;

    HYP_FIELD()
    HashCode    transform_hash_code;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hash_code;

        hash_code.Add(env_grid_type);
        hash_code.Add(grid_size);

        return hash_code;
    }
};

} // namespace hyperion

#endif