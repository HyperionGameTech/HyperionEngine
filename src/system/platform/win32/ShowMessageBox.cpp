#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <cstring>

static int FindNextButtonID(int startIndex, int buttons, const char *buttonTexts[3])
{
    if (startIndex >= buttons) {
        return -1;
    }
    
    if (strcmp("OK", buttonTexts[startIndex]) == 0) {
        return IDOK;
    }

    if (strcmp("Cancel", buttonTexts[startIndex]) == 0) {
        return IDCANCEL;
    }

    if (strcmp("Abort", buttonTexts[startIndex]) == 0) {
        return IDABORT;
    }

    if (strcmp("Retry", buttonTexts[startIndex]) == 0) {
        return IDRETRY;
    }

    if (strcmp("Ignore", buttonTexts[startIndex]) == 0) {
        return IDIGNORE;
    }

    if (strcmp("Yes", buttonTexts[startIndex]) == 0) {
        return IDYES;
    }

    if (strcmp("No", buttonTexts[startIndex]) == 0) {
        return IDNO;
    }

    if (strcmp("Try Again", buttonTexts[startIndex]) == 0) {
        return IDTRYAGAIN;
    }

    if (strcmp("Continue", buttonTexts[startIndex]) == 0) {
        return IDCONTINUE;
    }

    if (strcmp("Close", buttonTexts[startIndex]) == 0) {
        return IDCLOSE;
    }

    if (strcmp("Help", buttonTexts[startIndex]) == 0) {
        return IDHELP;
    }

    return -1;
}

extern "C" {

int ShowMessageBox(int type, const char *title, const char *message, int buttons, const char *buttonTexts[3])
{
    int buttonIndices[16] = { -1 };
    int startIndex = 0;
    int buttonId = -1;
    
    while (startIndex < buttons && (buttonId = FindNextButtonID(startIndex, buttons, buttonTexts)) != -1) {
        buttonIndices[buttonId] = startIndex++;
    }

    int titleLength = MultiByteToWideChar(CP_UTF8, 0, title, -1, nullptr, 0);
    int messageLength = MultiByteToWideChar(CP_UTF8, 0, message, -1, nullptr, 0);

    wchar_t* wideTitle = new wchar_t[titleLength];
    wchar_t* wideMessage = new wchar_t[messageLength];

    MultiByteToWideChar(CP_UTF8, 0, title, -1, wideTitle, titleLength);
    MultiByteToWideChar(CP_UTF8, 0, message, -1, wideMessage, messageLength);

    wchar_t* wideButtonTexts[3] = { };

    for (int i = 0; i < buttons; i++) {
        int buttonTextLength = MultiByteToWideChar(CP_UTF8, 0, buttonTexts[i], -1, nullptr, 0);
        wideButtonTexts[i] = new wchar_t[buttonTextLength];
        MultiByteToWideChar(CP_UTF8, 0, buttonTexts[i], -1, wideButtonTexts[i], buttonTextLength);
    }

    UINT typeFlags = 0;

    switch (type) {
    case 0:
        typeFlags = MB_ICONINFORMATION;
        break;
    case 1:
        typeFlags = MB_ICONWARNING;
        break;
    case 2:
        typeFlags = MB_ICONERROR;
        break;
    }

    if (buttonIndices[IDOK] != -1) {
        typeFlags |= MB_OK;
    }

    if (buttonIndices[IDCANCEL] != -1) {
        if (buttonIndices[IDOK] != -1) {
            typeFlags |= MB_OKCANCEL;
        } else if (buttonIndices[IDRETRY] != -1) {
            typeFlags |= MB_RETRYCANCEL;
        } else if (buttonIndices[IDTRYAGAIN] != -1 && buttonIndices[IDCONTINUE] != -1) {
            typeFlags |= MB_CANCELTRYCONTINUE;
        }
    }

    if (buttonIndices[IDYES] != -1 && buttonIndices[IDNO] == -1) {
        if (buttonIndices[IDCANCEL] != -1) {
            typeFlags |= MB_YESNOCANCEL;
        } else {
            typeFlags |= MB_YESNO;
        }
    }

    if (buttonIndices[IDHELP] != -1) {
        typeFlags |= MB_HELP;
    }

    int result = MessageBoxW(NULL, wideMessage, wideTitle, typeFlags);
    int buttonIndex;

    if (result < 16) {
        buttonIndex = buttonIndices[result];
    } else {
        buttonIndex = -1;
    }

    delete[] wideTitle;
    delete[] wideMessage;

    for (int i = 0; i < buttons; i++) {
        delete[] wideButtonTexts[i];
    }

    return buttonIndex;
}

}