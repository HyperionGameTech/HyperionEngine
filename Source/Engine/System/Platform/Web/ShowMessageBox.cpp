/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <SystemPch.hpp>

#include <emscripten/console.h>

extern "C"
{
    /// a modal dialog would have to block the calling worker on the browser thread
    /// so, the message goes to the console instead
    int ShowMessageBox(int type, const char* title, const char* message, int buttons, const char* buttonTexts[3])
    {
        emscripten_console_error(title != nullptr ? title : "");
        emscripten_console_error(message != nullptr ? message : "");

        return 0;
    }
}
