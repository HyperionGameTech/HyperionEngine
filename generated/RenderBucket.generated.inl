#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region RenderBucket Reflection Data

HYP_BEGIN_ENUM(RenderBucket, 282, 0, {})
    HypConstant(NAME(HYP_STR(RB_NONE)), RenderBucket::RB_NONE),
    HypConstant(NAME(HYP_STR(RB_OPAQUE)), RenderBucket::RB_OPAQUE),
    HypConstant(NAME(HYP_STR(RB_LIGHTMAP)), RenderBucket::RB_LIGHTMAP),
    HypConstant(NAME(HYP_STR(RB_TRANSLUCENT)), RenderBucket::RB_TRANSLUCENT),
    HypConstant(NAME(HYP_STR(RB_SKYBOX)), RenderBucket::RB_SKYBOX),
    HypConstant(NAME(HYP_STR(RB_DEBUG)), RenderBucket::RB_DEBUG),
    HypConstant(NAME(HYP_STR(RB_MAX)), RenderBucket::RB_MAX)
HYP_END_ENUM

#pragma endregion RenderBucket Reflection Data

} // namespace hyperion

