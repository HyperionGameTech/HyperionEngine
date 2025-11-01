#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region GlobalRenderBuffer Reflection Data

HYP_BEGIN_ENUM(GlobalRenderBuffer, 293, 0, {})
    HypConstant(NAME(HYP_STR(GRB_INVALID)), GlobalRenderBuffer::GRB_INVALID),
    HypConstant(NAME(HYP_STR(GRB_WORLDS)), GlobalRenderBuffer::GRB_WORLDS),
    HypConstant(NAME(HYP_STR(GRB_CAMERAS)), GlobalRenderBuffer::GRB_CAMERAS),
    HypConstant(NAME(HYP_STR(GRB_LIGHTS)), GlobalRenderBuffer::GRB_LIGHTS),
    HypConstant(NAME(HYP_STR(GRB_ENTITIES)), GlobalRenderBuffer::GRB_ENTITIES),
    HypConstant(NAME(HYP_STR(GRB_MATERIALS)), GlobalRenderBuffer::GRB_MATERIALS),
    HypConstant(NAME(HYP_STR(GRB_SKELETONS)), GlobalRenderBuffer::GRB_SKELETONS),
    HypConstant(NAME(HYP_STR(GRB_ENV_PROBES)), GlobalRenderBuffer::GRB_ENV_PROBES),
    HypConstant(NAME(HYP_STR(GRB_ENV_GRIDS)), GlobalRenderBuffer::GRB_ENV_GRIDS),
    HypConstant(NAME(HYP_STR(GRB_LIGHTMAP_VOLUMES)), GlobalRenderBuffer::GRB_LIGHTMAP_VOLUMES),
    HypConstant(NAME(HYP_STR(GRB_MAX)), GlobalRenderBuffer::GRB_MAX)
HYP_END_ENUM

#pragma endregion GlobalRenderBuffer Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region GlobalRendererType Reflection Data

HYP_BEGIN_ENUM(GlobalRendererType, 294, 0, {})
    HypConstant(NAME(HYP_STR(GRT_NONE)), GlobalRendererType::GRT_NONE),
    HypConstant(NAME(HYP_STR(GRT_ENV_PROBE)), GlobalRendererType::GRT_ENV_PROBE),
    HypConstant(NAME(HYP_STR(GRT_ENV_GRID)), GlobalRendererType::GRT_ENV_GRID),
    HypConstant(NAME(HYP_STR(GRT_SHADOW_MAP)), GlobalRendererType::GRT_SHADOW_MAP),
    HypConstant(NAME(HYP_STR(GRT_UI)), GlobalRendererType::GRT_UI),
    HypConstant(NAME(HYP_STR(GRT_MAX)), GlobalRendererType::GRT_MAX)
HYP_END_ENUM

#pragma endregion GlobalRendererType Reflection Data

} // namespace hyperion

