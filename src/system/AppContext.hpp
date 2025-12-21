/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#ifdef HYP_SDL
#include <SDL2/SDL_vulkan.h>
#include <SDL2/SDL.h>
#endif

#include <core/Defines.hpp>
#include <core/Types.hpp>

#include <core/functional/ScriptableDelegate.hpp>

#include <core/utilities/EnumFlags.hpp>

#include <core/memory/Memory.hpp>

#include <core/config/Config.hpp>

#include <core/reflection/ObjectBase.hpp>
#include <core/reflection/Handle.hpp>

#include <rendering/RenderObject.hpp>

#include <input/Mouse.hpp>

#ifdef HYP_VULKAN
#include <vulkan/vulkan_core.h>
#endif

#ifdef HYP_MACOS
#include <system/platform/mac/CocoaFwd.h>
#endif

namespace hyperion {

#ifndef HYP_WINDOWS
using HWND = void*;
#endif

class Game;
class InputManager;

#ifdef HYP_VULKAN
class VulkanInstance;
class IDummyVulkanSurfaceContext;
#endif

HYP_ENUM()
enum class WindowFlags : uint32
{
    NONE = 0x0,
    HEADLESS = 0x1,
    NO_GFX = 0x2,
    HIGH_DPI = 0x4,
    EVENTS_POLLING = 0x8 //!< Window will poll for events instead of using an event callback system (e.g Win32 WindowProc)
};

HYP_MAKE_ENUM_FLAGS(WindowFlags)

namespace cli {

class CommandLineArguments;
struct CommandLineArgumentDefinitions;

} // namespace cli

using cli::CommandLineArgumentDefinitions;
using cli::CommandLineArguments;

namespace sys {

class SystemEvent;

HYP_STRUCT()
struct WindowOptions
{
    HYP_STRUCT_BODY(WindowOptions);

    char title[256];
    Vec2i dimensions;
    uint32 flags;
    HWND parentHwnd;
};

HYP_CLASS(Abstract)
class HYP_API ApplicationWindow : public ObjectBase
{
    HYP_OBJECT_BODY(ApplicationWindow);

    ApplicationWindow() = default;

public:
    ApplicationWindow(ANSIString title, Vec2i size);
    ApplicationWindow(const ApplicationWindow& other) = delete;
    ApplicationWindow& operator=(const ApplicationWindow& other) = delete;
    virtual ~ApplicationWindow();

    HYP_METHOD()
    HYP_FORCE_INLINE const Handle<InputManager>& GetInputManager() const
    {
        return m_inputManager;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE HWND GetHWND() const
    {
        return m_hwnd;
    }

    HYP_FORCE_INLINE const SwapchainRef& GetSwapchain() const
    {
        return m_swapchain;
    }

    HYP_FORCE_INLINE void SetSwapchain(const SwapchainRef& swapchain)
    {
        m_swapchain = swapchain;
    }

#ifdef HYP_VULKAN
    HYP_FORCE_INLINE VkSurfaceKHR GetVkSurface() const
    {
        return m_vkSurface;
    }
#endif

    HYP_METHOD()
    HYP_FORCE_INLINE const Vec2i& GetSize() const
    {
        Mutex::Guard guard(m_mtx);
        return m_size;
    }

    HYP_METHOD()
    virtual void SetMousePosition(Vec2i position) = 0;

    HYP_METHOD()
    virtual Vec2i GetMousePosition() const = 0;

    virtual void HandleResize(Vec2i newSize);

    HYP_METHOD()
    virtual void SetIsMouseLocked(bool locked) = 0;

    HYP_METHOD()
    virtual bool IsMouseLocked() const = 0;

    HYP_METHOD()
    virtual bool HasMouseFocus() const = 0;

    HYP_METHOD()
    virtual bool IsHighDPI() const
    {
        return false;
    }

    virtual void CreateSwapchain();

    HYP_METHOD()
    virtual Vec2i GetDimensions() const = 0;

    HYP_FIELD()
    ScriptableDelegate<void, Vec2i> OnWindowSizeChanged;

protected:
    ANSIString m_title;
    Vec2i m_size;
    Handle<InputManager> m_inputManager;
    HWND m_hwnd;
    SwapchainRef m_swapchain;

#ifdef HYP_VULKAN
    VkSurfaceKHR m_vkSurface = VK_NULL_HANDLE;
#endif

    mutable Mutex m_mtx;
};

HYP_CLASS()
class HYP_API SDLApplicationWindow final : public ApplicationWindow
{
    HYP_OBJECT_BODY(SDLApplicationWindow);

public:
    SDLApplicationWindow(ANSIString title, Vec2i size);
    ~SDLApplicationWindow() override;

