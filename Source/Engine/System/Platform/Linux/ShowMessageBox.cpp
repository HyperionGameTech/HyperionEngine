/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <SystemPch.hpp>

#include <System/Platform/Linux/LinuxDialogs.hpp>

#include <Core/Logging/LogChannels.hpp>
#include <Core/Logging/Logger.hpp>

#include <cstdio>

using namespace Hyperion;

static int ShowZenityMessageBox(int type, const char* title, const char* message, int buttons, const char* buttonTexts[3])
{
    Array<String> args;

    // 0-1 buttons: plain info/warning/error box. 2-3 buttons: a question box, where the
    // ok/cancel labels are the first two buttons and the third is an extra button.
    if (buttons <= 1)
    {
        args.PushBack(type == 2 ? "--error" : type == 1 ? "--warning" : "--info");
    }
    else
    {
        args.PushBack("--question");
    }

    args.PushBack("--no-markup");
    args.PushBack(String("--title=") + title);
    args.PushBack(String("--text=") + message);

    if (buttons >= 1)
    {
        args.PushBack(String("--ok-label=") + buttonTexts[0]);
    }

    if (buttons >= 2)
    {
        args.PushBack(String("--cancel-label=") + buttonTexts[1]);
    }

    if (buttons >= 3)
    {
        args.PushBack(String("--extra-button=") + buttonTexts[2]);
    }

    LinuxDialogResult result;

    if (!RunLinuxDialog(LinuxDialogTool::ZENITY, args, result))
    {
        return -1;
    }

    if (buttons == 0)
    {
        return -1;
    }

    if (result.exitCode == 0)
    {
        return 0;
    }

    // Exit code 1 is both the cancel button and the extra button; zenity prints the extra button's label.
    // Closing the window also exits with 1, with no output
    if (result.exitCode == 1 && buttons >= 2)
    {
        if (buttons >= 3 && result.output == buttonTexts[2])
        {
            return 2;
        }

        return result.output.Empty() ? 1 : -1;
    }

    return -1;
}

static int ShowKDialogMessageBox(int type, const char* title, const char* message, int buttons, const char* buttonTexts[3])
{
    Array<String> args;
    args.PushBack("--title");
    args.PushBack(title);

    if (buttons <= 1)
    {
        // kdialog has no custom label for the single button of a plain message box
        args.PushBack(type == 2 ? "--error" : "--msgbox");
        args.PushBack(message);
    }
    else
    {
        args.PushBack(buttons == 3 ? (type >= 1 ? "--warningyesnocancel" : "--yesnocancel") : (type >= 1 ? "--warningyesno" : "--yesno"));
        args.PushBack(message);
        args.PushBack("--yes-label");
        args.PushBack(buttonTexts[0]);
        args.PushBack("--no-label");
        args.PushBack(buttonTexts[1]);

        if (buttons == 3)
        {
            args.PushBack("--cancel-label");
            args.PushBack(buttonTexts[2]);
        }
    }

    LinuxDialogResult result;

    if (!RunLinuxDialog(LinuxDialogTool::KDIALOG, args, result) || buttons == 0)
    {
        return -1;
    }

    // yes = 0, no = 1, cancel = 2 (also used when the window is closed)
    if (result.exitCode >= 0 && result.exitCode < buttons)
    {
        return result.exitCode;
    }

    return -1;
}

extern "C"
{
    int ShowMessageBox(int type, const char* title, const char* message, int buttons, const char* buttonTexts[3])
    {
        if (buttons < 0)
        {
            buttons = 0;
        }
        else if (buttons > 3)
        {
            buttons = 3;
        }

        switch (GetLinuxDialogTool())
        {
        case LinuxDialogTool::ZENITY:
            return ShowZenityMessageBox(type, title, message, buttons, buttonTexts);
        case LinuxDialogTool::KDIALOG:
            return ShowKDialogMessageBox(type, title, message, buttons, buttonTexts);
        default:
            // no dialog tool available, so at least make sure the message is seen
            std::fprintf(stderr, "[%s] %s\n", title, message);
            std::fflush(stderr);

            return -1;
        }
    }
}
