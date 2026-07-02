/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#ifdef HYP_ANDROID
#include <android/looper.h>
#endif

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <Scripting/ScriptableDelegate.hpp>

#include <Core/Utilities/EnumFlags.hpp>

#include <Core/Memory/Memory.hpp>

#include <Core/Containers/FixedArray.hpp>

#include <Core/Config/Config.hpp>

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Rendering/RenderTypes.hpp>

#include <Input/Mouse.hpp>

#if HYP_VULKAN
#include <Vulkan/vulkan_core.h>
#endif

#ifdef HYP_MACOS
#include <System/Platform/Mac/CocoaFwd.h>
#endif

namespace Hyperion {

#ifndef HYP_WINDOWS
using HWND = void*;
#endif

class Game;
class InputManager;

#if HYP_VULKAN
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

class Event;

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
class ENGINE_API ApplicationWindow : public ObjectBase
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

    Swapchain* GetSwapchain() const;
    void SetSwapchain(const SwapchainRef& swapchain);

#if HYP_VULKAN
    HYP_FORCE_INLINE VkSurfaceKHR GetVkSurface() const
    {
        return m_vkSurface;
    }
#endif

    HYP_METHOD()
    HYP_FORCE_INLINE Vec2i GetSize() const
    {
        TSharedLock lock(m_mtx);
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
    HYP_FORCE_INLINE bool HasFocus() const
    {
        return m_hasFocus.Get(MemoryOrder::RELAXED);
    }

    HYP_FORCE_INLINE void SetHasFocus(bool hasFocus)
    {
        m_hasFocus.Set(hasFocus, MemoryOrder::RELAXED);
    }

    HYP_METHOD()
    virtual bool IsHighDPI() const
    {
        return false;
    }

    HYP_METHOD()
    virtual float GetContentScaleFactor() const
    {
        return 1.0f;
    }

    HYP_METHOD()
    virtual float GetRenderTargetScale() const
    {
        return IsHighDPI() ? 0.7f : 1.0f;
    }

    HYP_METHOD()
    Vec2i GetRenderSize() const
    {
        TSharedLock lock(m_mtx);
        return Vec2i(Vec2f(m_size) * GetRenderTargetScale());
    }

    virtual void CreateSwapchain();

    HYP_METHOD()
    virtual Vec2i GetDimensions() const = 0;

    HYP_METHOD()
    virtual void Close() = 0;

    HYP_FIELD()
    static ScriptableDelegate<void, Vec2i> OnWindowSizeChanged;

    HYP_FIELD()
    static ScriptableDelegate<void> OnClose;

protected:
    ANSIString m_title;
    Vec2i m_size;
    Handle<InputManager> m_inputManager;
    HWND m_hwnd;
    SwapchainRef m_swapchain;
    AtomicVar<bool> m_hasFocus { true };

#if HYP_VULKAN
    VkSurfaceKHR m_vkSurface = VK_NULL_HANDLE;
#endif

    SharedMutex m_mtx;
};

HYP_CLASS(Abstract)
class ENGINE_API AppContextBase : public ObjectBase
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

    HYP_METHOD()
    Result RunCommandlet(ANSIStringView commandletName, const CommandLineArguments& args);

    const Class* FindCommandletClass(ANSIStringView commandletName);

    virtual int PollEvents(Event& event) = 0;

    HYP_FIELD()
    static ScriptableDelegate<void, ApplicationWindow*> OnCurrentWindowChanged;

protected:
    ApplicationWindow* m_mainWindow;
    Array<Handle<ApplicationWindow>> m_windows;
    ANSIString m_name;
    Handle<Game> m_game;
};

#ifdef HYP_WINDOWS

HYP_CLASS(Condition = "HYP_WINDOWS")
class ENGINE_API Win32ApplicationWindow final : public ApplicationWindow
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

    HYP_METHOD()
    void Close() override;

    HYP_FORCE_INLINE HINSTANCE GetHINSTANCE() const
    {
        return m_hinst;
    }

    void ProcessRawInput(void* rawInput);

private:
    static LRESULT __stdcall StaticWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT __stdcall ParentSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
    LRESULT WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HINSTANCE m_hinst = nullptr;
    HWND m_parentHwnd = nullptr;
    bool m_useWndProc = false;
    bool m_mouseLocked = false;
    bool m_isOpen = false;

    Vec2f m_mousePosition;
};

