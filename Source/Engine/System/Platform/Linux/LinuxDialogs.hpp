/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Core/Containers/String.hpp>
#include <Core/Containers/Array.hpp>

namespace Hyperion {

// Native dialogs on Linux are shown by running zenity (GTK) or kdialog (KDE), whichever is installed.
enum class LinuxDialogTool : uint8
{
    NONE,
    ZENITY,
    KDIALOG
};

struct LinuxDialogResult
{
    int exitCode = -1;
    String output; //!< stdout of the dialog process, trailing newline removed
};

LinuxDialogTool GetLinuxDialogTool();

//! Runs the tool directly (no shell, so arguments need no escaping) and blocks until it exits.
bool RunLinuxDialog(LinuxDialogTool tool, const Array<String>& args, LinuxDialogResult& outResult);

} // namespace Hyperion
