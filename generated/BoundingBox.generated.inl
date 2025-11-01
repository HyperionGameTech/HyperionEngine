#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region BoundingBox Reflection Data

HYP_BEGIN_STRUCT(BoundingBox, 237, 0, {}, HypClassAttribute("size", 32))
    HypField(NAME(HYP_STR(Min)), &BoundingBox::min, offsetof(BoundingBox, min), Span<const HypClassAttribute> { {HypClassAttribute("property", "Min"), HypClassAttribute("serialize", true), HypClassAttribute("editor", true) } }),
    HypField(NAME(HYP_STR(Max)), &BoundingBox::max, offsetof(BoundingBox, max), Span<const HypClassAttribute> { {HypClassAttribute("property", "Max"), HypClassAttribute("serialize", true), HypClassAttribute("editor", true) } })
HYP_END_STRUCT

#pragma endregion BoundingBox Reflection Data

static_assert(sizeof(BoundingBox) == 32, "Expected sizeof(BoundingBox) to be 32 bytes");
} // namespace hyperion

