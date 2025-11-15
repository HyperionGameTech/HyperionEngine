#include <editor/nativeui/TestNativeUI.hpp>

#include <ui.h>

#include <core/threading/Threads.hpp>
#include <core/threading/Scheduler.hpp>

#include <TestNativeUI.generated.inl>

namespace hyperion {

void TestNativeUI::Show_Internal()
{
    SetTitle("Add Reflection Probe");
    SetWindowSize(Vec2i(400, 450));

#ifdef HYP_LIBUI
    // Create "Add Reflection Probe" dialog using libui-ng
    uiWindow* window = m_window;
    AssertDebug(window != nullptr);

    // Make the window non-resizable (toolbox/modal style)
    uiWindowSetResizeable(window, 0);

    // Create a vertical box to hold our UI elements
    uiBox* vbox = uiNewVerticalBox();
    uiBoxSetPadded(vbox, 1);
    uiWindowSetChild(window, uiControl(vbox));

    // Add a title label
    uiLabel* titleLabel = uiNewLabel("Add Reflection Probe");
    uiBoxAppend(vbox, uiControl(titleLabel), 0);

    // Add separator
    uiBoxAppend(vbox, uiControl(uiNewHorizontalSeparator()), 0);

    // === Dimensions Selection ===
    uiBox* dimensionsBox = uiNewHorizontalBox();
    uiBoxSetPadded(dimensionsBox, 1);
    uiLabel* dimensionsLabel = uiNewLabel("Dimensions:");
    uiBoxAppend(dimensionsBox, uiControl(dimensionsLabel), 0);

    uiCombobox* dimensionsCombo = uiNewCombobox();
    uiComboboxAppend(dimensionsCombo, "64x64");
    uiComboboxAppend(dimensionsCombo, "128x128");
    uiComboboxAppend(dimensionsCombo, "256x256");
    uiComboboxSetSelected(dimensionsCombo, 1); // Default to 128x128
    uiBoxAppend(dimensionsBox, uiControl(dimensionsCombo), 1);
    uiBoxAppend(vbox, uiControl(dimensionsBox), 0);

    // === Dynamic Cubemap Checkbox ===
    uiCheckbox* isDynamicCheckbox = uiNewCheckbox("Is dynamic?");
    uiCheckboxSetChecked(isDynamicCheckbox, 0); // Default unchecked
    uiBoxAppend(vbox, uiControl(isDynamicCheckbox), 0);

    // Help text
    uiLabel* isDynamicCheckboxHelpText = uiNewLabel("If checked, the reflection probe will update in real-time to capture dynamic objects and lighting changes. Leave unchecked to bake lighting for static entities.");
    uiBoxAppend(vbox, uiControl(isDynamicCheckboxHelpText), 0);

    // Add separator
    uiBoxAppend(vbox, uiControl(uiNewHorizontalSeparator()), 0);

    // === Probe Volume Dimensions ===
    uiLabel* volumeLabel = uiNewLabel("Probe Volume Dimensions:");
    uiBoxAppend(vbox, uiControl(volumeLabel), 0);

    // X dimension
    uiBox* volumeXBox = uiNewHorizontalBox();
    uiBoxSetPadded(volumeXBox, 1);
    uiLabel* volumeXLabel = uiNewLabel("X:");
    uiBoxAppend(volumeXBox, uiControl(volumeXLabel), 0);
    uiEntry* volumeXEntry = uiNewEntry();
    uiEntrySetText(volumeXEntry, "50.0");
    uiBoxAppend(volumeXBox, uiControl(volumeXEntry), 1);
    uiBoxAppend(vbox, uiControl(volumeXBox), 0);

    // Y dimension
    uiBox* volumeYBox = uiNewHorizontalBox();
    uiBoxSetPadded(volumeYBox, 1);
    uiLabel* volumeYLabel = uiNewLabel("Y:");
    uiBoxAppend(volumeYBox, uiControl(volumeYLabel), 0);
    uiEntry* volumeYEntry = uiNewEntry();
    uiEntrySetText(volumeYEntry, "50.0");
    uiBoxAppend(volumeYBox, uiControl(volumeYEntry), 1);
    uiBoxAppend(vbox, uiControl(volumeYBox), 0);

    // Z dimension
    uiBox* volumeZBox = uiNewHorizontalBox();
    uiBoxSetPadded(volumeZBox, 1);
    uiLabel* volumeZLabel = uiNewLabel("Z:");
    uiBoxAppend(volumeZBox, uiControl(volumeZLabel), 0);
    uiEntry* volumeZEntry = uiNewEntry();
    uiEntrySetText(volumeZEntry, "50.0");
    uiBoxAppend(volumeZBox, uiControl(volumeZEntry), 1);
    uiBoxAppend(vbox, uiControl(volumeZBox), 0);

    // Add separator
    uiBoxAppend(vbox, uiControl(uiNewHorizontalSeparator()), 0);

    // === World Translation ===
    uiLabel* translationLabel = uiNewLabel("World Translation:");
    uiBoxAppend(vbox, uiControl(translationLabel), 0);

    // Translation X
    uiBox* translationXBox = uiNewHorizontalBox();
    uiBoxSetPadded(translationXBox, 1);
    uiLabel* translationXLabel = uiNewLabel("X:");
    uiBoxAppend(translationXBox, uiControl(translationXLabel), 0);
    uiEntry* translationXEntry = uiNewEntry();
    uiEntrySetText(translationXEntry, "0.0");
    uiBoxAppend(translationXBox, uiControl(translationXEntry), 1);
    uiBoxAppend(vbox, uiControl(translationXBox), 0);

    // Translation Y
    uiBox* translationYBox = uiNewHorizontalBox();
    uiBoxSetPadded(translationYBox, 1);
    uiLabel* translationYLabel = uiNewLabel("Y:");
    uiBoxAppend(translationYBox, uiControl(translationYLabel), 0);
    uiEntry* translationYEntry = uiNewEntry();
    uiEntrySetText(translationYEntry, "0.0");
    uiBoxAppend(translationYBox, uiControl(translationYEntry), 1);
    uiBoxAppend(vbox, uiControl(translationYBox), 0);

    // Translation Z
    uiBox* translationZBox = uiNewHorizontalBox();
    uiBoxSetPadded(translationZBox, 1);
    uiLabel* translationZLabel = uiNewLabel("Z:");
    uiBoxAppend(translationZBox, uiControl(translationZLabel), 0);
    uiEntry* translationZEntry = uiNewEntry();
    uiEntrySetText(translationZEntry, "0.0");
    uiBoxAppend(translationZBox, uiControl(translationZEntry), 1);
    uiBoxAppend(vbox, uiControl(translationZBox), 0);

    // Add separator
    uiBoxAppend(vbox, uiControl(uiNewHorizontalSeparator()), 0);

    // === OK and Cancel buttons ===
    uiBox* buttonBox = uiNewHorizontalBox();
    uiBoxSetPadded(buttonBox, 1);

    // Create a struct to hold all the UI controls we need to access in the callbacks
    struct DialogData
    {
        TestNativeUI* self;
        uiWindow* window;
        uiCombobox* dimensionsCombo;
        uiCheckbox* isDynamicCheckbox;
        uiEntry* volumeXEntry;
        uiEntry* volumeYEntry;
        uiEntry* volumeZEntry;
        uiEntry* translationXEntry;
        uiEntry* translationYEntry;
        uiEntry* translationZEntry;
    };

    auto* dialogData = new DialogData {
        this,
        window,
        dimensionsCombo,
        isDynamicCheckbox,
        volumeXEntry,
        volumeYEntry,
        volumeZEntry,
        translationXEntry,
        translationYEntry,
        translationZEntry
    };

    uiButton* okButton = uiNewButton("OK");
    uiButtonOnClicked(
        okButton, [](uiButton* b, void* data)
        {
            auto* dlgData = static_cast<DialogData*>(data);

            // Gather all the data from the form
            AddReflectionProbeResult result;

            // Get texture dimension from combobox
            int selectedDimension = uiComboboxSelected(dlgData->dimensionsCombo);
            switch (selectedDimension)
            {
            case 0:
                result.textureDimension = 64;
                break;
            case 1:
                result.textureDimension = 128;
                break;
            case 2:
                result.textureDimension = 256;
                break;
            default:
                result.textureDimension = 128;
                break;
            }

            // Get bake lighting checkbox state
            result.bakeLighting = uiCheckboxChecked(dlgData->isDynamicCheckbox) == 0;

            // Parse probe volume dimensions
            result.probeVolumeDimensions.x = float(atof(uiEntryText(dlgData->volumeXEntry)));
            result.probeVolumeDimensions.y = float(atof(uiEntryText(dlgData->volumeYEntry)));
            result.probeVolumeDimensions.z = float(atof(uiEntryText(dlgData->volumeZEntry)));

            // Parse world translation
            result.worldTranslation.x = float(atof(uiEntryText(dlgData->translationXEntry)));
            result.worldTranslation.y = float(atof(uiEntryText(dlgData->translationYEntry)));
            result.worldTranslation.z = float(atof(uiEntryText(dlgData->translationZEntry)));

            dlgData->self->OnAccepted(result);
            dlgData->self->OnAccepted.RemoveAllDetached();
            dlgData->self->Close();

            delete dlgData;
        },
        dialogData);
    uiBoxAppend(buttonBox, uiControl(okButton), 1);

    uiButton* cancelButton = uiNewButton("Cancel");
    uiButtonOnClicked(
        cancelButton, [](uiButton* b, void* data)
        {
            auto* dlgData = static_cast<DialogData*>(data);

            dlgData->self->OnCancelled();
            dlgData->self->Close();

            delete dlgData;
        },
        dialogData);
    uiBoxAppend(buttonBox, uiControl(cancelButton), 1);

    uiBoxAppend(vbox, uiControl(buttonBox), 0);
#endif
}

} // namespace hyperion
