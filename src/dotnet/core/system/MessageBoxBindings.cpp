/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/system/MessageBox.hpp>

#include <Types.hpp>

using namespace hyperion;

extern "C" {

HYP_EXPORT void MessageBox_Show(int type, const char *title, const char *message, int buttons, const char **button_texts, void(**button_callbacks)(void))
{
    SystemMessageBox message_box { MessageBoxType(type), title, message };

    for (int i = 0; i < buttons; i++) {
        message_box.Button(button_texts[i], [button_callbacks, i]() -> void
        {
            button_callbacks[i]();
        });
    }

    message_box.Show();
}

} // extern "C"