#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region EntityTag Reflection Data

HYP_BEGIN_ENUM(EntityTag, 346, 0, {})
    StaticField(NAME(HYP_STR(NONE)), EntityTag::NONE),
    StaticField(NAME(HYP_STR(STATIC)), EntityTag::STATIC),
    StaticField(NAME(HYP_STR(DYNAMIC)), EntityTag::DYNAMIC),
    StaticField(NAME(HYP_STR(LIGHT)), EntityTag::LIGHT),
    StaticField(NAME(HYP_STR(CAMERA_PRIMARY)), EntityTag::CAMERA_PRIMARY),
    StaticField(NAME(HYP_STR(LIGHTMAP_ELEMENT)), EntityTag::LIGHTMAP_ELEMENT),
    StaticField(NAME(HYP_STR(RECEIVES_UPDATE)), EntityTag::RECEIVES_UPDATE),
    StaticField(NAME(HYP_STR(SAVABLE_MAX)), EntityTag::SAVABLE_MAX),
    StaticField(NAME(HYP_STR(UI_OBJECT_VISIBLE)), EntityTag::UI_OBJECT_VISIBLE),
    StaticField(NAME(HYP_STR(EDITOR_FOCUSED)), EntityTag::EDITOR_FOCUSED),
    StaticField(NAME(HYP_STR(UPDATE_AABB)), EntityTag::UPDATE_AABB),
    StaticField(NAME(HYP_STR(UPDATE_RENDER_PROXY)), EntityTag::UPDATE_RENDER_PROXY),
    StaticField(NAME(HYP_STR(UPDATE_VISIBILITY_STATE)), EntityTag::UPDATE_VISIBILITY_STATE),
    StaticField(NAME(HYP_STR(TYPE_ID)), EntityTag::TYPE_ID),
    StaticField(NAME(HYP_STR(TYPE_ID_MASK)), EntityTag::TYPE_ID_MASK)
HYP_END_ENUM

#pragma endregion EntityTag Reflection Data

} // namespace hyperion

#include <scene/ComponentInterface.hpp>

namespace hyperion {

#pragma region TagComponentBase Reflection Data

HYP_BEGIN_STRUCT(TagComponentBase, 347, 0, {}, ClassAttribute("component", true))
    Field(NAME(HYP_STR(Value)), &TagComponentBase::value, offsetof(TagComponentBase, value))
HYP_END_STRUCT

#pragma endregion TagComponentBase Reflection Data

HYP_REGISTER_COMPONENT(TagComponentBase);
} // namespace hyperion

