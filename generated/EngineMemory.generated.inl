#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region EnginePoolName Reflection Data

HYP_BEGIN_ENUM(EnginePoolName, 415, 0, {})
    HypConstant(NAME(HYP_STR(EPN_INVALID)), EnginePoolName::EPN_INVALID),
    HypConstant(NAME(HYP_STR(EPN_CORE)), EnginePoolName::EPN_CORE),
    HypConstant(NAME(HYP_STR(EPN_RENDER)), EnginePoolName::EPN_RENDER),
    HypConstant(NAME(HYP_STR(EPN_SCENE)), EnginePoolName::EPN_SCENE),
    HypConstant(NAME(HYP_STR(EPN_MAX)), EnginePoolName::EPN_MAX)
HYP_END_ENUM

#pragma endregion EnginePoolName Reflection Data

} // namespace hyperion

