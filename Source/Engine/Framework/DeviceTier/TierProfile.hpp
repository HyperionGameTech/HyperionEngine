/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Defines.hpp>

#include <Core/Reflection/ObjectFwd.hpp>
#include <Core/Reflection/ObjectMacros.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/Map.hpp>
#include <Core/Containers/String.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Utilities/Variant.hpp>

namespace Hyperion {

HYP_STRUCT()
struct TierRule
{
    HYP_STRUCT_BODY(TierRule);

    HYP_FIELD(Property = "Name", Serialize)
    Name name;

    HYP_FIELD(Property = "When", Serialize)
    Map<Name, Name> when;

    HYP_FIELD(Property = "AtMost", Serialize)
    Map<Name, Name> atMost;

    HYP_FIELD(Property = "Set", Serialize)
    Map<Name, Variant<bool, double, String>> set;
};

HYP_STRUCT()
struct TierProfile
{
    HYP_STRUCT_BODY(TierProfile);

    HYP_FIELD(Property = "Rules", Serialize)
    Array<TierRule> rules;
};

} // namespace Hyperion
