#include "../include/framework.h"
#include "../include/MainWindow.h"
#include "../resources/resource.h"
#include "../include/Config.h"

#include <CommCtrl.h>
#include <string>
#include <memory>
#include <map>
#include <sstream>
#include <vector>

/// @brief Application entry point
/// @return result
INT WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nShowCmd)
{
    std::wstring mutexName = L"Windicator_Instance";

    HANDLE mutex = CreateMutex(nullptr, TRUE, mutexName.c_str());

    if (mutex != nullptr && GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBox(nullptr, L"An instance of Windicator is already running", L"Windicator", MB_ICONEXCLAMATION);
        return 8;
    }

    // The about dialog uses a SysLink control, which is only registered
    // once common controls are initialized with ICC_LINK_CLASS.
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_LINK_CLASS };
    InitCommonControlsEx(&icc);

    std::wstringstream wss(lpCmdLine);
    std::wstring arg;
    std::vector<std::wstring> args;
    while (wss >> arg) {
        args.push_back(arg);
    }

    // default to small white icons
    auto iconOffset = IDI_SMALL_START;

    std::vector<UINT> iconChoices = {
        IDI_SMALL_START,
        IDI_BLUE_SMALL_START,
        IDI_DARK_SMALL_START
    };

    auto iconChoice = std::find(args.begin(), args.end(), L"--blue") != args.end() ? 1 : 0;
    iconChoice = std::find(args.begin(), args.end(), L"--dark") != args.end() ? 2 : iconChoice;

    iconOffset = iconChoices[iconChoice];

    wchar_t szWindowName[MAX_LOAD_STRING];

    LoadString(hInstance, IDS_APP_TITLE, szWindowName, MAX_LOAD_STRING);

    std::shared_ptr<Config> config = std::make_shared<Config>();
    config->iconOffset = iconOffset;

    std::unique_ptr<MainWindow> mainWindow = std::make_unique<MainWindow>(config);

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
