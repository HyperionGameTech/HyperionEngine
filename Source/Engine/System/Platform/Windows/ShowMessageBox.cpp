#include <SystemPch.hpp>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <CommCtrl.h>

static void ToWideString(const char* utf8, wchar_t*& outWide)
{
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
    outWide = new wchar_t[len];
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, outWide, len);
}

typedef HRESULT(WINAPI* TaskDialogIndirectProc)(const TASKDIALOGCONFIG*, int*, int*, BOOL*);

static HMODULE LoadComctl32v6()
{
    HMODULE hComCtl = GetModuleHandleW(L"comctl32.dll");

    if (hComCtl)
    {
        if (GetProcAddress(hComCtl, "TaskDialogIndirect"))
        {
            return hComCtl;
        }
    }

    wchar_t tempPath[MAX_PATH];

    if (GetTempPathW(MAX_PATH, tempPath) == 0)
    {
        return nullptr;
    }

    wchar_t tempFile[MAX_PATH];

    if (GetTempFileNameW(tempPath, L"TD", 0, tempFile) == 0)
    {
        return nullptr;
    }

    const char* manifestXml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">"
        "<dependency>"
        "<dependentAssembly>"
        "<assemblyIdentity type=\"win32\" name=\"Microsoft.Windows.Common-Controls\" "
        "version=\"6.0.0.0\" processorArchitecture=\"*\" "
        "publicKeyToken=\"6595b64144ccf1df\" language=\"*\" />"
        "</dependentAssembly>"
        "</dependency>"
        "</assembly>";

    HANDLE hFile = CreateFileW(
        tempFile,
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_TEMPORARY,
        nullptr
    );

    if (hFile == INVALID_HANDLE_VALUE)
    {
        return nullptr;
    }

    DWORD written = 0;
    WriteFile(hFile, manifestXml, (DWORD)strlen(manifestXml), &written, nullptr);
    CloseHandle(hFile);

    ACTCTXW actCtx = {};
    actCtx.cbSize = sizeof(actCtx);
    actCtx.lpSource = tempFile;

    HANDLE hActCtx = CreateActCtxW(&actCtx);

    DeleteFileW(tempFile);

    if (hActCtx == INVALID_HANDLE_VALUE)
    {
        return nullptr;
    }

    ULONG_PTR cookie = 0;

    if (!ActivateActCtx(hActCtx, &cookie))
    {
        ReleaseActCtx(hActCtx);

        return nullptr;
    }

    hComCtl = LoadLibraryW(L"comctl32.dll");

    DeactivateActCtx(0, cookie);
    ReleaseActCtx(hActCtx);

    return hComCtl;
}

static TaskDialogIndirectProc GetTaskDialogIndirect()
{
    static TaskDialogIndirectProc pfn = (TaskDialogIndirectProc)-1;

    if (pfn == (TaskDialogIndirectProc)-1)
    {
        HMODULE hComCtl = LoadComctl32v6();

        if (hComCtl)
        {
            pfn = (TaskDialogIndirectProc)GetProcAddress(hComCtl, "TaskDialogIndirect");
        }
        else
        {
            pfn = nullptr;
        }
    }

    return pfn;
}

extern "C"
{

    int ShowMessageBox(int type, const char* title, const char* message, int buttons, const char* buttonTexts[3])
    {
        TaskDialogIndirectProc pTaskDialogIndirect = GetTaskDialogIndirect();

        wchar_t* wideTitle = nullptr;
        wchar_t* wideMessage = nullptr;
        ToWideString(title, wideTitle);
        ToWideString(message, wideMessage);

        const int MaxButtons = 3;
        wchar_t* wideButtonTexts[MaxButtons] = {};
        TASKDIALOG_BUTTON taskDialogButtons[MaxButtons] = {};

        for (int i = 0; i < buttons; i++)
        {
            ToWideString(buttonTexts[i], wideButtonTexts[i]);

            taskDialogButtons[i].nButtonID = 100 + i;
            taskDialogButtons[i].pszButtonText = wideButtonTexts[i];
        }

        TASKDIALOGCONFIG config = {};
        config.cbSize = sizeof(config);
        config.hwndParent = nullptr;
        config.pszWindowTitle = wideTitle;

        switch (type)
        {
        case 0:
            config.pszMainIcon = TD_INFORMATION_ICON;
            break;
        case 1:
            config.pszMainIcon = TD_WARNING_ICON;
            break;
        case 2:
            config.pszMainIcon = TD_ERROR_ICON;
            break;
        }

        config.pszMainInstruction = wideTitle;
        config.pszContent = wideMessage;

        if (buttons > 0)
        {
            config.cButtons = buttons;
            config.pButtons = taskDialogButtons;
            config.nDefaultButton = taskDialogButtons[0].nButtonID;
        }
        else
        {
            config.dwCommonButtons = TDCBF_OK_BUTTON;
        }

        int clickedButtonId = 0;
        HRESULT hr = pTaskDialogIndirect(&config, &clickedButtonId, nullptr, nullptr);

        int buttonIndex = -1;

        if (SUCCEEDED(hr))
        {
            if (buttons > 0)
            {
                buttonIndex = clickedButtonId - 100;

                if (buttonIndex < 0 || buttonIndex >= buttons)
                {
                    buttonIndex = -1;
                }
            }
        }

        delete[] wideTitle;
        delete[] wideMessage;

        for (int i = 0; i < buttons; i++)
        {
            delete[] wideButtonTexts[i];
        }

        return buttonIndex;
    }
}
