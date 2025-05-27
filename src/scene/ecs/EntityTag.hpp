/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_ENTITY_TAG_HPP
#define HYPERION_ECS_ENTITY_TAG_HPP

#include <core/Defines.hpp>

#include <Types.hpp>

namespace hyperion {

HYP_ENUM()
enum class EntityTag : uint32
{
    NONE,

    STATIC,
    DYNAMIC,

    LIGHT, /* associated with a LightComponent */

    UI, /* associated with a UIObject */

    CAMERA,
    CAMERA_PRIMARY,

    DESCRIPTOR_MAX, // Maximum value used for things like Octree entry hashes.

    UI_OBJECT_VISIBLE = DESCRIPTOR_MAX,

    EDITOR_FOCUSED,

    UPDATE_AABB,
    UPDATE_BVH,
    UPDATE_BLAS,
    UPDATE_LIGHT_TRANSFORM,
    UPDATE_RENDER_PROXY,
    UPDATE_VISIBILITY_STATE,
    UPDATE_CAMERA_TRANSFORM,
    UPDATE_ENV_GRID_TRANSFORM,
    UPDATE_ENV_GRID,
    UPDATE_ENV_PROBE_TRANSFORM,
    // UPDATE_NODE_TRANSFORM,  /* Node transform needs sync after updating TransformComponent */

    MAX
};

/*! \brief An EntityTag is a special component that is used to tag an entity with a specific flag.
 *
 *  \tparam tag The flag value
 */
template <EntityTag Tag>
struct EntityTagComponent
{
    static constexpr EntityTag value = Tag;
};

} // namespace hyperion

#endif