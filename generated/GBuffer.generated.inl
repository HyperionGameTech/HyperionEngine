#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region GBuffer Reflection Data

HYP_BEGIN_CLASS(GBuffer, 76, 0, NAME("HypObjectBase"), HypClassAttribute("noscriptbindings", true))
HYP_END_CLASS

#pragma endregion GBuffer Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region GBufferTargetName Reflection Data

HYP_BEGIN_ENUM(GBufferTargetName, 265, 0, {})
    HypConstant(NAME(HYP_STR(GTN_ALBEDO)), GBufferTargetName::GTN_ALBEDO),
    HypConstant(NAME(HYP_STR(GTN_NORMALS)), GBufferTargetName::GTN_NORMALS),
    HypConstant(NAME(HYP_STR(GTN_MATERIAL)), GBufferTargetName::GTN_MATERIAL),
    HypConstant(NAME(HYP_STR(GTN_LIGHTMAP)), GBufferTargetName::GTN_LIGHTMAP),
    HypConstant(NAME(HYP_STR(GTN_VELOCITY)), GBufferTargetName::GTN_VELOCITY),
    HypConstant(NAME(HYP_STR(GTN_WS_NORMALS)), GBufferTargetName::GTN_WS_NORMALS),
    HypConstant(NAME(HYP_STR(GTN_DEPTH)), GBufferTargetName::GTN_DEPTH),
    HypConstant(NAME(HYP_STR(GTN_MAX)), GBufferTargetName::GTN_MAX)
HYP_END_ENUM

#pragma endregion GBufferTargetName Reflection Data

} // namespace hyperion

