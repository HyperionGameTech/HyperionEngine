/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Core/Containers/Array.hpp>
#include <Core/Containers/String.hpp>

#include <Core/Name/Name.hpp>

#include <Core/FileSystem/FilePath.hpp>

#include <Core/Utilities/Result.hpp>

#include <Asset/AssetPath.hpp>

#include <Rendering/RenderableAttributes.hpp>

#include <Editor/EditorMemory.hpp>
#include <Editor/Net/EditorNetSettings.hpp>

namespace Hyperion {

HYP_STRUCT()
struct EditorWebSettings
{
    HYP_STRUCT_BODY(EditorWebSettings);

    HYP_FIELD(Property = "Worlds", Serialize)
    String worlds;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hashCode;
        hashCode.Add(worlds);

        return hashCode;
    }
};

HYP_STRUCT()
struct EditorCookSettings
{
    HYP_STRUCT_BODY(EditorCookSettings);

    HYP_FIELD(Property = "AdditionalAssets", Serialize)
    Array<AssetPath> additionalAssets;

    HYP_FIELD(Property = "RuntimeMaterials", Serialize)
    Array<MaterialAttributes> runtimeMaterials;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hashCode = additionalAssets.GetHashCode();
        hashCode.Add(runtimeMaterials.GetHashCode());

        return hashCode;
    }
};

HYP_STRUCT()
struct EditorPlatformSettings
{
    HYP_STRUCT_BODY(EditorPlatformSettings);

    HYP_FIELD(Property = "Web", Serialize)
    EditorWebSettings web;

    HYP_FIELD(Property = "Cook", Serialize)
    EditorCookSettings cook;

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hashCode = web.GetHashCode();
        hashCode.Add(cook.GetHashCode());

        return hashCode;
    }
};

HYP_CLASS()
class EDITOR_API EditorProjectSettings final : public ObjectBase
{
    HYP_OBJECT_BODY(EditorProjectSettings);

public:
    EditorProjectSettings() = default;

    HYP_FIELD(Property = "GameTitle", Serialize, Editor, Title = "Game Title")
    String gameTitle;

    HYP_FIELD(Property = "ModuleName", Serialize)
    String moduleName;

    HYP_FIELD(Property = "NetSettings", Serialize, Editor, Title = "Networking")
    EditorNetSettings netSettings;

    HYP_FIELD(Property = "PlatformSettings", Serialize)
    EditorPlatformSettings platformSettings;

    HYP_FIELD(Property = "EnabledPlugins", Serialize)
    Array<Name> enabledPlugins;

    HYP_FIELD(Property = "DisabledPlugins", Serialize)
    Array<Name> disabledPlugins;

    Array<Name> ResolveEnabledPlugins() const;
};

EDITOR_API String ResolveModuleName(const String& moduleName, const String& gameTitle, const FilePath& projectFilepath);

EDITOR_API String ReadProjectModuleName(const FilePath& projectFilepath);

EDITOR_API String GetProjectGameTitle(const FilePath& projectFilepath);

EDITOR_API String MakeFileNameFromTitle(const String& title);

EDITOR_API Result WriteGameArgsFile(const FilePath& directory, const Array<String>& args);

} // namespace Hyperion
