/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <EditorPch.hpp>

#include <Editor/EditorNativeModule.hpp>
#include <Editor/EditorGameModuleCommon.hpp>
#include <Editor/EditorProjectLayout.hpp>

#include <Core/Containers/Array.hpp>

#include <Core/Utilities/StringUtil.hpp>

#include <Core/IO/ByteWriter.hpp>
#include <Core/IO/ByteReader.hpp>

#include <Core/Logging/Logger.hpp>

#include <filesystem>

namespace Hyperion {

EDITOR_API HYP_DECLARE_LOG_CHANNEL(Editor);

namespace CoreApi {
CORE_API extern const FilePath& GetBaseDirectory();
} // namespace CoreApi

namespace {

struct GameProjectFile
{
    FilePath templatePath;
    FilePath filepath;
};

// a file as it will be once the project is renamed
struct RenamedFile
{
    FilePath currentPath;
    FilePath renamedPath;
    String text;
    String renamedText;
};

constexpr const char* ScratchDirectories[] = { "obj", "bin", ".vs", ".vscode", ".idea" };
constexpr const char* CodeExtensions[] = { "cpp", "cc", "c", "hpp", "h", "inl", "cs" };
constexpr const char* BuildExtensions[] = { "csproj", "props", "targets", "txt", "cmake", "slnx", "sln" };

Array<GameProjectFile> GetGameProjectFiles(const FilePath& projectFilepath, GameProjectLanguage language, const String& name)
{
    const FilePath sourceDir = projectFilepath.BasePath() / "Source";
    const String className = name + "Game";

    Array<GameProjectFile> files;

    if (language == GameProjectLanguage::Managed)
    {
        const FilePath templateDir = CoreApi::GetBaseDirectory() / "Source/Templates/ManagedGame";

        files.PushBack({ templateDir / "Game.csproj.in", sourceDir / (name + ".csproj") });
        files.PushBack({ templateDir / "Game.cs.in", sourceDir / (className + ".cs") });
        files.PushBack({ templateDir / "Program.cs.in", sourceDir / "Program.cs" });
        files.PushBack({ templateDir / "Game.slnx.in", projectFilepath.BasePath() / (name + ".slnx") });
    }
    else if (language == GameProjectLanguage::Native)
    {
        const FilePath templateDir = CoreApi::GetBaseDirectory() / "Source/Templates/NativeGame";

        files.PushBack({ templateDir / "CMakeLists.txt.in", sourceDir / "CMakeLists.txt" });
        files.PushBack({ templateDir / "Game" / "Game.hpp.in", sourceDir / "Game" / (className + ".hpp") });
        files.PushBack({ templateDir / "Game" / "Game.cpp.in", sourceDir / "Game" / (className + ".cpp") });
        files.PushBack({ templateDir / "Launcher" / "main.cpp.in", sourceDir / "Launcher" / "main.cpp" });
    }

    return files;
}

bool ReadTextFile(const FilePath& filepath, String& outText)
{
    if (!filepath.Exists())
    {
        return false;
    }

    FileByteReader reader { filepath };

    if (reader.Eof())
    {
        return false;
    }

    const ByteBuffer buffer = reader.Read();
    reader.Close();

    outText = String(buffer.ToByteView());

    return true;
}

bool WriteTextFile(const FilePath& filepath, const String& text)
{
    FileByteWriter writer { filepath };

    if (!writer.IsOpen())
    {
        return false;
    }

    writer.Write(text.Data(), text.Size());
    writer.Close();

    return true;
}

bool RenderGameProjectTemplate(
    const FilePath& templatePath,
    const FilePath& projectFilepath,
    const String& name,
    String& outText)
{
    String templateText;

    if (!ReadTextFile(templatePath, templateText))
    {
        return false;
    }

    String contentDir = "../../";

    if (TResult<String> contentSubfolderResult = ReadProjectContentSubfolder(projectFilepath); contentSubfolderResult.HasValue() && contentSubfolderResult.GetValue().Any())
    {
        contentDir += contentSubfolderResult.GetValue() + "/";
    }

    outText = templateText.ReplaceAll("@NAME@", name).ReplaceAll("@CLASS@", name + "Game").ReplaceAll("@CONTENT_DIR@", contentDir);

    return true;
}

bool HasExtension(const FilePath& filepath, Span<const char* const> extensions)
{
    const String extension = filepath.GetExtension().ToLower();

    for (const char* candidate : extensions)
    {
        if (extension == candidate)
        {
            return true;
        }
    }

    return false;
}

// the files under Source/, leaving out build and IDE scratch directories
Result CollectSourceFiles(const FilePath& sourceDir, Array<FilePath>& outFiles)
{
    std::error_code errorCode;

    for (std::filesystem::recursive_directory_iterator it(sourceDir.Data(), errorCode), end; !errorCode && it != end; it.increment(errorCode))
    {
        std::error_code entryErrorCode;

        if (it->is_directory(entryErrorCode))
        {
            const String directoryName = FilePath(it->path().string().c_str()).Basename();

            for (const char* scratchDirectory : ScratchDirectories)
            {
                if (directoryName == scratchDirectory)
                {
                    it.disable_recursion_pending();

                    break;
                }
            }

            continue;
        }

        outFiles.PushBack(FilePath(it->path().string().c_str()));
    }

    if (errorCode)
    {
        return HYP_MAKE_ERROR(Error, "Failed to list the files in {}: {}", sourceDir, errorCode.message().c_str());
    }

    return {};
}

void UndoRenames(const Array<RenamedFile*>& renamedFiles)
{
    for (size_t index = renamedFiles.Size(); index-- > 0;)
    {
        if (!renamedFiles[index]->renamedPath.Rename(renamedFiles[index]->currentPath))
        {
            HYP_LOG(Editor, Error, "Failed to move {} back to {} while undoing a game project rename", renamedFiles[index]->renamedPath, renamedFiles[index]->currentPath);
        }
    }
}

void UndoWrites(const Array<RenamedFile*>& writtenFiles)
{
    for (const RenamedFile* writtenFile : writtenFiles)
    {
        if (!WriteTextFile(writtenFile->renamedPath, writtenFile->text))
        {
            HYP_LOG(Editor, Error, "Failed to restore {} while undoing a game project rename", writtenFile->renamedPath);
        }
    }
}

} // namespace

EDITOR_API GameProjectLanguage GetGameProjectLanguage(const FilePath& projectFilepath)
{
    if (!(projectFilepath.BasePath() / "Source").Exists())
    {
        return GameProjectLanguage::None;
    }

    return IsManagedProject(projectFilepath) ? GameProjectLanguage::Managed : GameProjectLanguage::Native;
}

EDITOR_API bool IsGameProjectUnmodified(const FilePath& projectFilepath)
{
    const GameProjectLanguage language = GetGameProjectLanguage(projectFilepath);

    if (language == GameProjectLanguage::None)
    {
        return false;
    }

    const FilePath sourceDir = projectFilepath.BasePath() / "Source";
    const String name = GetNativeProjectName(projectFilepath);

    Array<FilePath> generatedPaths;

    for (const GameProjectFile& file : GetGameProjectFiles(projectFilepath, language, name))
    {
        String expectedText;
        String actualText;

        if (!RenderGameProjectTemplate(file.templatePath, projectFilepath, name, expectedText)
            || !ReadTextFile(file.filepath, actualText)
            || expectedText != actualText)
        {
            return false;
        }

        generatedPaths.PushBack(file.filepath.ToCanonical());
    }

    generatedPaths.PushBack((sourceDir / "Hyperion.Local.props").ToCanonical());
    generatedPaths.PushBack((sourceDir / "CMakeUserPresets.json").ToCanonical());

    Array<FilePath> sourceFiles;

    if (Result result = CollectSourceFiles(sourceDir, sourceFiles); result.HasError())
    {
        HYP_LOG(Editor, Warning, "{}", result.GetError().GetMessage());

        return false;
    }

    // anything the user added next to the generated files counts as their work too
    for (const FilePath& sourceFile : sourceFiles)
    {
        if (!generatedPaths.Contains(sourceFile.ToCanonical()))
        {
            return false;
        }
    }

    return true;
}

EDITOR_API Result GenerateGameProjectFiles(
    const FilePath& projectFilepath,
    GameProjectLanguage language,
    Span<const Name> pluginModules)
{
    if (language == GameProjectLanguage::None)
    {
        return HYP_MAKE_ERROR(Error, "No game project language given");
    }

    const String name = GetNativeProjectName(projectFilepath);

    for (const GameProjectFile& file : GetGameProjectFiles(projectFilepath, language, name))
    {
        String text;

        if (!RenderGameProjectTemplate(file.templatePath, projectFilepath, name, text))
        {
            return HYP_MAKE_ERROR(Error, "Template {} is missing", file.templatePath);
        }

        const FilePath directory = file.filepath.BasePath();

        if (!directory.Exists() && !directory.MkDir())
        {
            return HYP_MAKE_ERROR(Error, "Failed to create directory {}", directory);
        }

        if (!WriteTextFile(file.filepath, text))
        {
            return HYP_MAKE_ERROR(Error, "Failed to open {} for writing", file.filepath);
        }
    }

    if (language == GameProjectLanguage::Managed)
    {
        return WriteManagedProjectProps(projectFilepath, pluginModules);
    }

    return {};
}

EDITOR_API Result RemoveGameProjectFiles(const FilePath& projectFilepath)
{
    if (!IsGameProjectUnmodified(projectFilepath))
    {
        return HYP_MAKE_ERROR(Error, "The game project has been modified - not removing it");
    }

    const GameProjectLanguage language = GetGameProjectLanguage(projectFilepath);
    const FilePath sourceDir = projectFilepath.BasePath() / "Source";
    const String name = GetNativeProjectName(projectFilepath);

    const FilePath modulePath = (language == GameProjectLanguage::Managed)
        ? GetManagedModulePath(projectFilepath)
        : GetNativeModulePath(projectFilepath);

    if (!sourceDir.RemoveRecursively())
    {
        return HYP_MAKE_ERROR(Error, "Failed to remove {}", sourceDir);
    }

    Array<FilePath> leftoverPaths;

    for (const GameProjectFile& file : GetGameProjectFiles(projectFilepath, language, name))
    {
        if (file.filepath.Exists() && !file.filepath.Remove())
        {
            leftoverPaths.PushBack(file.filepath);
        }
    }

    if (language == GameProjectLanguage::Native)
    {
        RetireNativeModule(modulePath);
    }

    if (modulePath.Exists() && !modulePath.Remove())
    {
        leftoverPaths.PushBack(modulePath);
    }

    if (leftoverPaths.Any())
    {
        return HYP_MAKE_ERROR(Error, "Game project has been removed, but {} file(s) could not be deleted", leftoverPaths.Size());
    }

    return {};
}

EDITOR_API Result RenameGameProjectFiles(const FilePath& projectFilepath, const String& oldName, const String& newName)
{
    const GameProjectLanguage language = GetGameProjectLanguage(projectFilepath);

    if (language == GameProjectLanguage::None)
    {
        return HYP_MAKE_ERROR(Error, "{} - no game code to rename", projectFilepath);
    }

    const Array<GameProjectFile> oldFiles = GetGameProjectFiles(projectFilepath, language, oldName);
    const Array<GameProjectFile> newFiles = GetGameProjectFiles(projectFilepath, language, newName);

    for (size_t index = 0; index < oldFiles.Size(); index++)
    {
        if (oldFiles[index].filepath != newFiles[index].filepath && oldFiles[index].filepath.Exists() && newFiles[index].filepath.Exists())
        {
            return HYP_MAKE_ERROR(Error, "Can't rename {} to {}, already exists", oldFiles[index].filepath, newFiles[index].filepath);
        }
    }

    const Pair<String, String> classReplacements[] = {
        { String("g_cls") + oldName + "Game", String("g_cls") + newName + "Game" },
        { oldName + "Game", newName + "Game" }
    };

    const Pair<String, String> buildReplacements[] = {
        { oldName + "Game", newName + "Game" },
        { oldName, newName }
    };

    Array<FilePath> sourceFiles;

    if (Result result = CollectSourceFiles(projectFilepath.BasePath() / "Source", sourceFiles); result.HasError())
    {
        return result;
    }

    for (const GameProjectFile& file : oldFiles)
    {
        if (file.filepath.BasePath() == projectFilepath.BasePath() && file.filepath.Exists())
        {
            sourceFiles.PushBack(file.filepath);
        }
    }

    Array<RenamedFile> renamedFiles;

    for (const FilePath& sourceFile : sourceFiles)
    {
        RenamedFile renamedFile;
        renamedFile.currentPath = sourceFile;
        renamedFile.renamedPath = sourceFile;

        bool isGenerated = false;

        for (size_t index = 0; index < oldFiles.Size(); index++)
        {
            if (oldFiles[index].filepath.ToCanonical() == sourceFile.ToCanonical())
            {
                renamedFile.renamedPath = newFiles[index].filepath;
                isGenerated = true;

                break;
            }
        }

        Span<const Pair<String, String>> replacements;

        if (HasExtension(sourceFile, CodeExtensions))
        {
            replacements = classReplacements;
        }
        else if (isGenerated && HasExtension(sourceFile, BuildExtensions))
        {
            replacements = buildReplacements;
        }

        if (replacements.Size() != 0 && ReadTextFile(sourceFile, renamedFile.text))
        {
            renamedFile.renamedText = renamedFile.text;

            for (const Pair<String, String>& replacement : replacements)
            {
                renamedFile.renamedText = StringUtil::ReplaceIdentifier(renamedFile.renamedText, replacement.first, replacement.second);
            }
        }
        else
        {
            renamedFile.text = String();
            renamedFile.renamedText = String();
        }

        if (renamedFile.renamedPath != renamedFile.currentPath || renamedFile.renamedText != renamedFile.text)
        {
            renamedFiles.PushBack(std::move(renamedFile));
        }
    }

    Array<RenamedFile*> movedFiles;

    for (RenamedFile& renamedFile : renamedFiles)
    {
        if (renamedFile.renamedPath == renamedFile.currentPath)
        {
            continue;
        }

        if (!renamedFile.currentPath.Rename(renamedFile.renamedPath))
        {
            UndoRenames(movedFiles);

            return HYP_MAKE_ERROR(Error, "Failed to rename {} to {}", renamedFile.currentPath, renamedFile.renamedPath);
        }

        movedFiles.PushBack(&renamedFile);
    }

    Array<RenamedFile*> writtenFiles;

    for (RenamedFile& renamedFile : renamedFiles)
    {
        if (renamedFile.renamedText == renamedFile.text)
        {
            continue;
        }

        if (!WriteTextFile(renamedFile.renamedPath, renamedFile.renamedText))
        {
            UndoWrites(writtenFiles);
            UndoRenames(movedFiles);

            return HYP_MAKE_ERROR(Error, "Failed to write {}", renamedFile.renamedPath);
        }

        writtenFiles.PushBack(&renamedFile);
    }

    return {};
}

} // namespace Hyperion
