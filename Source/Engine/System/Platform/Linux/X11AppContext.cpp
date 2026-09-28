/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <SystemPch.hpp>

#include <System/AppContext.hpp>

#include <Input/Event.hpp>

#include <Core/Logging/LogChannels.hpp>
#include <Core/Logging/Logger.hpp>

#include <Rendering/RenderInterface.hpp>

#if HYP_VULKAN
#include <vulkan/vulkan.h>

#include <Rendering/Vulkan/VulkanInstance.hpp>
#include <Rendering/Vulkan/VulkanRenderInterface.hpp>
#endif

#include <Framework/Threads/MainThread.hpp>

#include <cstdlib>

// X11 defines macros like None, Bool and Status, so it has to come after all engine headers
#include <X11/Xlib.h>
#include <X11/XKBlib.h>

#if HYP_VULKAN
#include <vulkan/vulkan_xlib.h>
#endif

#include <System/Platform/Linux/X11Helpers.hpp>

namespace Hyperion {

namespace {

struct X11DisplayConnection
{
    Mutex mutex;
    ::Display* display = nullptr;
    uint32 refCount = 0;
    X11Atoms atoms;
    XErrorHandler previousErrorHandler = nullptr;

    static X11DisplayConnection& GetInstance()
    {
        static X11DisplayConnection s_instance;
        return s_instance;
    }
};

// The default Xlib error handler exits the process. Errors on our connection are logged instead,
// errors on other connections in the process (e.g the editor UI's) go to whichever handler was installed before us.
int HandleX11Error(::Display* display, XErrorEvent* errorEvent)
{
    X11DisplayConnection& connection = X11DisplayConnection::GetInstance();

    if (display != connection.display && connection.previousErrorHandler != nullptr)
    {
        return connection.previousErrorHandler(display, errorEvent);
    }

    char errorText[256] = {};
    XGetErrorText(display, errorEvent->error_code, errorText, sizeof(errorText));

    HYP_LOG(Core, Warning, "X11 error: {} (request code: {}, minor code: {}, resource id: {})",
        static_cast<const char*>(errorText),
        uint32(errorEvent->request_code),
        uint32(errorEvent->minor_code),
        uint64(errorEvent->resourceid));

    return 0;
}

} // namespace

::Display* X11_AcquireDisplay()
{
    X11DisplayConnection& connection = X11DisplayConnection::GetInstance();

    Mutex::Guard guard(connection.mutex);

    if (connection.refCount == 0)
    {
        // the renderer can touch the display from the render thread (surface creation, WSI)
        XInitThreads();

        ::Display* display = XOpenDisplay(nullptr);

        if (display == nullptr)
        {
            const char* displayName = std::getenv("DISPLAY");

            HYP_LOG(Core, Error, "Failed to open X11 display (DISPLAY={})", displayName != nullptr ? displayName : "<unset>");

            return nullptr;
        }

        connection.display = display;

        XErrorHandler previousErrorHandler = XSetErrorHandler(&HandleX11Error);

        if (previousErrorHandler != &HandleX11Error)
        {
            connection.previousErrorHandler = previousErrorHandler;
        }

        // Without this, held keys produce KeyRelease/KeyPress pairs instead of repeated KeyPress events
        Bool detectableAutoRepeatSupported = False;
        XkbSetDetectableAutoRepeat(display, True, &detectableAutoRepeatSupported);

        if (!detectableAutoRepeatSupported)
        {
            HYP_LOG(Core, Warning, "X11 server does not support detectable auto repeat - held keys will generate key up events");
        }

        connection.atoms.wmProtocols = XInternAtom(display, "WM_PROTOCOLS", False);
        connection.atoms.wmDeleteWindow = XInternAtom(display, "WM_DELETE_WINDOW", False);
        connection.atoms.netWmName = XInternAtom(display, "_NET_WM_NAME", False);
        connection.atoms.utf8String = XInternAtom(display, "UTF8_STRING", False);
    }

    ++connection.refCount;

    return connection.display;
}

void X11_ReleaseDisplay()
{
    X11DisplayConnection& connection = X11DisplayConnection::GetInstance();

    Mutex::Guard guard(connection.mutex);

    AssertDebug(connection.refCount != 0);

    if (connection.refCount == 0 || --connection.refCount != 0)
    {
        return;
    }

    ::Display* display = connection.display;
    connection.display = nullptr;

    // Our error handler stays installed; with no display of our own it just forwards to the previous one.
    XCloseDisplay(display);
}

const X11Atoms& X11_GetAtoms()
{
    return X11DisplayConnection::GetInstance().atoms;
}

X11AppContext::X11AppContext(ANSIString name, const CommandLineArguments& arguments)
    : AppContextBase(std::move(name), arguments),
      m_display(X11_AcquireDisplay())
{
}

X11AppContext::~X11AppContext()
{
    if (m_display != nullptr)
    {
        X11_ReleaseDisplay();
        m_display = nullptr;
    }
}

Handle<ApplicationWindow> X11AppContext::CreateSystemWindow(WindowOptions windowOptions)
{
    if (m_display == nullptr)
    {
        HYP_FAIL("Cannot create window: no X11 display connection. Make sure an X server is running and DISPLAY is set.");
    }

    Handle<X11ApplicationWindow> window = MakeHandle<X11ApplicationWindow>(windowOptions.title, windowOptions.dimensions);
    m_windows.PushBack(window);

    window->Initialize(windowOptions);

    return window;
}

X11ApplicationWindow* X11AppContext::FindWindowForX11Event(uint64 x11Window) const
{
    if (x11Window == 0)
    {
        return nullptr;
    }

    for (const Handle<ApplicationWindow>& window : m_windows)
    {
        AssertDebug(window->IsA(X11ApplicationWindow::StaticClass()));

        X11ApplicationWindow* candidate = static_cast<X11ApplicationWindow*>(window.Get());

        if (candidate->GetX11Window() == x11Window)
        {
            return candidate;
        }
    }

    return nullptr;
}

int X11AppContext::PollEvents(Event& event)
{
    HYP_SCOPE;
    AssertOnThread(g_mainThread);

    PurgeClosedWindows();

    event = Event();

    ::Display* display = static_cast<::Display*>(m_display);

    if (display == nullptr)
    {
        return 0;
    }

    // XPending flushes the output buffer and reads anything available without blocking
    while (XPending(display) > 0)
    {
        XEvent xEvent;
        XNextEvent(display, &xEvent);

        X11ApplicationWindow* window = FindWindowForX11Event(uint64(xEvent.xany.window));

        if (window == nullptr)
        {
            continue;
        }

        if (window->HandleX11Event(&xEvent, event) && event.GetType() != EventType::INVALID)
        {
            return 1;
        }

        event = Event();
    }

    return 0;
}

#if HYP_VULKAN

VkSurfaceKHR X11AppContext::CreateVulkanSurface(
    X11ApplicationWindow* window,
    IDummyVulkanSurfaceContext** ppOutDummySurfaceContext)
{
    VkSurfaceKHR surface = VK_NULL_HANDLE;

    VkXlibSurfaceCreateInfoKHR createInfo { VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR };

    if (window != nullptr)
    {
        if (window->GetVkSurface() != VK_NULL_HANDLE)
        {
            return window->GetVkSurface();
        }

        createInfo.dpy = static_cast<::Display*>(window->GetX11Display());
        createInfo.window = ::Window(window->GetX11Window());
    }
    else
    {
        if (!ppOutDummySurfaceContext)
        {
            return VK_NULL_HANDLE;
        }

        class X11DummyVulkanSurfaceContext : public IDummyVulkanSurfaceContext
        {
        public:
            X11DummyVulkanSurfaceContext(::Display* display, ::Window window, ::Colormap colormap)
                : m_display(display),
                  m_window(window),
                  m_colormap(colormap)
            {
            }

            virtual ~X11DummyVulkanSurfaceContext() override
            {
                XDestroyWindow(m_display, m_window);
                XFreeColormap(m_display, m_colormap);
                XFlush(m_display);

                X11_ReleaseDisplay();
            }

        private:
            ::Display* m_display;
            ::Window m_window;
            ::Colormap m_colormap;
        };

        ::Display* display = X11_AcquireDisplay();

        if (display == nullptr)
        {
            HYP_FAIL("Cannot create Vulkan surface: no X11 display connection. Make sure an X server is running and DISPLAY is set.");
        }

        const int screen = DefaultScreen(display);
        const ::Window rootWindow = RootWindow(display, screen);
        ::Visual* visual = DefaultVisual(display, screen);

        XSetWindowAttributes attributes {};
        attributes.colormap = XCreateColormap(display, rootWindow, visual, AllocNone);
        attributes.border_pixel = 0;

        // never mapped - only needed to query presentation support
        const ::Window dummyWindow = XCreateWindow(
            display, rootWindow,
            0, 0, 64, 64, 0,
            DefaultDepth(display, screen), InputOutput, visual,
            CWColormap | CWBorderPixel, &attributes);

        XFlush(display);

        createInfo.dpy = display;
        createInfo.window = dummyWindow;

        *ppOutDummySurfaceContext = new X11DummyVulkanSurfaceContext(display, dummyWindow, attributes.colormap);
    }

    VkResult vkResult = vkCreateXlibSurfaceKHR(
        RI.GetInstance()->GetInstance(),
        &createInfo,
        nullptr,
        &surface);

    Assert(vkResult == VK_SUCCESS, "Failed to create Xlib Vulkan surface: {}", int(vkResult));

    return surface;
}

#endif // HYP_VULKAN

} // namespace Hyperion
