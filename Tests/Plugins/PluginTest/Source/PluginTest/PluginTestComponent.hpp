/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Defines.hpp>
#include <Core/HashCode.hpp>

#include <Core/Reflection/ObjectMacros.hpp>

namespace Hyperion {

HYP_STRUCT(Component, Label = "Plugin Test Component", Editor = true)
struct PLUGINTEST_API PluginTestComponent
{
    HYP_STRUCT_BODY(PluginTestComponent);

    HYP_FIELD(Property = "Speed", Serialize = true, Editor = true)
    float speed = 1.0f;

    HYP_FIELD(Property = "Ticks", Transient)
    uint32 ticks = 0;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(speed);

        return hc;
    }
};

} // namespace Hyperion
