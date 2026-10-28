/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Containers/String.hpp>

#include <Core/FileSystem/FilePath.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Utilities/Span.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Utilities/Result.hpp>

namespace Hyperion {

class Game;
class EditorProject;
class EditorSubsystem;

enum class GameProjectLanguage : uint32
{
    None = 0,
    Native,
    Managed,

    Max
};

inline constexpr const char* GetGameProjectLanguageName(GameProjectLanguage language)
{
    constexpr const char* Names[uint32(GameProjectLanguage::Max)] = {
        "",
        "C++",
        "C#"
    };

    return Names[uint32(language)];
}

EDITOR_API String GetNativeProjectName(const FilePath& projectFilepath);

EDITOR_API FilePath GetNativeModulePath(const FilePath& projectFilepath);

// written by the editor's game build, for rebuilding the module outside the editor
EDITOR_API FilePath GetNativeBuildScriptPath(const FilePath& projectFilepath);

// why the built game module can't load in this engine, empty if it can (or isn't built)
EDITOR_API String CheckNativeModuleEngineVersion(const FilePath& projectFilepath);

EDITOR_API TResult<Handle<Game>> CreateGameFromNativeModule(const FilePath& projectFilepath);

EDITOR_API Result ReloadGameModule(EditorSubsystem* subsystem);

EDITOR_API void UpdateGameModules(EditorSubsystem* subsystem);

// for tests, where there is no editor to control.
EDITOR_API Result ReloadGameModule(EditorProject* project);
EDITOR_API void UpdateGameModules(EditorProject* project, bool isSimulating, EditorSubsystem* subsystem = nullptr);
EDITOR_API bool HasRetiredNativeModules();

EDITOR_API FilePath GetManagedProjectFilePath(const FilePath& projectFilepath);

EDITOR_API bool IsManagedProject(const FilePath& projectFilepath);

EDITOR_API FilePath GetManagedModulePath(const FilePath& projectFilepath);

EDITOR_API Result WriteManagedProjectProps(const FilePath& projectFilepath, Span<const Name> pluginModules);

EDITOR_API TResult<Handle<Game>> CreateGameFromManagedModule(const FilePath& projectFilepath);

EDITOR_API GameProjectLanguage GetGameProjectLanguage(const FilePath& projectFilepath);

EDITOR_API bool IsGameProjectUnmodified(const FilePath& projectFilepath);

EDITOR_API Result GenerateGameProjectFiles(const FilePath& projectFilepath, GameProjectLanguage language, Span<const Name> pluginModules);

EDITOR_API Result UpgradeGameProjectFiles(const FilePath& projectFilepath);

EDITOR_API Result RemoveGameProjectFiles(const FilePath& projectFilepath);

EDITOR_API Result RenameGameProjectFiles(const FilePath& projectFilepath, const String& oldName, const String& newName);

} // namespace Hyperion
