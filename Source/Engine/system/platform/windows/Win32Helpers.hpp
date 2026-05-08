/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

namespace Hyperion {

class Win32ApplicationWindow;
class Event;

bool HandleWindowEvent(
    Win32ApplicationWindow* window, Event& event,
    HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

} // namespace Hyperion