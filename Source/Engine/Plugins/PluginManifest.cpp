/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Engine/Plugins/PluginManifest.hpp>

#include <Core/DataProcessing/JSON/JSON.hpp>

#include <Core/IO/ByteReader.hpp>

#include <Core/Utilities/StringUtil.hpp>

namespace Hyperion {

TResult<PluginManifest> PluginManifest::Parse(const FilePath& manifestPath)
{
    if (!manifestPath.Exists())
    {
        return HYP_MAKE_ERROR(Error, "Plugin manifest does not exist at {}", manifestPath);
    }

    FileByteReader stream { manifestPath };

    if (stream.Eof())
    {
        return HYP_MAKE_ERROR(Error, "Failed to open plugin manifest at {}", manifestPath);
    }

    ByteBuffer buffer = stream.Read();
    String manifestStr = String(buffer.ToByteView());

    JSON::ParseResult parseResult = JSON::Parse(manifestStr);

    if (!parseResult.ok)
    {
        return HYP_MAKE_ERROR(Error, "Failed to parse plugin manifest at {}: {}", manifestPath, parseResult.message);
    }

    PluginManifest manifest;

    const auto nameValue = parseResult.value.Get("name");

    if (nameValue.IsUndefined() || !nameValue.IsString())
    {
        return HYP_MAKE_ERROR(Error, "Plugin manifest at {} is missing required string field \"name\"", manifestPath);
    }

    manifest.name = nameValue.AsString();

    if (manifest.name.Empty())
    {
        return HYP_MAKE_ERROR(Error, "Plugin manifest at {} has an empty \"name\"", manifestPath);
    }

    const auto versionValue = parseResult.value.Get("version");

    if (!versionValue.IsUndefined() && versionValue.IsString())
    {
        manifest.version = versionValue.AsString();
    }

    const auto engineMinVersionValue = parseResult.value.Get("engineMinVersion");

    if (!engineMinVersionValue.IsUndefined() && engineMinVersionValue.IsString())
    {
        manifest.engineMinVersion = engineMinVersionValue.AsString();
    }

    const auto entryValue = parseResult.value.Get("entry");

    if (!entryValue.IsUndefined() && entryValue.IsString())
    {
        manifest.entry = entryValue.AsString();
    }

    if (manifest.entry.Empty())
    {
        manifest.entry = manifest.name + ".dll";
    }

    const auto hostValue = parseResult.value.Get("host");

    if (!hostValue.IsUndefined() && hostValue.IsString())
    {
        const String host = hostValue.AsString().ToLower();

        if (host == "editor")
        {
            manifest.hostFlags = HypPluginHostFlags::Editor;
        }
        else if (host == "game")
        {
            manifest.hostFlags = HypPluginHostFlags::Game;
        }
        else if (host == "both")
        {
            manifest.hostFlags = HypPluginHostFlags::Editor | HypPluginHostFlags::Game;
        }
        else
        {
            return HYP_MAKE_ERROR(Error,
                "Plugin manifest at {} has invalid \"host\" value \"{}\" (expected editor, game or both)",
                manifestPath,
                host);
        }
    }
    else
    {
        manifest.hostFlags = HypPluginHostFlags::Editor | HypPluginHostFlags::Game;
    }

    return manifest;
}

} // namespace Hyperion
