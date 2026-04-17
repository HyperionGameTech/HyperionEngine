/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/utilities/ByteUtil.hpp>

#include <Core/Types.hpp>

namespace Hyperion {

class Entity;

HYP_STRUCT()
struct EntityTag
{
    HYP_STRUCT_BODY(EntityTag);

    HYP_FIELD()
    uint64 value;

    static const EntityTag None;

    /// Persistent tags
    static const EntityTag MobStatic;
    static const EntityTag MobDynamic;

    static const EntityTag Light;

    static const EntityTag PrimaryCamera;
    static const EntityTag EditorCamera;

    static const EntityTag LightmapElement;

    static const EntityTag ReceivesUpdate;

    static constexpr uint64 SerializableTagMask = 0xF;

    /// Non-persistent
    static const EntityTag UIVisible;

    static const EntityTag FocusedInEditor;

    static const EntityTag UpdateRenderProxy;
    static const EntityTag UpdateVisibility;
    static const EntityTag UpdateInstancedMeshData;

    static const EntityTag UpdatePhysicsShape;
    static const EntityTag UpdatePhysicsMaterial;

    static const EntityTag EntityTypeSentinel;

    constexpr EntityTag()
        : value(0)
    {
    }

    constexpr explicit EntityTag(uint64 value)
        : value(value)
    {
    }

    constexpr EntityTag(const EntityTag& other) = default;
    EntityTag& operator=(const EntityTag& other) = default;

    constexpr EntityTag(EntityTag&& other) noexcept
        : value(other.value)
    {
        other.value = 0;
    }

    EntityTag& operator=(EntityTag&& other) noexcept
    {
        value = other.value;
        other.value = 0;

        return *this;
    }

    HYP_FORCE_INLINE constexpr explicit operator bool() const
    {
        return value != 0;
    }

    HYP_FORCE_INLINE constexpr bool operator!() const
    {
        return value == 0;
    }

    HYP_FORCE_INLINE constexpr bool operator==(const EntityTag& other) const
    {
        return value == other.value;
    }

    HYP_FORCE_INLINE constexpr bool operator!=(const EntityTag& other) const
    {
        return value != other.value;
    }

    HYP_FORCE_INLINE constexpr explicit operator uint64() const
    {
        return value;
    }

    HYP_FORCE_INLINE constexpr HashCode GetHashCode() const
    {
        return HashCode(HashCode::ValueType(value));
    }
};

inline constexpr EntityTag EntityTag::None = EntityTag(0x0);

// Persistent tags

inline constexpr EntityTag EntityTag::MobStatic = EntityTag(0x1);
inline constexpr EntityTag EntityTag::MobDynamic = EntityTag(0x2);

inline constexpr EntityTag EntityTag::Light = EntityTag(0x3);

inline constexpr EntityTag EntityTag::PrimaryCamera = EntityTag(0x4);
inline constexpr EntityTag EntityTag::EditorCamera = EntityTag(0x5);

inline constexpr EntityTag EntityTag::LightmapElement = EntityTag(0x6);

inline constexpr EntityTag EntityTag::ReceivesUpdate = EntityTag(0x7);

// Non-persistent

inline constexpr EntityTag EntityTag::UIVisible = EntityTag(0x10);

inline constexpr EntityTag EntityTag::FocusedInEditor = EntityTag(0x20);

inline constexpr EntityTag EntityTag::UpdateRenderProxy = EntityTag(0x30);
inline constexpr EntityTag EntityTag::UpdateVisibility = EntityTag(0x40);
inline constexpr EntityTag EntityTag::UpdateInstancedMeshData = EntityTag(0x50);

inline constexpr EntityTag EntityTag::UpdatePhysicsShape = EntityTag(0x60);
inline constexpr EntityTag EntityTag::UpdatePhysicsMaterial = EntityTag(0x70);

inline constexpr EntityTag EntityTag::EntityTypeSentinel = EntityTag(1ull << 31);

// Mask for the actual TypeId to be stored in the upper 32 bits of the EntityTag value.
static constexpr uint64 EntityTypeTagMask = 0xFFFFFFFF00000000ull;

static constexpr inline bool IsEntityTypeTag(EntityTag tag)
{
    return (uint64(tag) & uint64(EntityTag::EntityTypeSentinel)) != 0;
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
        ? EntityTag::EntityTypeSentinel
        : EntityTag((uint64(CONSTEXPR_TYPE_ID(T)) << 32) | uint64(EntityTag::EntityTypeSentinel));
};

static constexpr inline EntityTag MakeEntityTypeTag(TypeId typeId)
{
    if (typeId == TypeId::Void() || typeId == TypeId::ForType<Entity>())
    {
        return EntityTag::EntityTypeSentinel;
    }

    return EntityTag((uint64(typeId.Value()) << 32) | uint64(EntityTag::EntityTypeSentinel));
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
