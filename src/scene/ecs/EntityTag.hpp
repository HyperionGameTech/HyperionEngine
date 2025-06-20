/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_ECS_ENTITY_TAG_HPP
#define HYPERION_ECS_ENTITY_TAG_HPP

#include <core/Defines.hpp>

#include <core/utilities/TypeID.hpp>

#include <Types.hpp>

namespace hyperion {

class Entity;

enum class EntityTag : uint64
{
    NONE,

    STATIC,
    DYNAMIC,

    LIGHT, /* associated with a LightComponent */

    UI, /* associated with a UIObject */

    CAMERA_PRIMARY,

    LIGHTMAP_ELEMENT, /* Has an entry in a LightmapVolume - See MeshComponent lightmap_* fields */

    RECEIVES_UPDATE,

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
    UPDATE_ENV_PROBE_TRANSFORM,

    TYPE_ID = (uint64(1) << 31),            // Flag to indicate that this EntityTag is a TypeID tag
    TYPE_ID_MASK = uint64(0xFFFFFFFF) << 32 // Mask to get TypeID from EntityTag
};

static constexpr inline bool IsTypeIDEntityTag(EntityTag tag)
{
    return tag == EntityTag::TYPE_ID;
}

static constexpr inline TypeID GetTypeIDFromEntityTag(EntityTag tag)
{
    if (!IsTypeIDEntityTag(tag))
    {
        return TypeID::Void();
    }

    return TypeID(static_cast<uint64>(tag) >> 32);
}

template <class T>
struct EntityTagTypeID
{
    static constexpr EntityTag value = (std::is_void_v<T> || std::is_same_v<T, Entity>)
        ? EntityTag::TYPE_ID
        : EntityTag((static_cast<uint64>(TypeID::ForType<T>().Value()) << 32) | uint64(EntityTag::TYPE_ID));
};

static constexpr inline EntityTag MakeEntityTagFromTypeID(TypeID type_id)
{
    if (type_id == TypeID::Void() || type_id == TypeID::ForType<Entity>())
    {
        return EntityTag::TYPE_ID;
    }

    return EntityTag((static_cast<uint64>(type_id.Value()) << 32) | uint64(EntityTag::TYPE_ID));
}

/*! \brief An EntityTag is a special component that is used to tag an entity with a specific flag.
 *
 *  \tparam tag The flag value
 */
template <EntityTag Tag>
struct EntityTagComponent
{
    static constexpr EntityTag value = Tag;
};

/*! \brief A helper used to query for Entity instances with a specific type.
 *
 *  \tparam T The type of Entity
 */
template <class T>
using EntityType = EntityTagComponent<EntityTagTypeID<T>::value>;

} // namespace hyperion

#endif