HYP_CLASS(Condition = "HYP_WINDOWS")
class ENGINE_API Win32AppContext final : public AppContextBase
{
    HYP_OBJECT_BODY(Win32AppContext);

public:
    Win32AppContext(ANSIString name, const CommandLineArguments& arguments);
    ~Win32AppContext() override;

    HYP_METHOD()
    Handle<ApplicationWindow> CreateSystemWindow(WindowOptions windowOptions) override;

    int PollEvents(Event& event) override;

#if HYP_VULKAN
    static VkSurfaceKHR CreateVulkanSurface(
        Win32ApplicationWindow* window,
        IDummyVulkanSurfaceContext** ppOutDummySurfaceContext);
#endif
};

#endif // HYP_WINDOWS

#ifdef HYP_MACOS

HYP_CLASS(Condition = "HYP_MACOS")
class ENGINE_API CocoaApplicationWindow final : public ApplicationWindow
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
    float GetContentScaleFactor() const override;

    HYP_METHOD()
    float GetRenderTargetScale() const override;

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

    HYP_METHOD()
    void Close() override;

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

    bool HandleNSEvent(NSEvent* nsEvent, Event& event);

private:
    void* m_windowDelegate = nullptr;
    void* m_metalLayer = nullptr;
    bool m_isEmbeddedView = false;
    mutable Vec2i m_mousePosition = Vec2i::Zero();
    bool m_useCocoaEvents = false;

    void* m_nsView = nullptr;
    bool m_mouseLocked = false;
};

HYP_CLASS(Condition = "HYP_MACOS")
class ENGINE_API CocoaAppContext final : public AppContextBase
{
    HYP_OBJECT_BODY(CocoaAppContext);

public:
    CocoaAppContext(ANSIString name, const CommandLineArguments& arguments);
    ~CocoaAppContext() override;

    HYP_METHOD()
    Handle<ApplicationWindow> CreateSystemWindow(WindowOptions windowOptions) override;

    int PollEvents(Event& event) override;

#if HYP_VULKAN
    static VkSurfaceKHR CreateVulkanSurface(
        CocoaApplicationWindow* window,
        IDummyVulkanSurfaceContext** ppOutDummySurfaceContext);
#endif
};

#endif // HYP_MACOS

#ifdef HYP_ANDROID

HYP_CLASS(Condition = "HYP_ANDROID")
class ENGINE_API AndroidApplicationWindow final : public ApplicationWindow
{
    HYP_OBJECT_BODY(AndroidApplicationWindow);

public:
    AndroidApplicationWindow(ANSIString title, Vec2i size);
    ~AndroidApplicationWindow() override;

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
    float GetContentScaleFactor() const override;

    HYP_METHOD()
    float GetRenderTargetScale() const override;

    HYP_METHOD()
    void Close() override;

    HYP_FORCE_INLINE void* GetNativeWindow() const
    {
        return m_nativeWindow;
    }

    void SetNativeWindow(void* nativeWindow);

    bool HandleInputEvent(int32 type, int32 action, float x, float y, int32 intParam, Event& outEvent);

private:
    void* m_nativeWindow = nullptr;
    bool m_mouseLocked = false;

    Vec2f m_touchPosition;
    Vec2f m_prevTouchPosition;

    // Track previous touch positions per pointer for multi-touch support
    FixedArray<Vec2f, 10> m_touchPrevPositions {};
};

HYP_CLASS(Condition = "HYP_ANDROID")
class ENGINE_API AndroidAppContext final : public AppContextBase
{
    HYP_OBJECT_BODY(AndroidAppContext);

public:
    AndroidAppContext(ANSIString name, const CommandLineArguments& arguments);
    ~AndroidAppContext() override;

    HYP_METHOD()
    Handle<ApplicationWindow> CreateSystemWindow(WindowOptions windowOptions) override;

    int PollEvents(Event& event) override;

    void SetNativeWindow(void* nativeWindow);
    void EnqueueEvent(Event&& event);

#if HYP_VULKAN
    static VkSurfaceKHR CreateVulkanSurface(
        AndroidApplicationWindow* window,
        IDummyVulkanSurfaceContext** ppOutDummySurfaceContext);
#endif

private:
    class AndroidLooperThread* m_looperThread;
};

#endif // HYP_ANDROID

#ifdef HYP_WINDOWS
ENGINE_API void Win32_CleanupWindowClasses();
#endif

#include <System/Helpers/AppContextHelpers.inc>

} // namespace Hyperion
