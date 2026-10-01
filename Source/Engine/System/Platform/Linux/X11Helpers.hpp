/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

// Must be included after the X11 headers.

namespace Hyperion {

struct X11Atoms
{
    ::Atom wmProtocols = 0;
    ::Atom wmDeleteWindow = 0;
    ::Atom netWmName = 0;
    ::Atom utf8String = 0;
};

//! Opens the process-wide X11 display connection on first use. Returns nullptr if no X server is available.
::Display* X11_AcquireDisplay();

//! Closes the display connection once every acquirer has released it.
void X11_ReleaseDisplay();

const X11Atoms& X11_GetAtoms();

} // namespace Hyperion
