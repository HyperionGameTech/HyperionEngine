/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_APP_CONTEXT_HPP
#define HYPERION_APP_CONTEXT_HPP

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
#include <core/Handle.hpp>

#include <input/Mouse.hpp>
#include <input/InputManager.hpp>

#include <rendering/backend/Platform.hpp>

#include <Types.hpp>

namespace hyperion {

class Game;
namespace platform {

// forward declare Instance
template <PlatformType PLATFORM>
class Instance;

} // namespace platform

using Instance = platform::Instance<Platform::current>;

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
class HYP_API ApplicationWindow : public HypObject<ApplicationWindow>
{
    HYP_OBJECT_BODY(ApplicationWindow);

public:
    ApplicationWindow(ANSIString title, Vec2i size);
    ApplicationWindow(const ApplicationWindow& other) = delete;
    ApplicationWindow& operator=(const ApplicationWindow& other) = delete;
    virtual ~ApplicationWindow() = default;

    HYP_FORCE_INLINE InputEventSink& GetInputEventSink()
    {
        return m_input_event_sink;
    }

    HYP_FORCE_INLINE const InputEventSink& GetInputEventSink() const
    {
        return m_input_event_sink;
    }

    virtual void SetMousePosition(Vec2i position) = 0;
    virtual Vec2i GetMousePosition() const = 0;

    virtual Vec2i GetDimensions() const = 0;
    virtual void HandleResize(Vec2i new_size);

    virtual void SetIsMouseLocked(bool locked) = 0;
    virtual bool HasMouseFocus() const = 0;

    virtual bool IsHighDPI() const
    {
        return false;
    }

#ifdef HYP_VULKAN
    virtual VkSurfaceKHR CreateVkSurface(Instance* instance) = 0;
#endif

    Delegate<void, Vec2i> OnWindowSizeChanged;

protected:
    ANSIString m_title;
    Vec2i m_size;
    InputEventSink m_input_event_sink;
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

    void Initialize(WindowOptions);

    void* GetInternalWindowHandle() const
    {
        return m_window_handle;
    }

#ifdef HYP_VULKAN
    virtual VkSurfaceKHR CreateVkSurface(Instance* instance) override;
#endif

private:
    void* m_window_handle = nullptr;
};

HYP_CLASS()
class HYP_API AppContextBase : public HypObject<AppContextBase>
{
    HYP_OBJECT_BODY(AppContextBase);

public:
    AppContextBase(ANSIString name, const CommandLineArguments& arguments);
    virtual ~AppContextBase();

    HYP_FORCE_INLINE const ANSIString& GetAppName() const
    {
        return m_name;
    }
    
    HYP_FORCE_INLINE GlobalConfig& GetConfiguration()
    {
        return m_configuration;
    }

    HYP_FORCE_INLINE const GlobalConfig& GetConfiguration() const
    {
        return m_configuration;
    }

    HYP_FORCE_INLINE ApplicationWindow* GetMainWindow() const
    {
        return m_main_window.Get();
    }

    void SetMainWindow(const Handle<ApplicationWindow>& window);

    HYP_FORCE_INLINE const Handle<InputManager>& GetInputManager() const
    {
        return m_input_manager;
    }

    void SetGame(const Handle<Game>& game);
    const Handle<Game>& GetGame() const;

    virtual Handle<ApplicationWindow> CreateSystemWindow(WindowOptions) = 0;
    virtual int PollEvent(SystemEvent& event) = 0;

    virtual void UpdateConfigurationOverrides();

#ifdef HYP_VULKAN
    virtual bool GetVkExtensions(Array<const char*>& out_extensions) const = 0;
#endif

    Delegate<void, ApplicationWindow*> OnCurrentWindowChanged;

protected:
    Handle<ApplicationWindow> m_main_window;
    Handle<InputManager> m_input_manager;
    ANSIString m_name;
    GlobalConfig m_configuration;
    Handle<Game> m_game;
};

HYP_CLASS()
class HYP_API SDLAppContext : public AppContextBase
{
    HYP_OBJECT_BODY(SDLAppContext);

public:
    SDLAppContext(ANSIString name, const CommandLineArguments& arguments);
    virtual ~SDLAppContext() override;

    virtual Handle<ApplicationWindow> CreateSystemWindow(WindowOptions) override;

    virtual int PollEvent(SystemEvent& event) override;

#ifdef HYP_VULKAN
    virtual bool GetVkExtensions(Array<const char*>& out_extensions) const override;
#endif
};

} // namespace sys

using sys::SystemEvent;

using sys::WindowOptions;

using sys::AppContextBase;
using sys::ApplicationWindow;

using sys::SDLAppContext;
using sys::SDLApplicationWindow;

} // namespace hyperion

#endif
