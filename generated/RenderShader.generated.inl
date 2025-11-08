#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region ShaderModuleType Reflection Data

HYP_BEGIN_ENUM(ShaderModuleType, 296, 0, {})
    StaticField(NAME(HYP_STR(SMT_UNSET)), ShaderModuleType::SMT_UNSET),
    StaticField(NAME(HYP_STR(SMT_VERTEX)), ShaderModuleType::SMT_VERTEX),
    StaticField(NAME(HYP_STR(SMT_FRAGMENT)), ShaderModuleType::SMT_FRAGMENT),
    StaticField(NAME(HYP_STR(SMT_GEOMETRY)), ShaderModuleType::SMT_GEOMETRY),
    StaticField(NAME(HYP_STR(SMT_COMPUTE)), ShaderModuleType::SMT_COMPUTE),
    StaticField(NAME(HYP_STR(SMT_TASK)), ShaderModuleType::SMT_TASK),
    StaticField(NAME(HYP_STR(SMT_MESH)), ShaderModuleType::SMT_MESH),
    StaticField(NAME(HYP_STR(SMT_TESS_CONTROL)), ShaderModuleType::SMT_TESS_CONTROL),
    StaticField(NAME(HYP_STR(SMT_TESS_EVAL)), ShaderModuleType::SMT_TESS_EVAL),
    StaticField(NAME(HYP_STR(SMT_RAY_GEN)), ShaderModuleType::SMT_RAY_GEN),
    StaticField(NAME(HYP_STR(SMT_RAY_INTERSECT)), ShaderModuleType::SMT_RAY_INTERSECT),
    StaticField(NAME(HYP_STR(SMT_RAY_ANY_HIT)), ShaderModuleType::SMT_RAY_ANY_HIT),
    StaticField(NAME(HYP_STR(SMT_RAY_CLOSEST_HIT)), ShaderModuleType::SMT_RAY_CLOSEST_HIT),
    StaticField(NAME(HYP_STR(SMT_RAY_MISS)), ShaderModuleType::SMT_RAY_MISS),
    StaticField(NAME(HYP_STR(SMT_MAX)), ShaderModuleType::SMT_MAX)
HYP_END_ENUM

#pragma endregion ShaderModuleType Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region ShaderBase Reflection Data

HYP_BEGIN_CLASS(ShaderBase, 116, 1, NAME("ObjectBase"), ClassAttribute("abstract", true),ClassAttribute("noscriptbindings", true))
HYP_END_CLASS

#pragma endregion ShaderBase Reflection Data

} // namespace hyperion

