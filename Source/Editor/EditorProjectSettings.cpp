/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <EditorPch.hpp>

#include <Editor/EditorProjectSettings.hpp>

#include <Asset/SerializationUtils.hpp>

#include <Core/Reflection/BoxedValue.hpp>

#include <Core/IO/ByteReader.hpp>
#include <Core/IO/ByteWriter.hpp>

#include <Core/Threading/Mutex.hpp>

#include <Core/Utilities/StringUtil.hpp>
#include <Core/Utilities/Time.hpp>

#include <EditorProjectSettings.generated.inl>

namespace Hyperion {

namespace {

struct CachedProjectSettings
{
    FilePath projectFilepath;
    Time timestamp;
    String gameTitle;
    String moduleName;
};

static Mutex s_cachedProjectSettingsMutex;
static Array<CachedProjectSettings> s_cachedProjectSettings;

CachedProjectSettings ReadCachedProjectSettings(const FilePath& projectFilepath)
{
    const Time timestamp = projectFilepath.LastModifiedTimestamp();

    if (timestamp == Time(0))
    {
        return CachedProjectSettings { projectFilepath };
    }

    Mutex::Guard guard(s_cachedProjectSettingsMutex);

    auto it = s_cachedProjectSettings.FindIf([&projectFilepath](const CachedProjectSettings& entry)
        {
            return entry.projectFilepath == projectFilepath;
        });

    if (it != s_cachedProjectSettings.End() && it->timestamp == timestamp)
    {
        return *it;
    }

    CachedProjectSettings* cached = it != s_cachedProjectSettings.End()
        ? &*it
        : &s_cachedProjectSettings.PushBack(CachedProjectSettings { projectFilepath });

    cached->timestamp = timestamp;
    cached->gameTitle = String::empty;
    cached->moduleName = String::empty;

    FileByteReader projectStream { projectFilepath };

    TResult<BoxedValue> fieldResult = FieldFromHMF(projectStream, "ProjectSettings"_sh);

    projectStream.Close();

    if (fieldResult.HasValue() && fieldResult.GetValue().Is<Handle<EditorProjectSettings>>())
    {
        if (const Handle<EditorProjectSettings>& settings = fieldResult.GetValue().Get<Handle<EditorProjectSettings>>(); settings.IsValid())
        {
            cached->gameTitle = settings->gameTitle.Trimmed();
            cached->moduleName = settings->moduleName;
        }
    }

    return *cached;
}

} // namespace

EDITOR_API String ResolveModuleName(const String& moduleName, const String& gameTitle, const FilePath& projectFilepath)
{
    String name = StringUtil::ToIdentifier(moduleName);

    // until the game's code exists it's named after the title
    if (name.Empty() && !(projectFilepath.BasePath() / "Source").Exists())
    {
        name = StringUtil::ToIdentifier(gameTitle);
    }

    if (name.Empty())
    {
        name = StringUtil::ToIdentifier(String(FilePath(projectFilepath.Basename()).StripExtension()));
    }

    if (name.Empty())
    {
        name = "MyGame";
    }

    return name;
}

EDITOR_API String ReadProjectModuleName(const FilePath& projectFilepath)
{
    const CachedProjectSettings settings = ReadCachedProjectSettings(projectFilepath);

    return ResolveModuleName(settings.moduleName, settings.gameTitle, projectFilepath);
}

EDITOR_API String GetProjectGameTitle(const FilePath& projectFilepath)
{
    const CachedProjectSettings settings = ReadCachedProjectSettings(projectFilepath);

    if (settings.gameTitle.Any())
    {
        return settings.gameTitle;
    }

    return FilePath(projectFilepath.Basename()).StripExtension();
}

EDITOR_API String MakeFileNameFromTitle(const String& title)
{
    static const char* const s_invalidCharacters[] = { "<", ">", ":", "\"", "/", "\\", "|", "?", "*", "\t", "\r", "\n" };

    String fileName = title;

    for (const char* invalidCharacter : s_invalidCharacters)
    {
        fileName = fileName.ReplaceAll(invalidCharacter, "");
    }

    fileName = fileName.Trimmed();

    // Windows drops trailing dots from file names
    while (fileName.EndsWith("."))
    {
        fileName = String(fileName.Substr(0, fileName.Length() - 1)).Trimmed();
    }

    return fileName;
}

EDITOR_API Result WriteGameArgsFile(const FilePath& directory, const Array<String>& args)
{
    const FilePath argsFilePath = directory / "args.txt";

    FileByteWriter writer { argsFilePath };

    if (!writer.IsOpen())
    {
        return HYP_MAKE_ERROR(Error, "Failed to write {}", argsFilePath);
    }

    for (const String& arg : args)
    {
        writer.WriteString(arg + "\n");
    }

    writer.Close();

    return {};
}

} // namespace Hyperion
