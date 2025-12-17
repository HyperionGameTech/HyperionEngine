#include <EditorPch.hpp>

#include <editor/EditorWindow.hpp>

#include <core/threading/Threads.hpp>
#include <core/threading/Scheduler.hpp>

#ifdef HYP_LIBUI
#include <ui.h>
#endif

#include <EditorWindow.generated.inl>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

EditorWindow::EditorWindow()
    : m_title("Window"),
      m_windowSize(800, 600)
{
}

EditorWindow::~EditorWindow()
{
}

void EditorWindow::SetTitle(const String& title)
{
    m_title = title;

#ifdef HYP_LIBUI
    if (m_window != nullptr)
    {
        uiWindowSetTitle(m_window, m_title.Data());
    }
#endif
}

void EditorWindow::SetWindowSize(const Vec2i& size)
{
    m_windowSize = size;

#ifdef HYP_LIBUI
    if (m_window != nullptr)
    {
        uiWindowSetContentSize(m_window, m_windowSize.x, m_windowSize.y);
    }
#endif
}

void EditorWindow::Show()
{
#ifndef HYP_LIBUI
    HYP_LOG(Editor, Warning, "Attempted to show EditorWindow but HYP_LIBUI is not enabled!");
#else
    auto showWindowImpl = [this]()
    {
        AssertDebug(m_window == nullptr);

        m_header->IncRefStrong(); // to keep this alive until window closing

        m_window = uiNewWindow(m_title.Data(), m_windowSize.x, m_windowSize.y, 0);
        uiWindowOnClosing(
            m_window, [](uiWindow* w, void* data)
            {
                EditorWindow* editorWindow = static_cast<EditorWindow*>(data);

                // clear window pointer on close
                editorWindow->m_window = nullptr;

                editorWindow->m_header->DecRefStrong();

                // returning 1 destroys the window
                return 1;
            },
            this);

        uiWindowSetMargined(m_window, 1);

        Show_Internal();

        uiControlShow(uiControl(m_window));
    };

    if (IsOnThread(g_mainThread))
    {
        showWindowImpl();

        return;
    }

    GetThreadById(g_mainThread)->GetScheduler().Enqueue([strongRef = MakeStrongRef(this), showWindowImpl]() mutable
        {
            showWindowImpl();
        },
        TaskEnqueueFlags::FIRE_AND_FORGET);
#endif
}

void EditorWindow::Close()
{
#ifdef HYP_LIBUI
    if (!m_window)
    {
        return;
    }

    uiControlDestroy(uiControl(m_window));
    m_window = nullptr;

    // dec extra ref taken in Show()
    m_header->DecRefStrong();
#endif
}

} // namespace hyperion
