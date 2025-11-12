#include <core/Defines.hpp>

#include <editor/EditorWindow.hpp>

#include <core/threading/Threads.hpp>
#include <core/threading/Scheduler.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#ifdef HYP_LIBUI
#include <ui.h>
#endif

#include <EditorWindow.generated.inl>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

EditorWindow::EditorWindow()
{
}

EditorWindow::~EditorWindow()
{
}

void EditorWindow::Show()
{
#ifndef HYP_LIBUI
    HYP_LOG(Editor, Warning, "Attempted to show EditorWindow but HYP_LIBUI is not enabled!");
#else
    auto showWindowImpl = [this]()
    {
        AssertDebug(m_window == nullptr);

        m_window = uiNewWindow("Test Native UI", 400, 300, 0);
        uiWindowOnClosing(m_window, [](uiWindow* w, void* data)
            {
                // clear window pointer on close
                static_cast<EditorWindow*>(data)->m_window = nullptr;

                // returning 1 destroys the window
                return 1;
            },
            this);

        uiWindowSetMargined(m_window, 1);

        Show_Internal();

        uiControlShow(uiControl(m_window));
    };

    if (Threads::IsOnThread(g_mainThread))
    {
        showWindowImpl();

        return;
    }

    Threads::GetThread(g_mainThread)->GetScheduler().Enqueue([strongRef = MakeStrongRef(this), showWindowImpl]()
        {
            showWindowImpl();
        },
        TaskEnqueueFlags::FIRE_AND_FORGET);
#endif
}

} // namespace hyperion
