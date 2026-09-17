/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/Utilities/ByteUtil.hpp>

#include <Core/Types.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Util.hpp>

namespace Hyperion {

class Entity;

// clang-format off

#define HYP_FOR_EACH_ENTITY_TAG(X) \
    X(None, 0x0, false, false)                      \
    X(MobStatic, 0x1, true, false)                  \
    X(MobDynamic, 0x2, true, false)                 \
    X(Light, 0x3, true, false)                      \
    X(PrimaryCamera, 0x4, true, true)               \
    X(EditorCamera, 0x5, true, false)               \
    X(LightmapElement, 0x6, true, false)            \
    X(Replicated, 0x7, true, true)                  \
    X(ReceivesUpdate, 0x8, false, true)             \
    X(Player, 0x9, true, true)                      \
    X(UIVisible, 0x10, false, false)                \
    X(FocusedInEditor, 0x20, false, false)          \
    X(UpdateRenderProxy, 0x30, false, false)        \
    X(UpdateVisibility, 0x40, false, false)         \
    X(UpdateInstancedMeshData, 0x50, false, false)  \
    X(UpdateReplication, 0x60, false, false)        \
    X(UpdatePhysicsShape, 0x100, false, false)      \
    X(UpdatePhysicsMaterial, 0x200, false, false)

// clang-format on

HYP_STRUCT()
struct EntityTag
{
    HYP_STRUCT_BODY(EntityTag);

    HYP_FIELD()
    uint64 value;

#define MAKE_ENTITY_TAG_DECL(X, ...) static const EntityTag X;
    HYP_FOR_EACH_ENTITY_TAG(MAKE_ENTITY_TAG_DECL);
#undef MAKE_ENTITY_TAG_DECL

    static constexpr uint64 SerializableTagMask = 0xF;

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

#define HYP_ENTITY_TAG_DEF(Name, Value, ...) \
    inline constexpr EntityTag EntityTag::Name = EntityTag(Value);
HYP_FOR_EACH_ENTITY_TAG(HYP_ENTITY_TAG_DEF)
#undef HYP_ENTITY_TAG_DEF

inline constexpr EntityTag EntityTag::EntityTypeSentinel = EntityTag(1ull << 31);

static constexpr const EntityTag AllEntityTags[] = {
#define HYP_ENTITY_TAG_TAG(Name, Value, ...) EntityTag::Name,
    HYP_FOR_EACH_ENTITY_TAG(HYP_ENTITY_TAG_TAG)
#undef HYP_ENTITY_TAG_TAG
};

static constexpr const char* AllEntityTagNameStrings[] = {
#define HYP_ENTITY_TAG_STR(Name, Value, ...) HYP_STR(Name),
    HYP_FOR_EACH_ENTITY_TAG(HYP_ENTITY_TAG_STR)
#undef HYP_ENTITY_TAG_STR
};

static constexpr size_t MaxEntityTags = GetArrayCount(AllEntityTags);

/*! \brief Returns the name of the predefined EntityTag matching \p tag, or nullptr. */
inline constexpr const char* GetEntityTagName(EntityTag tag)
{
    for (size_t i = 0; i < MaxEntityTags; i++)
    {
        if (AllEntityTags[i] == tag)
        {
            return AllEntityTagNameStrings[i];
        }
    }

    return nullptr;
}

/*! \brief Returns the predefined EntityTag whose name matches \p nameHash, or None. */
inline constexpr EntityTag GetEntityTagByName(StringHash nameHash)
{
#define HYP_ENTITY_TAG_HASH(Name, Value, ...) \
    constexpr HashCode::ValueType Name##Hash = StringHash(#Name).hashCode;
    HYP_FOR_EACH_ENTITY_TAG(HYP_ENTITY_TAG_HASH)
#undef HYP_ENTITY_TAG_HASH

    switch (nameHash.hashCode)
    {
#define HYP_ENTITY_TAG_CASE(Name, Value, ...) \
    case Name##Hash:                     \
        return EntityTag::Name;
        HYP_FOR_EACH_ENTITY_TAG(HYP_ENTITY_TAG_CASE)
#undef HYP_ENTITY_TAG_CASE
    }

    return EntityTag::None;
}

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

    return TypeId(uint64(tag) >> 32);
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

template <class T>
struct EntityTypeTag : TagComponent<EntityType_Impl<T>::value>
{
};

/*! \brief A helper used to query for Entity instances with a specific type.
 *
 *  \tparam T The type of Entity
 */
template <class T>
using EntityType = EntityTypeTag<T>;

/*! \brief Trait used by EntitySetIterator to detect an EntityType<T> query */
template <class T>
struct EntityTypeTagInfo
{
    static constexpr bool IsEntityTypeTag = false;
};

template <class T>
struct EntityTypeTagInfo<EntityTypeTag<T>>
{
    static constexpr bool IsEntityTypeTag = true;
    using EntityHandleType = T;
};

} // namespace Hyperion
