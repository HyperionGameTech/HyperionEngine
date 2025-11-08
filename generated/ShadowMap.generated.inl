#include <core/reflection/ObjectMacros.hpp>
#include <core/reflection/ClassUtils.hpp>

namespace hyperion {

#pragma region ShadowMapType Reflection Data

HYP_BEGIN_ENUM(ShadowMapType, 258, 0, {})
    StaticField(NAME(HYP_STR(SMT_DIRECTIONAL)), ShadowMapType::SMT_DIRECTIONAL),
    StaticField(NAME(HYP_STR(SMT_SPOT)), ShadowMapType::SMT_SPOT),
    StaticField(NAME(HYP_STR(SMT_OMNI)), ShadowMapType::SMT_OMNI)
HYP_END_ENUM

#pragma endregion ShadowMapType Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region ShadowMapFilter Reflection Data

HYP_BEGIN_ENUM(ShadowMapFilter, 259, 0, {})
    StaticField(NAME(HYP_STR(SMF_STANDARD)), ShadowMapFilter::SMF_STANDARD),
    StaticField(NAME(HYP_STR(SMF_PCF)), ShadowMapFilter::SMF_PCF),
    StaticField(NAME(HYP_STR(SMF_CONTACT_HARDENED)), ShadowMapFilter::SMF_CONTACT_HARDENED),
    StaticField(NAME(HYP_STR(SMF_VSM)), ShadowMapFilter::SMF_VSM),
    StaticField(NAME(HYP_STR(SMF_MAX)), ShadowMapFilter::SMF_MAX)
HYP_END_ENUM

#pragma endregion ShadowMapFilter Reflection Data

} // namespace hyperion

