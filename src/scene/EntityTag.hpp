/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/utilities/TypeId.hpp>
#include <core/utilities/ByteUtil.hpp>

#include <core/Types.hpp>

namespace hyperion {

class Entity;

enum class EntityTag : uint64
{
    NONE,

    STATIC,
    DYNAMIC,

    LIGHT,

    CAMERA_PRIMARY,

    LIGHTMAP_ELEMENT,

    RECEIVES_UPDATE,

    SAVABLE_MAX, // savable entity tags end here.

    UI_OBJECT_VISIBLE = SAVABLE_MAX,

    EDITOR_FOCUSED,

    UPDATE_AABB,
    UPDATE_RENDER_PROXY,
    UPDATE_VISIBILITY_STATE,

    TYPE_ID = (uint64(1) << 31),            // Flag to indicate that this EntityTag is an EntityType tag
    TYPE_ID_MASK = uint64(0xFFFFFFFF) << 32 // Mask to get TypeId from the vaue
};

static constexpr inline bool IsEntityTypeTag(EntityTag tag)
{
    return uint64(tag) & uint64(EntityTag::TYPE_ID);
}

static constexpr inline TypeId GetTypeIdFromEntityTag(EntityTag tag)
{
    static_assert(sizeof(TypeId) == sizeof(uint32), "Using this requires sizeof(TypeId) is 32 bytes");
    if (!IsEntityTypeTag(tag))
    {
        return TypeId::Void();
    }

    return TypeId(static_cast<uint64>(tag) >> 32);
}

template <class T>
struct EntityType_Impl
{
    //static_assert(std::is_base_of_v<Entity, T>, "T must be a base of Entity to use EntityType");
    static constexpr EntityTag value = (std::is_void_v<T> || std::is_same_v<T, Entity>)
        ? EntityTag::TYPE_ID
        : EntityTag((static_cast<uint64>(TypeId::ForType<T>().Value()) << 32) | uint64(EntityTag::TYPE_ID));
};

static constexpr inline EntityTag MakeEntityTypeTag(TypeId typeId)
{
    if (typeId == TypeId::Void() || typeId == TypeId::ForType<Entity>())
    {
        return EntityTag::TYPE_ID;
    }

    return EntityTag((static_cast<uint64>(typeId.Value()) << 32) | uint64(EntityTag::TYPE_ID));
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
using EntityType = EntityTagComponent<EntityType_Impl<T>::value>;

} // namespace hyperion
