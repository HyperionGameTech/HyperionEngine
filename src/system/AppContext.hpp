/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once
#ifdef HYP_SDL
#include <SDL2/SDL_vulkan.h>
#include <SDL2/SDL.h>
#endif

#include <core/memory/UniquePtr.hpp>
#include <core/memory/RefCountedPtr.hpp>

#include <core/functional/Delegate.hpp>

#include <core/filesystem/FilePath.hpp>

#include <core/utilities/EnumFlags.hpp>

#include <core/memory/Memory.hpp>

#include <core/config/Config.hpp>

#include <core/object/HypObject.hpp>

#include <core/Defines.hpp>
#include <core/object/Handle.hpp>

#include <input/Mouse.hpp>
#include <input/InputManager.hpp>

#include <core/Types.hpp>

namespace hyperion {

class Game;

#ifdef HYP_VULKAN
class VulkanInstance;
#endif

enum class WindowFlags : uint32
{
    NONE = 0x0,
    HEADLESS = 0x1,
    NO_GFX = 0x2,
    HIGH_DPI = 0x4
};

HYP_MAKE_ENUM_FLAGS(WindowFlags)

namespace cli {

class CommandLineArguments;
struct CommandLineArgumentDefinitions;

} // namespace cli

using cli::CommandLineArgumentDefinitions;
using cli::CommandLineArguments;

extern HYP_API const CommandLineArgumentDefinitions& DefaultCommandLineArgumentDefinitions();

namespace sys {

class SystemEvent;

struct WindowOptions
{
    ANSIString title;
    Vec2i size;
    WindowFlags flags = WindowFlags::NONE;
};

HYP_CLASS(Abstract)
class HYP_API ApplicationWindow : public HypObjectBase
{
    HYP_OBJECT_BODY(ApplicationWindow);

public:
    ApplicationWindow(ANSIString title, Vec2i size);
    ApplicationWindow(const ApplicationWindow& other) = delete;
    ApplicationWindow& operator=(const ApplicationWindow& other) = delete;
    virtual ~ApplicationWindow() = default;

    HYP_FORCE_INLINE InputEventSink& GetInputEventSink()
    {
        return m_inputEventSink;
    }

    HYP_FORCE_INLINE const InputEventSink& GetInputEventSink() const
    {
        return m_inputEventSink;
    }

    virtual void SetMousePosition(Vec2i position) = 0;
    virtual Vec2i GetMousePosition() const = 0;

    virtual Vec2i GetDimensions() const = 0;
    virtual void HandleResize(Vec2i newSize);

    virtual void SetIsMouseLocked(bool locked) = 0;
    virtual bool HasMouseFocus() const = 0;

    virtual bool IsHighDPI() const
    {
        return false;
    }

    Delegate<void, Vec2i> OnWindowSizeChanged;

protected:
    ANSIString m_title;
    Vec2i m_size;
    InputEventSink m_inputEventSink;
};

HYP_CLASS()
class HYP_API SDLApplicationWindow : public ApplicationWindow
{
    HYP_OBJECT_BODY(SDLApplicationWindow);

public:
    SDLApplicationWindow(ANSIString title, Vec2i size);
    virtual ~SDLApplicationWindow() override;

    virtual void SetMousePosition(Vec2i position) override;
    virtual Vec2i GetMousePosition() const override;

    virtual Vec2i GetDimensions() const override;

    virtual void SetIsMouseLocked(bool locked) override;
    virtual bool HasMouseFocus() const override;

    virtual bool IsHighDPI() const override;

    void Initialize(WindowOptions windowOptions);

    void* GetInternalWindowHandle() const
    {
        return m_windowHandle;
    }

private:
    void* m_windowHandle = nullptr;
};

HYP_CLASS()
class HYP_API AppContextBase : public HypObjectBase
{
    HYP_OBJECT_BODY(AppContextBase);

public:
    AppContextBase(ANSIString name, const CommandLineArguments& arguments);
    virtual ~AppContextBase();

    HYP_FORCE_INLINE const ANSIString& GetAppName() const
    {
        return m_name;
    }

    HYP_FORCE_INLINE ApplicationWindow* GetMainWindow() const
    {
        return m_mainWindow.Get();
    }

    void SetMainWindow(const Handle<ApplicationWindow>& window);

    HYP_FORCE_INLINE const Handle<InputManager>& GetInputManager() const
    {
        return m_inputManager;
    }

    virtual Handle<ApplicationWindow> CreateSystemWindow(WindowOptions windowOptions) = 0;
    virtual int PollEvent(SystemEvent& event) = 0;

    virtual void UpdateConfigurationOverrides();

    Delegate<void, ApplicationWindow*> OnCurrentWindowChanged;

protected:
    Handle<ApplicationWindow> m_mainWindow;
    Handle<InputManager> m_inputManager;
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

    Handle<ApplicationWindow> CreateSystemWindow(WindowOptions windowOptions) override;

    int PollEvent(SystemEvent& event) override;
};

HYP_CLASS()
class HYP_API Win32ApplicationWindow final : public ApplicationWindow
{
    HYP_OBJECT_BODY(Win32ApplicationWindow);

public:
    Win32ApplicationWindow(ANSIString title, Vec2i size);
    ~Win32ApplicationWindow() override;

    void Initialize(WindowOptions windowOptions);

    void SetMousePosition(Vec2i position) override;
    Vec2i GetMousePosition() const override;

    Vec2i GetDimensions() const override;

    void SetIsMouseLocked(bool locked) override;
    bool HasMouseFocus() const override;

#ifdef HYP_WINDOWS
    HYP_FORCE_INLINE HWND GetHWND() const
    {
        return m_hwnd;
    }

    HYP_FORCE_INLINE HINSTANCE GetHINSTANCE() const
    {
        return m_hinst;
    }

private:
    static LRESULT __stdcall StaticWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    HWND m_hwnd = nullptr;
    HINSTANCE m_hinst = nullptr;
    bool m_mouseLocked = false;
#endif
};

HYP_CLASS()
class HYP_API Win32AppContext final : public AppContextBase
{
    HYP_OBJECT_BODY(Win32AppContext);

public:
    Win32AppContext(ANSIString name, const CommandLineArguments& arguments);
    ~Win32AppContext() override;

    Handle<ApplicationWindow> CreateSystemWindow(WindowOptions) override;

    int PollEvent(SystemEvent& event) override;
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

} // namespace hyperion

