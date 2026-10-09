/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <SystemPch.hpp>

#include <System/OpenFileDialog.hpp>
#include <System/SaveFileDialog.hpp>
#include <System/SelectFolderDialog.hpp>

#include <System/Platform/Linux/LinuxDialogs.hpp>

namespace Hyperion {

enum class LinuxFileDialogMode : uint8
{
    OPEN,
    OPEN_MULTIPLE,
    SAVE,
    SELECT_FOLDER
};

static String GetStartPath(const FilePath& baseDir)
{
    String startPath = baseDir;

    // with a trailing slash the dialogs open inside the directory instead of preselecting it
    if (startPath.Any() && !startPath.EndsWith("/"))
    {
        startPath += "/";
    }

    return startPath;
}

static String BuildPatternList(Span<const ANSIStringView> extensions)
{
    String patterns;

    for (const ANSIStringView& extension : extensions)
    {
        if (patterns.Any())
        {
            patterns += " ";
        }

        patterns += "*.";
        patterns.Append(extension.Data(), extension.Data() + extension.Size());
    }

    return patterns;
}

static Array<String> BuildZenityArgs(LinuxFileDialogMode mode, UTF8StringView title, const FilePath& baseDir, Span<const ANSIStringView> extensions)
{
    Array<String> args;
    args.PushBack("--file-selection");
    args.PushBack(String("--title=") + String(title));

    const String startPath = GetStartPath(baseDir);

    if (startPath.Any())
    {
        args.PushBack(String("--filename=") + startPath);
    }

    switch (mode)
    {
    case LinuxFileDialogMode::OPEN_MULTIPLE:
        args.PushBack("--multiple");
        args.PushBack("--separator=\n");
        break;
    case LinuxFileDialogMode::SAVE:
        args.PushBack("--save");
        break;
    case LinuxFileDialogMode::SELECT_FOLDER:
        args.PushBack("--directory");
        break;
    default:
        break;
    }

    if (mode != LinuxFileDialogMode::SELECT_FOLDER)
    {
        const String patterns = BuildPatternList(extensions);

        if (patterns.Any())
        {
            args.PushBack(String("--file-filter=Supported Files (") + patterns + ") | " + patterns);
        }

        args.PushBack("--file-filter=All Files | *");
    }

    return args;
}

static Array<String> BuildKDialogArgs(LinuxFileDialogMode mode, UTF8StringView title, const FilePath& baseDir, Span<const ANSIStringView> extensions)
{
    Array<String> args;
    args.PushBack("--title");
    args.PushBack(String(title));

    switch (mode)
    {
    case LinuxFileDialogMode::OPEN:
    case LinuxFileDialogMode::OPEN_MULTIPLE:
        args.PushBack("--getopenfilename");
        break;
    case LinuxFileDialogMode::SAVE:
        args.PushBack("--getsavefilename");
        break;
    case LinuxFileDialogMode::SELECT_FOLDER:
        args.PushBack("--getexistingdirectory");
        break;
    }

    const String startPath = GetStartPath(baseDir);
    args.PushBack(startPath.Any() ? startPath : String("."));

    if (mode != LinuxFileDialogMode::SELECT_FOLDER)
    {
        // kdialog filter format: "<patterns>|<description>", one filter per line
        const String patterns = BuildPatternList(extensions);

        String filter;

        if (patterns.Any())
        {
            filter = patterns + "|Supported Files (" + patterns + ")\n";
        }

        filter += "*|All Files";

        args.PushBack(filter);
    }

    if (mode == LinuxFileDialogMode::OPEN_MULTIPLE)
    {
        args.PushBack("--multiple");
        args.PushBack("--separate-output");
    }

    return args;
}

static TResult<Array<FilePath>> RunFileDialog(LinuxFileDialogMode mode, UTF8StringView title, const FilePath& baseDir, Span<const ANSIStringView> extensions)
{
    const LinuxDialogTool tool = GetLinuxDialogTool();

    Array<String> args;

    switch (tool)
    {
    case LinuxDialogTool::ZENITY:
        args = BuildZenityArgs(mode, title, baseDir, extensions);
        break;
    case LinuxDialogTool::KDIALOG:
        args = BuildKDialogArgs(mode, title, baseDir, extensions);
        break;
    default:
        return HYP_MAKE_ERROR(Error, "No file dialog available - install zenity or kdialog");
    }

    LinuxDialogResult result;

    if (!RunLinuxDialog(tool, args, result))
    {
        return HYP_MAKE_ERROR(Error, "Failed to launch the file dialog");
    }

    // both tools exit with 1 when the dialog is cancelled or closed
    if (result.exitCode == 1)
    {
        return HYP_MAKE_ERROR(Error, "Operation cancelled by user");
    }

    if (result.exitCode != 0)
    {
        return HYP_MAKE_ERROR(Error, "File dialog failed (exit code: {})", result.exitCode);
    }

    Array<FilePath> paths;

    for (String& line : result.output.Split('\n'))
    {
        if (line.Any())
        {
            paths.PushBack(FilePath(std::move(line)));
        }
    }

    if (paths.Empty())
    {
        return HYP_MAKE_ERROR(Error, "Operation cancelled by user");
    }

    return paths;
}

void ShowOpenFileDialog(
    UTF8StringView title,
    const FilePath& baseDir,
    Span<const ANSIStringView> extensions,
    bool allowMultiple,
    bool allowDirectories,
    Proc<void(TResult<Array<FilePath>>&& result)>&& callback)
{
    // Neither tool can pick files and directories in the same dialog, so directories are only offered when no file types are requested
    const LinuxFileDialogMode mode = allowDirectories && extensions.Size() == 0
        ? LinuxFileDialogMode::SELECT_FOLDER
        : allowMultiple ? LinuxFileDialogMode::OPEN_MULTIPLE
                        : LinuxFileDialogMode::OPEN;

    TResult<Array<FilePath>> result = RunFileDialog(mode, title, baseDir, extensions);

    if (callback)
    {
        callback(std::move(result));
    }
}

void ShowSaveFileDialog(
    UTF8StringView title,
    const FilePath& baseDir,
    Span<const ANSIStringView> extensions,
    Proc<void(TResult<FilePath>&& result)>&& callback)
{
    TResult<Array<FilePath>> result = RunFileDialog(LinuxFileDialogMode::SAVE, title, baseDir, extensions);

    if (!callback)
    {
        return;
    }

    if (result.HasError())
    {
        callback(TResult<FilePath>(result.GetError()));

        return;
    }

    callback(TResult<FilePath>(std::move(result.GetValue()[0])));
}

void ShowSelectFolderDialog(
    UTF8StringView title,
    const FilePath& baseDir,
    Proc<void(TResult<FilePath>&& result)>&& callback)
{
    TResult<Array<FilePath>> result = RunFileDialog(LinuxFileDialogMode::SELECT_FOLDER, title, baseDir, {});

    if (!callback)
    {
        return;
    }

    if (result.HasError())
    {
        callback(TResult<FilePath>(result.GetError()));

        return;
    }

    callback(TResult<FilePath>(std::move(result.GetValue()[0])));
}

} // namespace Hyperion
