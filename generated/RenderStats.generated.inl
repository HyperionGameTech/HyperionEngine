#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region RenderStatsCountType Reflection Data

HYP_BEGIN_ENUM(RenderStatsCountType, 333, 0, {})
    HypConstant(NAME(HYP_STR(ERS_DRAW_CALLS)), RenderStatsCountType::ERS_DRAW_CALLS),
    HypConstant(NAME(HYP_STR(ERS_INSTANCED_DRAW_CALLS)), RenderStatsCountType::ERS_INSTANCED_DRAW_CALLS),
    HypConstant(NAME(HYP_STR(ERS_TRIANGLES)), RenderStatsCountType::ERS_TRIANGLES),
    HypConstant(NAME(HYP_STR(ERS_RENDER_GROUPS)), RenderStatsCountType::ERS_RENDER_GROUPS),
    HypConstant(NAME(HYP_STR(ERS_VIEWS)), RenderStatsCountType::ERS_VIEWS),
    HypConstant(NAME(HYP_STR(ERS_TEXTURES)), RenderStatsCountType::ERS_TEXTURES),
    HypConstant(NAME(HYP_STR(ERS_MATERIALS)), RenderStatsCountType::ERS_MATERIALS),
    HypConstant(NAME(HYP_STR(ERS_LIGHTS)), RenderStatsCountType::ERS_LIGHTS),
    HypConstant(NAME(HYP_STR(ERS_LIGHTMAP_VOLUMES)), RenderStatsCountType::ERS_LIGHTMAP_VOLUMES),
    HypConstant(NAME(HYP_STR(ERS_ENV_PROBES)), RenderStatsCountType::ERS_ENV_PROBES),
    HypConstant(NAME(HYP_STR(ERS_ENV_GRIDS)), RenderStatsCountType::ERS_ENV_GRIDS),
    HypConstant(NAME(HYP_STR(ERS_DEBUG_DRAWS)), RenderStatsCountType::ERS_DEBUG_DRAWS),
    HypConstant(NAME(HYP_STR(ERS_MAX)), RenderStatsCountType::ERS_MAX)
HYP_END_ENUM

#pragma endregion RenderStatsCountType Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region RenderStats Reflection Data

HYP_BEGIN_STRUCT(RenderStats, 334, 0, {})
HYP_END_STRUCT

#pragma endregion RenderStats Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region RenderStatsCounts Reflection Data

HYP_BEGIN_STRUCT(RenderStatsCounts, 335, 0, {})
HYP_END_STRUCT

#pragma endregion RenderStatsCounts Reflection Data

} // namespace hyperion

