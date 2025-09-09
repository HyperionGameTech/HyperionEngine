/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <system/MessageBox.hpp>

#include <core/Types.hpp>

using namespace hyperion;

extern "C"
{

    HYP_EXPORT void MessageBox_Show(int type, const char* title, const char* message, int buttons, const char** buttonTexts, void (**buttonCallbacks)(void))
    {
        SystemMessageBox messageBox { MessageBoxType(type), title, message };

        for (int i = 0; i < buttons; i++)
        {
            messageBox.Button(buttonTexts[i], [buttonCallbacks, i]() -> void
                {
                    buttonCallbacks[i]();
                });
        }

        messageBox.Show();
    }

} // extern "C"