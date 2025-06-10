#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <cstring>

static int FindNextButtonID(int start_index, int buttons, const char* button_texts[3])
{
    if (start_index >= buttons)
    {
        return -1;
    }

    if (strcmp("OK", button_texts[start_index]) == 0)
    {
        return IDOK;
    }

    if (strcmp("Cancel", button_texts[start_index]) == 0)
    {
        return IDCANCEL;
    }

    if (strcmp("Abort", button_texts[start_index]) == 0)
    {
        return IDABORT;
    }

    if (strcmp("Retry", button_texts[start_index]) == 0)
    {
        return IDRETRY;
    }

    if (strcmp("Ignore", button_texts[start_index]) == 0)
    {
        return IDIGNORE;
    }

    if (strcmp("Yes", button_texts[start_index]) == 0)
    {
        return IDYES;
    }

    if (strcmp("No", button_texts[start_index]) == 0)
    {
        return IDNO;
    }

    if (strcmp("Try Again", button_texts[start_index]) == 0)
    {
        return IDTRYAGAIN;
    }

    if (strcmp("Continue", button_texts[start_index]) == 0)
    {
        return IDCONTINUE;
    }

    if (strcmp("Close", button_texts[start_index]) == 0)
    {
        return IDCLOSE;
    }

    if (strcmp("Help", button_texts[start_index]) == 0)
    {
        return IDHELP;
    }

    return -1;
}

extern "C"
{

    int ShowMessageBox(int type, const char* title, const char* message, int buttons, const char* button_texts[3])
    {
        int button_indices[16] = { -1 };
        int start_index = 0;
        int button_id = -1;

        while (start_index < buttons && (button_id = FindNextButtonID(start_index, buttons, button_texts)) != -1)
        {
            button_indices[button_id] = start_index++;
        }

        int title_length = MultiByteToWideChar(CP_UTF8, 0, title, -1, nullptr, 0);
        int message_length = MultiByteToWideChar(CP_UTF8, 0, message, -1, nullptr, 0);

        wchar_t* wide_title = new wchar_t[title_length];
        wchar_t* wide_message = new wchar_t[message_length];

        MultiByteToWideChar(CP_UTF8, 0, title, -1, wide_title, title_length);
        MultiByteToWideChar(CP_UTF8, 0, message, -1, wide_message, message_length);

        wchar_t* wide_button_texts[3] = {};

        for (int i = 0; i < buttons; i++)
        {
            int button_text_length = MultiByteToWideChar(CP_UTF8, 0, button_texts[i], -1, nullptr, 0);
            wide_button_texts[i] = new wchar_t[button_text_length];
            MultiByteToWideChar(CP_UTF8, 0, button_texts[i], -1, wide_button_texts[i], button_text_length);
        }

        UINT type_flags = 0;

        switch (type)
        {
        case 0:
            type_flags = MB_ICONINFORMATION;
            break;
        case 1:
            type_flags = MB_ICONWARNING;
            break;
        case 2:
            type_flags = MB_ICONERROR;
            break;
        }

        if (button_indices[IDOK] != -1)
        {
            type_flags |= MB_OK;
        }

        if (button_indices[IDCANCEL] != -1)
        {
            if (button_indices[IDOK] != -1)
            {
                type_flags |= MB_OKCANCEL;
            }
            else if (button_indices[IDRETRY] != -1)
            {
                type_flags |= MB_RETRYCANCEL;
            }
            else if (button_indices[IDTRYAGAIN] != -1 && button_indices[IDCONTINUE] != -1)
            {
                type_flags |= MB_CANCELTRYCONTINUE;
            }
        }

        if (button_indices[IDYES] != -1 && button_indices[IDNO] == -1)
        {
            if (button_indices[IDCANCEL] != -1)
            {
                type_flags |= MB_YESNOCANCEL;
            }
            else
            {
                type_flags |= MB_YESNO;
            }
        }

        if (button_indices[IDHELP] != -1)
        {
            type_flags |= MB_HELP;
        }

        int result = MessageBoxW(NULL, wide_message, wide_title, type_flags);
        int button_index;

        if (result < 16)
        {
            button_index = button_indices[result];
        }
        else
        {
            button_index = -1;
        }

        delete[] wide_title;
        delete[] wide_message;

        for (int i = 0; i < buttons; i++)
        {
            delete[] wide_button_texts[i];
        }

        return button_index;
    }
}