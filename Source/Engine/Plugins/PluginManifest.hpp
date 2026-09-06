/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/Containers/String.hpp>

#include <Core/FileSystem/FilePath.hpp>

#include <Core/Utilities/Result.hpp>

#include <Engine/Plugins/PluginAPI.hpp>

namespace Hyperion {

struct PluginManifest
{
    String name;
    String version = "0.0.0";
    String engineMinVersion;
    String entry;
    EnumFlags<HypPluginHostFlags> hostFlags = HypPluginHostFlags::None;

    static TResult<PluginManifest> Parse(const FilePath& manifestPath);
};

} // namespace Hyperion
