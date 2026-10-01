/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <SystemPch.hpp>

#include <System/Platform/Linux/LinuxDialogs.hpp>

#include <Core/Logging/LogChannels.hpp>
#include <Core/Logging/Logger.hpp>

#include <cerrno>
#include <cstdlib>
#include <cstring>

#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace Hyperion {

static bool IsExecutableInPath(const char* name)
{
    const char* pathEnv = std::getenv("PATH");

    if (pathEnv == nullptr)
    {
        return false;
    }

    const char* segmentStart = pathEnv;

    while (true)
    {
        const char* segmentEnd = std::strchr(segmentStart, ':');
        const size_t segmentLength = segmentEnd != nullptr ? size_t(segmentEnd - segmentStart) : std::strlen(segmentStart);

        if (segmentLength != 0)
        {
            String candidate(segmentStart, segmentStart + segmentLength);
            candidate += "/";
            candidate += name;

            if (access(candidate.Data(), X_OK) == 0)
            {
                return true;
            }
        }

        if (segmentEnd == nullptr)
        {
            return false;
        }

        segmentStart = segmentEnd + 1;
    }
}

static const char* GetLinuxDialogToolName(LinuxDialogTool tool)
{
    switch (tool)
    {
    case LinuxDialogTool::ZENITY:
        return "zenity";
    case LinuxDialogTool::KDIALOG:
        return "kdialog";
    default:
        return nullptr;
    }
}

LinuxDialogTool GetLinuxDialogTool()
{
    static const LinuxDialogTool s_tool = []()
    {
        // prefer kdialog on KDE so dialogs match the desktop
        const char* desktop = std::getenv("XDG_CURRENT_DESKTOP");
        const bool isKde = desktop != nullptr && std::strstr(desktop, "KDE") != nullptr;

        if (isKde && IsExecutableInPath("kdialog"))
        {
            return LinuxDialogTool::KDIALOG;
        }

        if (IsExecutableInPath("zenity"))
        {
            return LinuxDialogTool::ZENITY;
        }

        if (IsExecutableInPath("kdialog"))
        {
            return LinuxDialogTool::KDIALOG;
        }

        HYP_LOG(Core, Warning, "Neither zenity nor kdialog is installed - message boxes and file dialogs are unavailable");

        return LinuxDialogTool::NONE;
    }();

    return s_tool;
}

bool RunLinuxDialog(LinuxDialogTool tool, const Array<String>& args, LinuxDialogResult& outResult)
{
    outResult = LinuxDialogResult {};

    const char* toolName = GetLinuxDialogToolName(tool);

    if (toolName == nullptr)
    {
        return false;
    }

    Array<char*> argv;
    argv.PushBack(const_cast<char*>(toolName));

    for (const String& arg : args)
    {
        argv.PushBack(const_cast<char*>(arg.Data()));
    }

    argv.PushBack(nullptr);

    int pipeFds[2];

    if (pipe(pipeFds) != 0)
    {
        HYP_LOG(Core, Error, "Failed to create pipe for {}: {}", toolName, std::strerror(errno));

        return false;
    }

    posix_spawn_file_actions_t fileActions;
    posix_spawn_file_actions_init(&fileActions);
    posix_spawn_file_actions_addclose(&fileActions, pipeFds[0]);
    posix_spawn_file_actions_adddup2(&fileActions, pipeFds[1], STDOUT_FILENO);
    posix_spawn_file_actions_addclose(&fileActions, pipeFds[1]);
    // GTK/Qt print noise on stderr that would otherwise end up in our log output
    posix_spawn_file_actions_addopen(&fileActions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);

    pid_t pid = 0;
    const int spawnResult = posix_spawnp(&pid, toolName, &fileActions, nullptr, argv.Data(), environ);

    posix_spawn_file_actions_destroy(&fileActions);
    close(pipeFds[1]);

    if (spawnResult != 0)
    {
        close(pipeFds[0]);

        HYP_LOG(Core, Error, "Failed to launch {}: {}", toolName, std::strerror(spawnResult));

        return false;
    }

    char buffer[4096];

    while (true)
    {
        const ssize_t numRead = read(pipeFds[0], buffer, sizeof(buffer));

        if (numRead > 0)
        {
            outResult.output.Append(buffer, buffer + numRead);

            continue;
        }

        if (numRead < 0 && errno == EINTR)
        {
            continue;
        }

        break;
    }

    close(pipeFds[0]);

    int status = 0;

    while (waitpid(pid, &status, 0) < 0)
    {
        if (errno != EINTR)
        {
            return false;
        }
    }

    outResult.exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

    while (outResult.output.Any() && (outResult.output.Back() == '\n' || outResult.output.Back() == '\r'))
    {
        outResult.output.PopBack();
    }

    return true;
}

} // namespace Hyperion
