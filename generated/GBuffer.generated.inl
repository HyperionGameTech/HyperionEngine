#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region GBuffer Reflection Data

HYP_BEGIN_CLASS(GBuffer, 103, 0, NAME("HypObjectBase"), ClassAttribute("noscriptbindings", true))
HYP_END_CLASS

#pragma endregion GBuffer Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region GBufferTargetName Reflection Data

HYP_BEGIN_ENUM(GBufferTargetName, 327, 0, {})
    StaticField(NAME(HYP_STR(GTN_ALBEDO)), GBufferTargetName::GTN_ALBEDO),
    StaticField(NAME(HYP_STR(GTN_NORMALS)), GBufferTargetName::GTN_NORMALS),
    StaticField(NAME(HYP_STR(GTN_MATERIAL)), GBufferTargetName::GTN_MATERIAL),
    StaticField(NAME(HYP_STR(GTN_LIGHTMAP)), GBufferTargetName::GTN_LIGHTMAP),
    StaticField(NAME(HYP_STR(GTN_VELOCITY)), GBufferTargetName::GTN_VELOCITY),
    StaticField(NAME(HYP_STR(GTN_WS_NORMALS)), GBufferTargetName::GTN_WS_NORMALS),
    StaticField(NAME(HYP_STR(GTN_DEPTH)), GBufferTargetName::GTN_DEPTH),
    StaticField(NAME(HYP_STR(GTN_MAX)), GBufferTargetName::GTN_MAX)
HYP_END_ENUM

#pragma endregion GBufferTargetName Reflection Data

} // namespace hyperion

