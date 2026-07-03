#include "../include/framework.h"
#include "../include/MainWindow.h"
#include "../resources/resource.h"

#include <CommCtrl.h>
#include <memory>

/// @brief Application entry point
/// @return result
INT WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nShowCmd)
{
    HANDLE mutex = CreateMutex(nullptr, TRUE, L"Windicator_Instance");

    if (mutex != nullptr && GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBox(nullptr, L"An instance of Windicator is already running", L"Windicator", MB_ICONEXCLAMATION);
        return 8;
    }

    // The about dialog uses a SysLink control, which is only registered
    // once common controls are initialized with ICC_LINK_CLASS.
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_LINK_CLASS };
    InitCommonControlsEx(&icc);

    wchar_t szWindowName[MAX_LOAD_STRING];

    LoadString(hInstance, IDS_APP_TITLE, szWindowName, MAX_LOAD_STRING);

    std::unique_ptr<MainWindow> mainWindow = std::make_unique<MainWindow>();

    if (!mainWindow->Create(szWindowName, WS_OVERLAPPED)) {
        MessageBox(nullptr, L"Failed to create the main window", L"Windicator", MB_ICONERROR);
        return 1;
    }

    mainWindow->Show(SW_HIDE);

    // auto* const hAccelerators =
    //     LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_MAIN_MENU));

    MSG msg = {};
    BOOL bRet;

    while ((bRet = GetMessage(&msg, nullptr, 0, 0)) != 0) {
        // GetMessage returns -1 on error; treating it as a normal message
        // would dispatch garbage.
        if (bRet == -1) {
            break;
        }

        // if (!TranslateAccelerator(msg.hwnd, hAccelerators, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        // }
    }

    if (mutex != nullptr) {
        ReleaseMutex(mutex);
        CloseHandle(mutex);
    }

    return 0;
}
