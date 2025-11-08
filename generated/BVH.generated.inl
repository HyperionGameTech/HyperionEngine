#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region BVHNode Reflection Data

HYP_BEGIN_STRUCT(BVHNode, 381, 0, {})
    Field(NAME(HYP_STR(Aabb)), &BVHNode::aabb, offsetof(BVHNode, aabb), Span<const ClassAttribute> { {ClassAttribute("serialize", true) } }),
    Field(NAME(HYP_STR(Children)), &BVHNode::children, offsetof(BVHNode, children), Span<const ClassAttribute> { {ClassAttribute("serialize", true) } }),
    Field(NAME(HYP_STR(TriangleIds)), &BVHNode::triangleIds, offsetof(BVHNode, triangleIds), Span<const ClassAttribute> { {ClassAttribute("serialize", true) } }),
    Field(NAME(HYP_STR(Flags)), &BVHNode::flags, offsetof(BVHNode, flags), Span<const ClassAttribute> { {ClassAttribute("serialize", true) } })
HYP_END_STRUCT

#pragma endregion BVHNode Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region BvhFlags Reflection Data

HYP_BEGIN_ENUM(BvhFlags, 382, 0, {})
    StaticField(NAME(HYP_STR(BF_NONE)), BvhFlags::BF_NONE),
    StaticField(NAME(HYP_STR(BF_IS_LEAF_NODE)), BvhFlags::BF_IS_LEAF_NODE)
HYP_END_ENUM

#pragma endregion BvhFlags Reflection Data

} // namespace hyperion

