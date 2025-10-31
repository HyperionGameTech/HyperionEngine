#include <core/reflection/HypObjectMacros.hpp>
#include <core/reflection/HypClassUtils.hpp>

namespace hyperion {

#pragma region ShadowMapType Reflection Data

HYP_BEGIN_ENUM(ShadowMapType, 353, 0, {})
    HypConstant(NAME(HYP_STR(SMT_DIRECTIONAL)), ShadowMapType::SMT_DIRECTIONAL),
    HypConstant(NAME(HYP_STR(SMT_SPOT)), ShadowMapType::SMT_SPOT),
    HypConstant(NAME(HYP_STR(SMT_OMNI)), ShadowMapType::SMT_OMNI)
HYP_END_ENUM

#pragma endregion ShadowMapType Reflection Data

} // namespace hyperion


namespace hyperion {

#pragma region ShadowMapFilter Reflection Data

HYP_BEGIN_ENUM(ShadowMapFilter, 354, 0, {})
    HypConstant(NAME(HYP_STR(SMF_STANDARD)), ShadowMapFilter::SMF_STANDARD),
    HypConstant(NAME(HYP_STR(SMF_PCF)), ShadowMapFilter::SMF_PCF),
    HypConstant(NAME(HYP_STR(SMF_CONTACT_HARDENED)), ShadowMapFilter::SMF_CONTACT_HARDENED),
    HypConstant(NAME(HYP_STR(SMF_VSM)), ShadowMapFilter::SMF_VSM),
    HypConstant(NAME(HYP_STR(SMF_MAX)), ShadowMapFilter::SMF_MAX)
HYP_END_ENUM

#pragma endregion ShadowMapFilter Reflection Data

} // namespace hyperion