    HYP_METHOD()
    void SetMousePosition(Vec2i position) override;

    HYP_METHOD()
    Vec2i GetMousePosition() const override;

    HYP_METHOD()
    Vec2i GetDimensions() const override;

    HYP_METHOD()
    void SetIsMouseLocked(bool locked) override;

    HYP_METHOD()
    bool IsMouseLocked() const override;

    HYP_METHOD()
    bool HasMouseFocus() const override;

    HYP_METHOD()
    bool IsHighDPI() const override;

    void Initialize(WindowOptions windowOptions);
};

HYP_CLASS(Abstract)
class HYP_API AppContextBase : public ObjectBase
{
    HYP_OBJECT_BODY(AppContextBase);

    AppContextBase() = default;

public:
    HYP_METHOD()
    static const Handle<AppContextBase>& GetInstance();

    AppContextBase(ANSIString name, const CommandLineArguments& arguments);
    virtual ~AppContextBase();

    HYP_METHOD()
    HYP_FORCE_INLINE const ANSIString& GetAppName() const
    {
        return m_name;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE ApplicationWindow* GetMainWindow() const
    {
        return m_mainWindow;
    }

    HYP_METHOD()
    void SetMainWindow(const Handle<ApplicationWindow>& window);

    HYP_METHOD()
    HYP_FORCE_INLINE const Array<Handle<ApplicationWindow>>& GetWindows() const
    {
        return m_windows;
    }

    HYP_METHOD()
    virtual Handle<ApplicationWindow> CreateSystemWindow(WindowOptions windowOptions) = 0;

    HYP_METHOD()
    void RemoveWindow(ApplicationWindow* window);

    virtual int PollEvents(SystemEvent& event) = 0;

    HYP_FIELD()
    ScriptableDelegate<void, ApplicationWindow*> OnCurrentWindowChanged;

protected:
    ApplicationWindow* m_mainWindow;
    Array<Handle<ApplicationWindow>> m_windows;
    ANSIString m_name;
    Handle<Game> m_game;
};

HYP_CLASS()
class HYP_API SDLAppContext final : public AppContextBase
{
    HYP_OBJECT_BODY(SDLAppContext);

public:
    SDLAppContext(ANSIString name, const CommandLineArguments& arguments);
    ~SDLAppContext() override;

    HYP_METHOD()
    Handle<ApplicationWindow> CreateSystemWindow(WindowOptions windowOptions) override;

    int PollEvents(SystemEvent& event) override;

#ifdef HYP_VULKAN
    static VkSurfaceKHR CreateVulkanSurface(
        SDLApplicationWindow* window,
        IDummyVulkanSurfaceContext** ppOutDummySurfaceContext);
#endif
};

HYP_CLASS()
class HYP_API Win32ApplicationWindow final : public ApplicationWindow
{
    HYP_OBJECT_BODY(Win32ApplicationWindow);

    friend class Win32AppContext;

public:
    Win32ApplicationWindow(ANSIString title, Vec2i size);
    ~Win32ApplicationWindow() override;

    void Initialize(WindowOptions windowOptions);

    HYP_METHOD()
    void SetMousePosition(Vec2i position) override;

    HYP_METHOD()
    Vec2i GetMousePosition() const override;

    HYP_METHOD()
    Vec2i GetDimensions() const override;

    HYP_METHOD()
    void SetIsMouseLocked(bool locked) override;

    HYP_METHOD()
    bool IsMouseLocked() const override
    {
        return m_mouseLocked;
    }

    HYP_METHOD()
    bool HasMouseFocus() const override;

#ifdef HYP_WINDOWS
    HYP_FORCE_INLINE HINSTANCE GetHINSTANCE() const
    {
        return m_hinst;
    }

