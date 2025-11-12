#include <editor/nativeui/TestNativeUI.hpp>

#include <ui.h>

#include <core/threading/Threads.hpp>
#include <core/threading/Scheduler.hpp>

#include <TestNativeUI.generated.inl>

namespace hyperion {

void TestNativeUI::Show_Internal()
{
    // test native UI window using libui-ng
    uiWindow* window = m_window;
    AssertDebug(window != nullptr);

    // Create a vertical box to hold our UI elements
    uiBox* vbox = uiNewVerticalBox();
    uiBoxSetPadded(vbox, 1);
    uiWindowSetChild(window, uiControl(vbox));

    // Add a title label
    uiLabel* titleLabel = uiNewLabel("Hyperion Native UI Test");
    uiBoxAppend(vbox, uiControl(titleLabel), 0);

    // Add some spacing
    uiBoxAppend(vbox, uiControl(uiNewHorizontalSeparator()), 0);

    // Add a description label
    uiLabel* descLabel = uiNewLabel("This is a test window using libui-ng");
    uiBoxAppend(vbox, uiControl(descLabel), 0);

    // Create a struct to hold click counts (needs to persist)
    struct ButtonData
    {
        int clickCount = 0;
    };
    auto* button1Data = new ButtonData();
    auto* button2Data = new ButtonData();

    // Add a button that changes text on click
    uiButton* button1 = uiNewButton("Click Me!");
    uiButtonOnClicked(button1, [](uiButton* b, void* data)
        {
            auto* btnData = static_cast<ButtonData*>(data);
            btnData->clickCount++;

            char buffer[64];
            snprintf(buffer, sizeof(buffer), "Clicked %d time%s!",
                btnData->clickCount,
                btnData->clickCount == 1 ? "" : "s");
            uiButtonSetText(b, buffer);
        },
        button1Data);
    uiBoxAppend(vbox, uiControl(button1), 0);

    // Add another button that toggles between two states
    uiButton* button2 = uiNewButton("Toggle Me");
    uiButtonOnClicked(button2, [](uiButton* b, void* data)
        {
            auto* btnData = static_cast<ButtonData*>(data);
            btnData->clickCount++;

            if (btnData->clickCount % 2 == 1)
            {
                uiButtonSetText(b, "Toggled ON");
            }
            else
            {
                uiButtonSetText(b, "Toggle Me");
            }
        },
        button2Data);
    uiBoxAppend(vbox, uiControl(button2), 0);

    // Add a status label
    uiLabel* statusLabel = uiNewLabel("Status: Ready");
    uiBoxAppend(vbox, uiControl(statusLabel), 0);
}

} // namespace hyperion
