#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region GlobalRenderBuffer Reflection Data

HYP_BEGIN_ENUM(GlobalRenderBuffer, 292, 0, {})
    StaticField(NAME(HYP_STR(GRB_INVALID)), GlobalRenderBuffer::GRB_INVALID),
    StaticField(NAME(HYP_STR(GRB_WORLDS)), GlobalRenderBuffer::GRB_WORLDS),
    StaticField(NAME(HYP_STR(GRB_CAMERAS)), GlobalRenderBuffer::GRB_CAMERAS),
    StaticField(NAME(HYP_STR(GRB_LIGHTS)), GlobalRenderBuffer::GRB_LIGHTS),
    StaticField(NAME(HYP_STR(GRB_ENTITIES)), GlobalRenderBuffer::GRB_ENTITIES),
    StaticField(NAME(HYP_STR(GRB_MATERIALS)), GlobalRenderBuffer::GRB_MATERIALS),
    StaticField(NAME(HYP_STR(GRB_SKELETONS)), GlobalRenderBuffer::GRB_SKELETONS),
    StaticField(NAME(HYP_STR(GRB_ENV_PROBES)), GlobalRenderBuffer::GRB_ENV_PROBES),
    StaticField(NAME(HYP_STR(GRB_ENV_GRIDS)), GlobalRenderBuffer::GRB_ENV_GRIDS),
    StaticField(NAME(HYP_STR(GRB_LIGHTMAP_VOLUMES)), GlobalRenderBuffer::GRB_LIGHTMAP_VOLUMES),
    StaticField(NAME(HYP_STR(GRB_MAX)), GlobalRenderBuffer::GRB_MAX)
HYP_END_ENUM

#pragma endregion GlobalRenderBuffer Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region GlobalRendererType Reflection Data

HYP_BEGIN_ENUM(GlobalRendererType, 293, 0, {})
    StaticField(NAME(HYP_STR(GRT_NONE)), GlobalRendererType::GRT_NONE),
    StaticField(NAME(HYP_STR(GRT_ENV_PROBE)), GlobalRendererType::GRT_ENV_PROBE),
    StaticField(NAME(HYP_STR(GRT_ENV_GRID)), GlobalRendererType::GRT_ENV_GRID),
    StaticField(NAME(HYP_STR(GRT_SHADOW_MAP)), GlobalRendererType::GRT_SHADOW_MAP),
    StaticField(NAME(HYP_STR(GRT_UI)), GlobalRendererType::GRT_UI),
    StaticField(NAME(HYP_STR(GRT_MAX)), GlobalRendererType::GRT_MAX)
HYP_END_ENUM

#pragma endregion GlobalRendererType Reflection Data

} // namespace hyperion

