#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region RenderStatsCountType Reflection Data

HYP_BEGIN_ENUM(RenderStatsCountType, 334, 0, {})
    StaticField(NAME(HYP_STR(ERS_DRAW_CALLS)), RenderStatsCountType::ERS_DRAW_CALLS),
    StaticField(NAME(HYP_STR(ERS_INSTANCED_DRAW_CALLS)), RenderStatsCountType::ERS_INSTANCED_DRAW_CALLS),
    StaticField(NAME(HYP_STR(ERS_TRIANGLES)), RenderStatsCountType::ERS_TRIANGLES),
    StaticField(NAME(HYP_STR(ERS_RENDER_GROUPS)), RenderStatsCountType::ERS_RENDER_GROUPS),
    StaticField(NAME(HYP_STR(ERS_VIEWS)), RenderStatsCountType::ERS_VIEWS),
    StaticField(NAME(HYP_STR(ERS_TEXTURES)), RenderStatsCountType::ERS_TEXTURES),
    StaticField(NAME(HYP_STR(ERS_MATERIALS)), RenderStatsCountType::ERS_MATERIALS),
    StaticField(NAME(HYP_STR(ERS_LIGHTS)), RenderStatsCountType::ERS_LIGHTS),
    StaticField(NAME(HYP_STR(ERS_LIGHTMAP_VOLUMES)), RenderStatsCountType::ERS_LIGHTMAP_VOLUMES),
    StaticField(NAME(HYP_STR(ERS_ENV_PROBES)), RenderStatsCountType::ERS_ENV_PROBES),
    StaticField(NAME(HYP_STR(ERS_ENV_GRIDS)), RenderStatsCountType::ERS_ENV_GRIDS),
    StaticField(NAME(HYP_STR(ERS_DEBUG_DRAWS)), RenderStatsCountType::ERS_DEBUG_DRAWS),
    StaticField(NAME(HYP_STR(ERS_MAX)), RenderStatsCountType::ERS_MAX)
HYP_END_ENUM

#pragma endregion RenderStatsCountType Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region RenderStats Reflection Data

HYP_BEGIN_STRUCT(RenderStats, 335, 0, {})
HYP_END_STRUCT

#pragma endregion RenderStats Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region RenderStatsCounts Reflection Data

HYP_BEGIN_STRUCT(RenderStatsCounts, 336, 0, {})
HYP_END_STRUCT

#pragma endregion RenderStatsCounts Reflection Data

} // namespace hyperion

