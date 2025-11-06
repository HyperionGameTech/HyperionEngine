#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region BoundingBox Reflection Data

HYP_BEGIN_STRUCT(BoundingBox, 238, 0, {}, ClassAttribute("size", 32))
    Field(NAME(HYP_STR(Min)), &BoundingBox::min, offsetof(BoundingBox, min), Span<const ClassAttribute> { {ClassAttribute("property", "Min"), ClassAttribute("serialize", true), ClassAttribute("editor", true) } }),
    Field(NAME(HYP_STR(Max)), &BoundingBox::max, offsetof(BoundingBox, max), Span<const ClassAttribute> { {ClassAttribute("property", "Max"), ClassAttribute("serialize", true), ClassAttribute("editor", true) } })
HYP_END_STRUCT

#pragma endregion BoundingBox Reflection Data

static_assert(sizeof(BoundingBox) == 32, "Expected sizeof(BoundingBox) to be 32 bytes");
} // namespace hyperion