    void ProcessRawInput(void* rawInput);

private:
    static LRESULT __stdcall StaticWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HINSTANCE m_hinst = nullptr;
    bool m_useWndProc = false;
    Vec2i m_virtualMousePos;
#endif

    bool m_mouseLocked = false;
};

HYP_CLASS()
class HYP_API Win32AppContext final : public AppContextBase
{
    HYP_OBJECT_BODY(Win32AppContext);

public:
    Win32AppContext(ANSIString name, const CommandLineArguments& arguments);
    ~Win32AppContext() override;

    HYP_METHOD()
    Handle<ApplicationWindow> CreateSystemWindow(WindowOptions windowOptions) override;

    int PollEvents(SystemEvent& event) override;

#ifdef HYP_VULKAN
    static VkSurfaceKHR CreateVulkanSurface(
        Win32ApplicationWindow* window,
        IDummyVulkanSurfaceContext** ppOutDummySurfaceContext);
#endif
};

HYP_CLASS()
class HYP_API CocoaApplicationWindow final : public ApplicationWindow
{
    HYP_OBJECT_BODY(CocoaApplicationWindow);

public:
    CocoaApplicationWindow(ANSIString title, Vec2i size);
    ~CocoaApplicationWindow() override;

    void Initialize(WindowOptions windowOptions);

    HYP_METHOD()
    void SetMousePosition(Vec2i position) override;

    HYP_METHOD()
    Vec2i GetMousePosition() const override;

    HYP_METHOD()
    Vec2i GetDimensions() const override;

    HYP_METHOD()
    void SetIsMouseLocked(bool locked) override;

    HYP_METHOD()
    bool IsMouseLocked() const override
    {
        return m_mouseLocked;
    }

    HYP_METHOD()
    bool HasMouseFocus() const override;

    HYP_METHOD()
    bool IsHighDPI() const override;

    HYP_METHOD()
    HYP_FORCE_INLINE void* GetNSWindow() const
    {
        return m_hwnd;
    }

    HYP_METHOD()
    HYP_FORCE_INLINE void* GetNSView() const
    {
        return m_nsView;
    }

#ifdef HYP_MACOS
    HYP_FORCE_INLINE bool IsEmbeddedView() const
    {
        return m_isEmbeddedView;
    }

    void* GetCAMetalLayer() const
    {
        return m_metalLayer;
    }

    HYP_FORCE_INLINE bool UseCocoaEvents() const
    {
        return m_useCocoaEvents;
    }

    bool HandleNSEvent(NSEvent* nsEvent, SystemEvent& event);

private:
    void* m_windowDelegate = nullptr;
    void* m_metalLayer = nullptr;
    bool m_isEmbeddedView = false;
    mutable Vec2i m_mousePosition = Vec2i::Zero();
    bool m_useCocoaEvents = false;
#endif

    void* m_nsView = nullptr;
    bool m_mouseLocked = false;
};

HYP_CLASS()
class HYP_API CocoaAppContext final : public AppContextBase
{
    HYP_OBJECT_BODY(CocoaAppContext);

public:
    CocoaAppContext(ANSIString name, const CommandLineArguments& arguments);
    ~CocoaAppContext() override;

    HYP_METHOD()
    Handle<ApplicationWindow> CreateSystemWindow(WindowOptions windowOptions) override;

    int PollEvents(SystemEvent& event) override;

#ifdef HYP_VULKAN
    static VkSurfaceKHR CreateVulkanSurface(
        CocoaApplicationWindow* window,
        IDummyVulkanSurfaceContext** ppOutDummySurfaceContext);
#endif
};

} // namespace sys

using sys::SystemEvent;

using sys::WindowOptions;

using sys::AppContextBase;
using sys::ApplicationWindow;

using sys::SDLAppContext;
using sys::SDLApplicationWindow;

using sys::Win32AppContext;
using sys::Win32ApplicationWindow;

using sys::CocoaAppContext;
using sys::CocoaApplicationWindow;

#ifdef HYP_WINDOWS
namespace sys {
HYP_API void Win32_CleanupWindowClasses();
}
#endif

#include <system/helpers/AppContextHelpers.inl>

} // namespace hyperion
