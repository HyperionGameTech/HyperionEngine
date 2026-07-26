/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <SystemPch.hpp>

#include <System/AppContext.hpp>

#include <Input/InputManager.hpp>
#include <Input/Event.hpp>

#include <Core/Threading/Threads.hpp>

#include <Core/Debug/Debug.hpp>

#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/keycodes.h>

#include <jni.h>

#if HYP_VULKAN
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>

#include <Rendering/Vulkan/VulkanInstance.hpp>
#include <Rendering/Vulkan/VulkanRenderInterface.hpp>
#endif

namespace Hyperion {

CORE_API HYP_DECLARE_LOG_CHANNEL(Core);

namespace {

JavaVM* g_javaVM = nullptr;
jclass g_hyperionBridgeClass = nullptr;
jmethodID g_showSoftKeyboardMethod = nullptr;
jmethodID g_hideSoftKeyboardMethod = nullptr;

JNIEnv* GetJNIEnvForCurrentThread(bool& outDidAttach)
{
    outDidAttach = false;

    if (g_javaVM == nullptr)
    {
        return nullptr;
    }

    JNIEnv* env = nullptr;

    if (g_javaVM->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_OK)
    {
        return env;
    }

    if (g_javaVM->AttachCurrentThread(&env, nullptr) != JNI_OK)
    {
        return nullptr;
    }

    outDidAttach = true;

    return env;
}

void CallSoftKeyboardMethod(jmethodID methodId)
{
    if (g_hyperionBridgeClass == nullptr || methodId == nullptr)
    {
        return;
    }

    bool didAttach = false;
    JNIEnv* env = GetJNIEnvForCurrentThread(didAttach);

    if (env == nullptr)
    {
        return;
    }

    env->CallStaticVoidMethod(g_hyperionBridgeClass, methodId);

    if (didAttach)
    {
        g_javaVM->DetachCurrentThread();
    }
}

} // anonymous namespace

static KeyCode MapAndroidKeyCodeToKeyCode(int32_t androidKeyCode)
{
    switch (androidKeyCode)
    {
    case AKEYCODE_A:
        return KeyCode::KEY_A;
    case AKEYCODE_B:
        return KeyCode::KEY_B;
    case AKEYCODE_C:
        return KeyCode::KEY_C;
    case AKEYCODE_D:
        return KeyCode::KEY_D;
    case AKEYCODE_E:
        return KeyCode::KEY_E;
    case AKEYCODE_F:
        return KeyCode::KEY_F;
    case AKEYCODE_G:
        return KeyCode::KEY_G;
    case AKEYCODE_H:
        return KeyCode::KEY_H;
    case AKEYCODE_I:
        return KeyCode::KEY_I;
    case AKEYCODE_J:
        return KeyCode::KEY_J;
    case AKEYCODE_K:
        return KeyCode::KEY_K;
    case AKEYCODE_L:
        return KeyCode::KEY_L;
    case AKEYCODE_M:
        return KeyCode::KEY_M;
    case AKEYCODE_N:
        return KeyCode::KEY_N;
    case AKEYCODE_O:
        return KeyCode::KEY_O;
    case AKEYCODE_P:
        return KeyCode::KEY_P;
    case AKEYCODE_Q:
        return KeyCode::KEY_Q;
    case AKEYCODE_R:
        return KeyCode::KEY_R;
    case AKEYCODE_S:
        return KeyCode::KEY_S;
    case AKEYCODE_T:
        return KeyCode::KEY_T;
    case AKEYCODE_U:
        return KeyCode::KEY_U;
    case AKEYCODE_V:
        return KeyCode::KEY_V;
    case AKEYCODE_W:
        return KeyCode::KEY_W;
    case AKEYCODE_X:
        return KeyCode::KEY_X;
    case AKEYCODE_Y:
        return KeyCode::KEY_Y;
    case AKEYCODE_Z:
        return KeyCode::KEY_Z;
    case AKEYCODE_0:
        return KeyCode::KEY_0;
    case AKEYCODE_1:
        return KeyCode::KEY_1;
    case AKEYCODE_2:
        return KeyCode::KEY_2;
    case AKEYCODE_3:
        return KeyCode::KEY_3;
    case AKEYCODE_4:
        return KeyCode::KEY_4;
    case AKEYCODE_5:
        return KeyCode::KEY_5;
    case AKEYCODE_6:
        return KeyCode::KEY_6;
    case AKEYCODE_7:
        return KeyCode::KEY_7;
    case AKEYCODE_8:
        return KeyCode::KEY_8;
    case AKEYCODE_9:
        return KeyCode::KEY_9;
    case AKEYCODE_F1:
        return KeyCode::KEY_F1;
    case AKEYCODE_F2:
        return KeyCode::KEY_F2;
    case AKEYCODE_F3:
        return KeyCode::KEY_F3;
    case AKEYCODE_F4:
        return KeyCode::KEY_F4;
    case AKEYCODE_F5:
        return KeyCode::KEY_F5;
    case AKEYCODE_F6:
        return KeyCode::KEY_F6;
    case AKEYCODE_F7:
        return KeyCode::KEY_F7;
    case AKEYCODE_F8:
        return KeyCode::KEY_F8;
    case AKEYCODE_F9:
        return KeyCode::KEY_F9;
    case AKEYCODE_F10:
        return KeyCode::KEY_F10;
    case AKEYCODE_F11:
        return KeyCode::KEY_F11;
    case AKEYCODE_F12:
        return KeyCode::KEY_F12;
    case AKEYCODE_SPACE:
        return KeyCode::KEY_SPACE;
    case AKEYCODE_ENTER:
        return KeyCode::KEY_RETURN;
    case AKEYCODE_TAB:
        return KeyCode::KEY_TAB;
    case AKEYCODE_DEL:
        return KeyCode::KEY_BACKSPACE;
    case AKEYCODE_CAPS_LOCK:
        return KeyCode::KEY_CAPSLOCK;
    case AKEYCODE_GRAVE:
        return KeyCode::KEY_TILDE;
    case AKEYCODE_ESCAPE:
        return KeyCode::KEY_ESCAPE;
    case AKEYCODE_SHIFT_LEFT:
        return KeyCode::KEY_LSHIFT;
    case AKEYCODE_SHIFT_RIGHT:
        return KeyCode::KEY_RSHIFT;
    case AKEYCODE_CTRL_LEFT:
        return KeyCode::KEY_LCTRL;
    case AKEYCODE_CTRL_RIGHT:
        return KeyCode::KEY_RCTRL;
    case AKEYCODE_ALT_LEFT:
        return KeyCode::KEY_LALT;
    case AKEYCODE_ALT_RIGHT:
        return KeyCode::KEY_RALT;
    case AKEYCODE_DPAD_LEFT:
        return KeyCode::KEY_LEFT;
    case AKEYCODE_DPAD_RIGHT:
        return KeyCode::KEY_RIGHT;
    case AKEYCODE_DPAD_UP:
        return KeyCode::KEY_UP;
    case AKEYCODE_DPAD_DOWN:
        return KeyCode::KEY_DOWN;
    case AKEYCODE_COMMA:
        return KeyCode::KEY_COMMA;
    case AKEYCODE_MINUS:
        return KeyCode::KEY_DASH;
    case AKEYCODE_PERIOD:
        return KeyCode::KEY_PERIOD;
    default:
        return KeyCode::KEY_UNKNOWN;
    }
}

static constexpr int32 ANDROID_ACTION_DOWN = 0;
static constexpr int32 ANDROID_ACTION_UP = 1;
static constexpr int32 ANDROID_ACTION_MOVE = 2;
static constexpr int32 ANDROID_ACTION_POINTER_DOWN = 5;
static constexpr int32 ANDROID_ACTION_POINTER_UP = 6;

static constexpr int32 ANDROID_KEY_ACTION_DOWN = 0;
static constexpr int32 ANDROID_KEY_ACTION_UP = 1;

AndroidApplicationWindow::AndroidApplicationWindow(ANSIString title, Vec2i size)
    : ApplicationWindow(std::move(title), size),
      m_nativeWindow(nullptr)
{
}

AndroidApplicationWindow::~AndroidApplicationWindow()
{
    if (m_nativeWindow != nullptr)
    {
        ANativeWindow_release(static_cast<ANativeWindow*>(m_nativeWindow));
        m_nativeWindow = nullptr;
    }
}

void AndroidApplicationWindow::Initialize(WindowOptions windowOptions)
{
    AssertOnThread(g_mainThread);

    TUniqueLock lock(m_mtx);

    m_title = windowOptions.title;
    m_size = windowOptions.dimensions;
}

void AndroidApplicationWindow::SetNativeWindow(void* nativeWindow, Vec2i size)
{
    Vec2i newSize = GetSize();

    {
        TUniqueLock lock(m_mtx);

        if (m_nativeWindow != nullptr)
        {
            ANativeWindow_release(static_cast<ANativeWindow*>(m_nativeWindow));
        }

        m_nativeWindow = nativeWindow;

        if (m_nativeWindow != nullptr)
        {
            ANativeWindow_acquire(static_cast<ANativeWindow*>(m_nativeWindow));

            if (size.x > 0 && size.y > 0)
            {
                // dimensions reported by surfaceChanged(); these are authoritative
                newSize = size;

                const Vec2i readbackSize {
                    ANativeWindow_getWidth(static_cast<ANativeWindow*>(m_nativeWindow)),
                    ANativeWindow_getHeight(static_cast<ANativeWindow*>(m_nativeWindow))
                };

                if (readbackSize != newSize)
                {
                    HYP_LOG(Core, Verbose, "Native window reports {} but the surface reports {}; using the surface's size", readbackSize, newSize);
                }
            }
            else
            {
                newSize = Vec2i {
                    ANativeWindow_getWidth(static_cast<ANativeWindow*>(m_nativeWindow)),
                    ANativeWindow_getHeight(static_cast<ANativeWindow*>(m_nativeWindow))
                };
            }

            m_hwnd = m_nativeWindow;
        }
        else
        {
            m_hwnd = nullptr;
        }
    }

    HandleResize(newSize);
}

void AndroidApplicationWindow::SetMousePosition(Vec2i position)
{
    // can't do that on Android touch devices
}

Vec2i AndroidApplicationWindow::GetMousePosition() const
{
    TSharedLock lock(m_mtx);
    return Vec2i(m_touchPosition);
}

Vec2i AndroidApplicationWindow::GetDimensions() const
{
    TSharedLock lock(m_mtx);

    if (m_nativeWindow != nullptr)
    {
        return Vec2i {
            ANativeWindow_getWidth(static_cast<ANativeWindow*>(m_nativeWindow)),
            ANativeWindow_getHeight(static_cast<ANativeWindow*>(m_nativeWindow))
        };
    }

    return m_size;
}

void AndroidApplicationWindow::SetIsMouseLocked(bool locked)
{
    m_mouseLocked = locked;
}

bool AndroidApplicationWindow::HasMouseFocus() const
{
    return m_nativeWindow != nullptr;
}

float AndroidApplicationWindow::GetContentScaleFactor() const
{
    return 2.0f;
}

float AndroidApplicationWindow::GetRenderTargetScale() const
{
    // Render at 60% of native resolution
    return 0.6f;
}

void AndroidApplicationWindow::Close()
{
    TUniqueLock lock(m_mtx);

    if (m_nativeWindow != nullptr)
    {
        ANativeWindow_release(static_cast<ANativeWindow*>(m_nativeWindow));
        m_nativeWindow = nullptr;
        m_hwnd = nullptr;
    }

#if HYP_VULKAN
    if (m_vkSurface != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(
            RI.GetInstance()->GetInstance(),
            m_vkSurface,
            nullptr);
        m_vkSurface = VK_NULL_HANDLE;
    }
#endif
}

bool AndroidApplicationWindow::HandleInputEvent(int32 type, int32 action, float x, float y, int32 intParam, Event& outEvent)
{
    PlatformEvent platformEvent {};

    switch (type)
    {
    case 0: // touch
    {
        const Vec2f currentPos = { x, y };

        {
            TUniqueLock lock(m_mtx);
            m_touchPosition = currentPos;
        }

        // intParam contains the pointer ID for multi-touch support
        const int32 pointerId = intParam;

        switch (action)
        {
        case ANDROID_ACTION_DOWN:
        case ANDROID_ACTION_POINTER_DOWN:
        {
            outEvent = Event(EventType::TOUCH_DOWN, this, platformEvent);

            MotionData motionData { currentPos, Vec2f::Zero(), /* isAbsolute */ false };
            outEvent.GetEventData().Set(TouchEventData { pointerId, motionData });

            if (pointerId >= 0 && pointerId < static_cast<int32>(m_touchPrevPositions.Size()))
            {
                m_touchPrevPositions[pointerId] = currentPos;
            }

            return true;
        }
        case ANDROID_ACTION_UP:
        case ANDROID_ACTION_POINTER_UP:
        {
            outEvent = Event(EventType::TOUCH_UP, this, platformEvent);

            Vec2f prevPos = currentPos;
            if (pointerId >= 0 && pointerId < static_cast<int32>(m_touchPrevPositions.Size()))
            {
                prevPos = m_touchPrevPositions[pointerId];
            }

            MotionData motionData { currentPos, currentPos - prevPos, /* isAbsolute */ false };
            outEvent.GetEventData().Set(TouchEventData { pointerId, motionData });

            return true;
        }
        case ANDROID_ACTION_MOVE:
        {
            outEvent = Event(EventType::TOUCH_MOVE, this, platformEvent);

            // Get previous position for this specific pointer
            Vec2f prevPos = currentPos;
            if (pointerId >= 0 && pointerId < static_cast<int32>(m_touchPrevPositions.Size()))
            {
                prevPos = m_touchPrevPositions[pointerId];
            }

            Vec2f delta = currentPos - prevPos;
            MotionData motionData { currentPos, delta, /* isAbsolute */ false };
            outEvent.GetEventData().Set(TouchEventData { pointerId, motionData });

            // Update stored position for this pointer
            if (pointerId >= 0 && pointerId < static_cast<int32>(m_touchPrevPositions.Size()))
            {
                m_touchPrevPositions[pointerId] = currentPos;
            }

            return true;
        }
        default:
            break;
        }

        break;
    }

    case 1: // key
    {
        const KeyCode mappedKey = MapAndroidKeyCodeToKeyCode(intParam);

        switch (action)
        {
        case ANDROID_KEY_ACTION_DOWN:
            outEvent = Event(EventType::KEYDOWN, this, platformEvent);
            outEvent.GetEventData().Set(mappedKey);
            return true;

        case ANDROID_KEY_ACTION_UP:
            outEvent = Event(EventType::KEYUP, this, platformEvent);
            outEvent.GetEventData().Set(mappedKey);
            return true;

        default:
            break;
        }

        break;
    }

    default:
        break;
    }

    return false;
}

bool AndroidApplicationWindow::HandleTextInputEvent(const String& text, Event& outEvent)
{
    if (text.Empty())
    {
        return false;
    }

    PlatformEvent platformEvent {};

    outEvent = Event(EventType::TEXT_INPUT, this, platformEvent);
    outEvent.GetEventData().Set(text);

    return true;
}

void AndroidApplicationWindow::ShowVirtualKeyboard()
{
    CallSoftKeyboardMethod(g_showSoftKeyboardMethod);
}

void AndroidApplicationWindow::HideVirtualKeyboard()
{
    CallSoftKeyboardMethod(g_hideSoftKeyboardMethod);
}

} // namespace Hyperion

extern "C" void Hyp_Android_InitJNI(JNIEnv* env, jclass hyperionBridgeClass)
{
    using namespace Hyperion;

    if (g_hyperionBridgeClass != nullptr)
    {
        return;
    }

    env->GetJavaVM(&g_javaVM);

    g_hyperionBridgeClass = reinterpret_cast<jclass>(env->NewGlobalRef(hyperionBridgeClass));
    g_showSoftKeyboardMethod = env->GetStaticMethodID(g_hyperionBridgeClass, "showSoftKeyboard", "()V");
    g_hideSoftKeyboardMethod = env->GetStaticMethodID(g_hyperionBridgeClass, "hideSoftKeyboard", "()V");
}
