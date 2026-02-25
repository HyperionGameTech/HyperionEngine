/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <HyperionPch.hpp>

#include <system/MessageBox.hpp>

using namespace Hyperion;

extern "C"
{

    HYP_EXPORT void MessageBox_Show(int type, const char* pTitle, const char* pMessage, int buttons, const char** ppButtonTexts, void (**ppCallbacks)(void))
    {
        SystemMessageBox messageBox { MessageBoxType(type), pTitle, pMessage };

        for (int i = 0; i < buttons; i++)
        {
            messageBox.Button(ppButtonTexts[i], [ppCallbacks, i]() -> void
                {
                    ppCallbacks[i]();
                });
        }

        messageBox.Show();
    }

} // extern "C"
