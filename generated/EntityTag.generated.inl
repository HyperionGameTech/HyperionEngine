#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region EntityTag Reflection Data

HYP_BEGIN_ENUM(EntityTag, 372, 0, {})
    HypConstant(NAME(HYP_STR(NONE)), EntityTag::NONE),
    HypConstant(NAME(HYP_STR(STATIC)), EntityTag::STATIC),
    HypConstant(NAME(HYP_STR(DYNAMIC)), EntityTag::DYNAMIC),
    HypConstant(NAME(HYP_STR(LIGHT)), EntityTag::LIGHT),
    HypConstant(NAME(HYP_STR(CAMERA_PRIMARY)), EntityTag::CAMERA_PRIMARY),
    HypConstant(NAME(HYP_STR(LIGHTMAP_ELEMENT)), EntityTag::LIGHTMAP_ELEMENT),
    HypConstant(NAME(HYP_STR(RECEIVES_UPDATE)), EntityTag::RECEIVES_UPDATE),
    HypConstant(NAME(HYP_STR(SAVABLE_MAX)), EntityTag::SAVABLE_MAX),
    HypConstant(NAME(HYP_STR(UI_OBJECT_VISIBLE)), EntityTag::UI_OBJECT_VISIBLE),
    HypConstant(NAME(HYP_STR(EDITOR_FOCUSED)), EntityTag::EDITOR_FOCUSED),
    HypConstant(NAME(HYP_STR(UPDATE_AABB)), EntityTag::UPDATE_AABB),
    HypConstant(NAME(HYP_STR(UPDATE_RENDER_PROXY)), EntityTag::UPDATE_RENDER_PROXY),
    HypConstant(NAME(HYP_STR(UPDATE_VISIBILITY_STATE)), EntityTag::UPDATE_VISIBILITY_STATE),
    HypConstant(NAME(HYP_STR(TYPE_ID)), EntityTag::TYPE_ID),
    HypConstant(NAME(HYP_STR(TYPE_ID_MASK)), EntityTag::TYPE_ID_MASK)
HYP_END_ENUM

#pragma endregion EntityTag Reflection Data

} // namespace hyperion

#include <scene/ComponentInterface.hpp>

namespace hyperion {

#pragma region TagComponentBase Reflection Data

HYP_BEGIN_STRUCT(TagComponentBase, 373, 0, {}, HypClassAttribute("component", true))
    HypField(NAME(HYP_STR(Value)), &TagComponentBase::value, offsetof(TagComponentBase, value))
HYP_END_STRUCT

#pragma endregion TagComponentBase Reflection Data

HYP_REGISTER_COMPONENT(TagComponentBase);
} // namespace hyperion

