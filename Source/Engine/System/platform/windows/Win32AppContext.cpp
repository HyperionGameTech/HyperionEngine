/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <SystemPch.hpp>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <System/AppContext.hpp>
#include <System/platform/windows/Win32Helpers.hpp>

#include <Input/Event.hpp>

#include <Rendering/RenderInterface.hpp>

#if HYP_VULKAN
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>

#include <Rendering/vulkan/VulkanInstance.hpp>
#endif

#include <Framework/threads/MainThread.hpp>

namespace Hyperion {

extern void Win32_RegisterWindowClass(const WideString& className);
extern void Win32_UnregisterWindowClass(const WideString& className);
extern void Win32_CleanupWindowClasses();

Win32AppContext::Win32AppContext(ANSIString name, const CommandLineArguments& arguments)
    : AppContextBase(std::move(name), arguments)
{
}

Win32AppContext::~Win32AppContext() = default;

Handle<ApplicationWindow> Win32AppContext::CreateSystemWindow(WindowOptions windowOptions)
{
    Handle<Win32ApplicationWindow> window = MakeHandle<Win32ApplicationWindow>(windowOptions.title, windowOptions.dimensions);
    m_windows.PushBack(window);

    window->Initialize(windowOptions);

    return window;
}

int Win32AppContext::PollEvents(Event& event)
{
    AssertOnThread(g_mainThread);

    event = Event();

    MSG msg {};

    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        ObjId<Win32ApplicationWindow> windowId;
        windowId.value = static_cast<decltype(ObjId<Win32ApplicationWindow>::value)>(GetWindowLongPtrW(msg.hwnd, GWLP_USERDATA));

        Handle<Win32ApplicationWindow> windowHandle { windowId };

        TranslateMessage(&msg);
        DispatchMessageW(&msg);

        if (windowHandle.IsValid() && !windowHandle->m_useWndProc)
        {
            if (HandleWindowEvent(windowHandle.Get(), event, msg.hwnd, msg.message, msg.wParam, msg.lParam))
            {
                const EventType eventType = event.GetType();

                return (eventType != EventType::INVALID) ? 1 : 0;
            }
        }
    }

    return 0;
}

#if HYP_VULKAN

VkSurfaceKHR Win32AppContext::CreateVulkanSurface(
    Win32ApplicationWindow* window,
    IDummyVulkanSurfaceContext** ppOutDummySurfaceContext)
{
    VkSurfaceKHR surface = VK_NULL_HANDLE;

    static constexpr const wchar_t* DummyClassName = L"DummyWindowClass";

    VkWin32SurfaceCreateInfoKHR createInfo { VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };

    if (window != nullptr)
    {
        Win32ApplicationWindow* win32Window = DynamicCast<Win32ApplicationWindow>(window);
        Assert(win32Window != nullptr);

        if (win32Window->GetVkSurface() != VK_NULL_HANDLE)
        {
            return win32Window->GetVkSurface();
        }

        createInfo.hinstance = win32Window->GetHINSTANCE();
        createInfo.hwnd = win32Window->GetHWND();
    }
    else
    {
        if (!ppOutDummySurfaceContext)
        {
            return VK_NULL_HANDLE;
        }

        class Win32DummyVulkanSurfaceContext : public IDummyVulkanSurfaceContext
        {
        public:
            Win32DummyVulkanSurfaceContext(HINSTANCE hInstance, HWND hwnd)
                : m_hInstance(hInstance),
                  m_hwnd(hwnd)
            {
            }

            virtual ~Win32DummyVulkanSurfaceContext() override
            {
                Assert(DestroyWindow(m_hwnd));
                UnregisterClassW(DummyClassName, m_hInstance);

                Win32_UnregisterWindowClass(DummyClassName);
            }

        private:
            HINSTANCE m_hInstance;
            HWND m_hwnd;
        };

        HINSTANCE hInstance = GetModuleHandleW(nullptr);

        WNDCLASSEXW windowClass = {};
        windowClass.cbSize = sizeof(WNDCLASSEXW);
        windowClass.lpfnWndProc = DefWindowProcW;
        windowClass.hInstance = hInstance;
        windowClass.lpszClassName = DummyClassName;

        ATOM classAtom = RegisterClassExW(&windowClass);
        if (classAtom == 0)
        {
            HYP_FAIL("Failed to register Win32 window class for Vulkan dummy window! Win32 Error: {}", GetLastError());
        }

        Win32_RegisterWindowClass(DummyClassName);

        HWND hwnd = CreateWindowExW(
            0,
            DummyClassName,
            L"Hyperion Vulkan Dummy Window",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            nullptr,
            nullptr,
            hInstance,
            nullptr);

        Assert(hwnd != nullptr);

        createInfo.hinstance = hInstance;
        createInfo.hwnd = hwnd;

        *ppOutDummySurfaceContext = new Win32DummyVulkanSurfaceContext(hInstance, hwnd);
    }

    VkResult vkResult = vkCreateWin32SurfaceKHR(
        RI.GetInstance()->GetInstance(),
        &createInfo,
        nullptr,
        &surface);

    Assert(vkResult == VK_SUCCESS, "Failed to create Win32 Vulkan surface: {}", int(vkResult));

    return surface;
}

#endif // HYP_VULKAN

} // namespace Hyperion
