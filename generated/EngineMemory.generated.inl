#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region EnginePoolName Reflection Data

HYP_BEGIN_ENUM(EnginePoolName, 258, 0, {})
    StaticField(NAME(HYP_STR(EPN_INVALID)), EnginePoolName::EPN_INVALID),
    StaticField(NAME(HYP_STR(EPN_CORE)), EnginePoolName::EPN_CORE),
    StaticField(NAME(HYP_STR(EPN_RENDER)), EnginePoolName::EPN_RENDER),
    StaticField(NAME(HYP_STR(EPN_SCENE)), EnginePoolName::EPN_SCENE),
    StaticField(NAME(HYP_STR(EPN_MAX)), EnginePoolName::EPN_MAX)
HYP_END_ENUM

#pragma endregion EnginePoolName Reflection Data

} // namespace hyperion

