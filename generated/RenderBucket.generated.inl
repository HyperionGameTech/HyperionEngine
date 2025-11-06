#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region RenderBucket Reflection Data

HYP_BEGIN_ENUM(RenderBucket, 280, 0, {})
    StaticField(NAME(HYP_STR(RB_NONE)), RenderBucket::RB_NONE),
    StaticField(NAME(HYP_STR(RB_OPAQUE)), RenderBucket::RB_OPAQUE),
    StaticField(NAME(HYP_STR(RB_LIGHTMAP)), RenderBucket::RB_LIGHTMAP),
    StaticField(NAME(HYP_STR(RB_TRANSLUCENT)), RenderBucket::RB_TRANSLUCENT),
    StaticField(NAME(HYP_STR(RB_SKYBOX)), RenderBucket::RB_SKYBOX),
    StaticField(NAME(HYP_STR(RB_DEBUG)), RenderBucket::RB_DEBUG),
    StaticField(NAME(HYP_STR(RB_MAX)), RenderBucket::RB_MAX)
HYP_END_ENUM

#pragma endregion RenderBucket Reflection Data

} // namespace hyperion

