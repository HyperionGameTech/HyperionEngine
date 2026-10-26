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
#include <Core/Containers/String.hpp>

#include <Core/FileSystem/FilePath.hpp>

#include <Core/Name/Name.hpp>

namespace Hyperion {

/// <Plugin>/<Plugin>.hypplugin

HYP_STRUCT()
struct PluginDescriptor
{
    HYP_STRUCT_BODY(PluginDescriptor);

    HYP_FIELD(Property = "Name", Serialize)
    Name name;

    HYP_FIELD(Property = "Version", Serialize)
    String version = "0.0.0";

    HYP_FIELD(Property = "Description", Serialize)
    String description;

    HYP_FIELD(Property = "EnabledByDefault", Serialize)
    bool enabledByDefault = false;

    HYP_FIELD(Property = "Dependencies", Serialize)
    Array<Name> dependencies;

    FilePath directory;
};

} // namespace Hyperion
