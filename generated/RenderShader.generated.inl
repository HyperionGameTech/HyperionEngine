#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region ShaderModuleType Reflection Data

HYP_BEGIN_ENUM(ShaderModuleType, 325, 0, {})
    HypConstant(NAME(HYP_STR(SMT_UNSET)), ShaderModuleType::SMT_UNSET),
    HypConstant(NAME(HYP_STR(SMT_VERTEX)), ShaderModuleType::SMT_VERTEX),
    HypConstant(NAME(HYP_STR(SMT_FRAGMENT)), ShaderModuleType::SMT_FRAGMENT),
    HypConstant(NAME(HYP_STR(SMT_GEOMETRY)), ShaderModuleType::SMT_GEOMETRY),
    HypConstant(NAME(HYP_STR(SMT_COMPUTE)), ShaderModuleType::SMT_COMPUTE),
    HypConstant(NAME(HYP_STR(SMT_TASK)), ShaderModuleType::SMT_TASK),
    HypConstant(NAME(HYP_STR(SMT_MESH)), ShaderModuleType::SMT_MESH),
    HypConstant(NAME(HYP_STR(SMT_TESS_CONTROL)), ShaderModuleType::SMT_TESS_CONTROL),
    HypConstant(NAME(HYP_STR(SMT_TESS_EVAL)), ShaderModuleType::SMT_TESS_EVAL),
    HypConstant(NAME(HYP_STR(SMT_RAY_GEN)), ShaderModuleType::SMT_RAY_GEN),
    HypConstant(NAME(HYP_STR(SMT_RAY_INTERSECT)), ShaderModuleType::SMT_RAY_INTERSECT),
    HypConstant(NAME(HYP_STR(SMT_RAY_ANY_HIT)), ShaderModuleType::SMT_RAY_ANY_HIT),
    HypConstant(NAME(HYP_STR(SMT_RAY_CLOSEST_HIT)), ShaderModuleType::SMT_RAY_CLOSEST_HIT),
    HypConstant(NAME(HYP_STR(SMT_RAY_MISS)), ShaderModuleType::SMT_RAY_MISS),
    HypConstant(NAME(HYP_STR(SMT_MAX)), ShaderModuleType::SMT_MAX)
HYP_END_ENUM

#pragma endregion ShaderModuleType Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region ShaderBase Reflection Data

HYP_BEGIN_CLASS(ShaderBase, 101, 1, NAME("HypObjectBase"), HypClassAttribute("abstract", true),HypClassAttribute("noscriptbindings", true))
HYP_END_CLASS

#pragma endregion ShaderBase Reflection Data

} // namespace hyperion

