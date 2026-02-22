/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <Core/Defines.hpp>

#include <Core/utilities/ByteUtil.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

class Entity;

HYP_ENUM()
enum class EntityTag : uint64
{
    None,

    MobStatic,
    MobDynamic,

    Light,

    PrimaryCamera,

    LightmapElement,

    ReceivesUpdate,

    MaxPersistent, // persistent entity tags end here.

    UIVisible = MaxPersistent,

    FocusedInEditor,

    UpdateRenderProxy,
    UpdateVisibility,

    EntityType = 2147483648,            // Flag to indicate that this EntityTag is an EntityType tag
    EntityTypeMask = 0xFFFFFFFF00000000 // Mask to get TypeId from the vaue
};

static constexpr inline bool IsEntityTypeTag(EntityTag tag)
{
    return uint64(tag) & uint64(EntityTag::EntityType);
}

static constexpr inline TypeId GetTypeIdFromEntityTag(EntityTag tag)
{
    static_assert(sizeof(TypeId) == sizeof(uint32), "Using this requires sizeof(TypeId) is 32 bit");
    if (!IsEntityTypeTag(tag))
    {
        return TypeId::Void();
    }

    return TypeId(static_cast<uint64>(tag) >> 32);
}

template <class T>
struct EntityType_Impl
{
    // static_assert(std::is_base_of_v<Entity, T>, "T must be a base of Entity to use EntityType");
    static constexpr EntityTag value = (std::is_void_v<T> || std::is_same_v<T, Entity>)
        ? EntityTag::EntityType
        : EntityTag((uint64(CONSTEXPR_TYPE_ID(T)) << 32) | uint64(EntityTag::EntityType));
};

static constexpr inline EntityTag MakeEntityTypeTag(TypeId typeId)
{
    if (typeId == TypeId::Void() || typeId == TypeId::ForType<Entity>())
    {
        return EntityTag::EntityType;
    }

    return EntityTag((static_cast<uint64>(typeId.Value()) << 32) | uint64(EntityTag::EntityType));
}

HYP_STRUCT(Component)
struct TagComponentBase
{
    HYP_STRUCT_BODY(TagComponentBase);

    HYP_FIELD()
    EntityTag value = EntityTag::None;
};

/*! \brief An EntityTag is a special component that is used to tag an entity with a specific flag.
 *
 *  \tparam tag The flag value
 */
template <EntityTag TEntityTag>
struct TagComponent : TagComponentBase
{
    static constexpr EntityTag Tag = TEntityTag;

    TagComponent()
    {
        TagComponentBase::value = TEntityTag;
    }
};

/*! \brief A helper used to query for Entity instances with a specific type.
 *
 *  \tparam T The type of Entity
 */
template <class T>
using EntityType = TagComponent<EntityType_Impl<T>::value>;

} // namespace Hyperion
