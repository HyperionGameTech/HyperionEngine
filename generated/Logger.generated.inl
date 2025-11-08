#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region Logger Reflection Data

HYP_BEGIN_CLASS(Logger, 29, 0, NAME("ObjectBase"))
HYP_END_CLASS

#pragma endregion Logger Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region LogChannel Reflection Data

HYP_BEGIN_STRUCT(LogChannel, 256, 0, {})
    Field(NAME(HYP_STR(Id)), &LogChannel::id, offsetof(LogChannel, id)),
    Field(NAME(HYP_STR(Name)), &LogChannel::name, offsetof(LogChannel, name)),
    Field(NAME(HYP_STR(ParentChannel)), &LogChannel::parentChannel, offsetof(LogChannel, parentChannel)),
    Field(NAME(HYP_STR(MaskBitset)), &LogChannel::maskBitset, offsetof(LogChannel, maskBitset), Span<const ClassAttribute> { {ClassAttribute("noscriptbindings", true) } })
HYP_END_STRUCT

#pragma endregion LogChannel Reflection Data

} // namespace hyperion

