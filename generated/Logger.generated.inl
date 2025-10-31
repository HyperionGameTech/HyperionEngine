#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region Logger Reflection Data

HYP_BEGIN_CLASS(Logger, 28, 0, NAME("HypObjectBase"))
HYP_END_CLASS

#pragma endregion Logger Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region LogChannel Reflection Data

HYP_BEGIN_STRUCT(LogChannel, 254, 0, {})
    HypField(NAME(HYP_STR(Id)), &LogChannel::id, offsetof(LogChannel, id)),
    HypField(NAME(HYP_STR(Name)), &LogChannel::name, offsetof(LogChannel, name)),
    HypField(NAME(HYP_STR(ParentChannel)), &LogChannel::parentChannel, offsetof(LogChannel, parentChannel)),
    HypField(NAME(HYP_STR(MaskBitset)), &LogChannel::maskBitset, offsetof(LogChannel, maskBitset), Span<const HypClassAttribute> { {HypClassAttribute("noscriptbindings", true) } })
HYP_END_STRUCT

#pragma endregion LogChannel Reflection Data

} // namespace hyperion

