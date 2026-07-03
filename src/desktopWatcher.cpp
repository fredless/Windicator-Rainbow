#include "../include/framework.h"
#include "../include/desktopWatcher.h"
#include "../include/messages.h"

#include <vector>

namespace DesktopWatcher {

    /// @brief Display a MessageBox with the translated error from errorCode
    /// @param hParent Parent window handle
    /// @param errorCode Error code to translate
    VOID ShowErrorMessageBox(HWND hParent, DWORD errorCode)
    {
        wchar_t errorMessage[256] = {};
        // nSize is in characters, not bytes -- passing sizeof() here lets
        // FormatMessage overrun the buffer on long messages.
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_FROM_HMODULE,
                nullptr, errorCode,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), errorMessage, ARRAYSIZE(errorMessage) - 1, nullptr);

        OutputDebugString(errorMessage);
        MessageBox(hParent, errorMessage, L"Windicator Error", MB_ICONERROR);
    }

    /// @brief Get the current desktop number
    /// @param hKey handle to key or nullptr to allocate local handle
    /// @return Desktop Number (0 when it cannot be determined)
    UINT GetCurrentDesktopNumber(HKEY hKey)
    {
        BYTE buffer[4096] = {};
        PVOID pvData = buffer;
        DWORD size = sizeof(buffer);
        LONG status{};
        BOOL localHandle = (hKey == nullptr);

        if (localHandle) {

            REGSAM samDesired = KEY_READ | KEY_NOTIFY;

            status = RegOpenKeyEx(
                    HKEY_CURRENT_USER,
                    L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\VirtualDesktops",
                    0,
                    samDesired,
                    &hKey
            );

            if (ERROR_SUCCESS != status) {
                return 0;
            }
        }

        // Get the current desktop ids
        status = RegGetValue(
                hKey,
                nullptr,
                L"VirtualDesktopIds",
                RRF_RT_REG_BINARY,
                nullptr,
                pvData,
                &size
        );

        if (ERROR_SUCCESS != status) {
            // The value does not exist until more than one desktop has been
            // created, so its absence means we are on the only desktop.
            if (localHandle) {
                RegCloseKey(hKey);
            }
            return 1;
        }

        std::vector<GUID> desktops;

        // create list of desktop GUIDS using the binary data retrieved from the registry
        for (size_t i = 0; i < size / 16; i++) {

            GUID desktopId;
            memcpy(&desktopId, &static_cast<BYTE*>(pvData)[i * 16], 16);

            desktops.push_back(desktopId);
        }

        // get the current virtual desktop id
        GUID currentDesktopId = {};
        size = sizeof(currentDesktopId);

        status = RegGetValue(
                hKey,
                nullptr,
                L"CurrentVirtualDesktop",
                RRF_RT_REG_BINARY,
                nullptr,
                &currentDesktopId,
                &size
        );

        if (localHandle) {
            RegCloseKey(hKey);
        }

        if (ERROR_SUCCESS != status || size != sizeof(currentDesktopId)) {
            // The value is absent until the first desktop switch after logon,
            // which leaves the session on the first desktop.
            return 1;
        }

        // check which GUID in desktops matches the current desktop and send
        // a message to the main window proc containing the desktop number in the
        // low word of the LPARAM.
        UINT idx = 1;
        UINT desktopNumber = 1;
        for (auto& desktopId: desktops) {
            if (desktopId == currentDesktopId) {
                desktopNumber = idx;
            }
            idx++;
        }

        return desktopNumber;
    }

    /// @brief Thread to watch for registry changes related to Virtual Desktops
    /// @param lParam Pointer to a DesktopWatcher structure.
    /// @return result
    DWORD DesktopWatcherThreadProc(LPVOID lParam)
    {
        auto* const pData = static_cast<DesktopWatcherData*>(lParam);

        DWORD dwFilter = REG_NOTIFY_CHANGE_NAME |
                REG_NOTIFY_CHANGE_ATTRIBUTES |
                REG_NOTIFY_CHANGE_LAST_SET |
                REG_NOTIFY_CHANGE_SECURITY;

        REGSAM samDesired = KEY_READ | KEY_NOTIFY;
        HKEY hKey = nullptr;

        auto status = RegOpenKeyEx(
                HKEY_CURRENT_USER,
                L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\VirtualDesktops",
                0,
                samDesired,
                &hKey
        );

        if (status != ERROR_SUCCESS) {
            ShowErrorMessageBox(pData->hWnd, status);
            return FALSE;
        }

        // A single manual-reset event, reused for every notification.
        auto hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

        if (hEvent == nullptr) {
            ShowErrorMessageBox(pData->hWnd, GetLastError());
            RegCloseKey(hKey);
            return FALSE;
        }

        // Transient failures are retried; only a persistent failure streak
        // stops the watcher, so one hiccup does not freeze the icon for the
        // rest of the session.
        constexpr UINT MAX_CONSECUTIVE_ERRORS = 10;
        UINT consecutiveErrors = 0;

        // Report the desktop we start on before waiting for changes.
        PostMessage(pData->hWnd, APP_WM_DESKTOP_CHANGE, 0,
                MAKELPARAM(GetCurrentDesktopNumber(hKey), 0));

        // Only one notification may be armed per set of parameters at a time;
        // re-arming while one is pending piles up registrations.
        BOOL armed = FALSE;

        while (true) {

            // This will probably only trigger if _TIDY_TIMEOUT is defined since we are likely
            // waiting for a registry change event when the parent process sets keepGoing to FALSE
            // and terminates.
            {
                std::lock_guard lock(pData->lock);
                if (!pData->keepGoing) {
                    break;
                }
            }

            if (!armed) {
                ResetEvent(hEvent);

                // Watch the registry key for a change of buffer.
                status = RegNotifyChangeKeyValue(hKey,
                        TRUE,
                        dwFilter,
                        hEvent,
                        TRUE);

                if (status != ERROR_SUCCESS) {
                    if (++consecutiveErrors >= MAX_CONSECUTIVE_ERRORS) {
                        ShowErrorMessageBox(pData->hWnd, status);
                        break;
                    }
                    Sleep(1000);
                    continue;
                }

                armed = TRUE;
            }

            // This is an optional build definition to use a time-out to make sure we can catch the program exit
            // so the registry handle can be closed.  It is probably not needed since this app will likely
            // run the duration of your session so leaking a single registry handle is not an issue.  The system
            // would eventually clean it up anyway.  If you want to use it uncomment the _TIDY_TIMEOUT definition
            // in the CMakeLists.txt before you build.  It will create a tiny increase in CPU usage since the
            // thread will loop instead of an infinite wait event for a registry change.
#ifdef _TIDY_TIMEOUT
            auto timeout = 500;
#else
            auto timeout = INFINITE;
#endif
            auto result = WaitForSingleObject(hEvent, timeout);

            if (result == WAIT_TIMEOUT) {
                continue;
            }

            if (result != WAIT_OBJECT_0) {
                if (++consecutiveErrors >= MAX_CONSECUTIVE_ERRORS) {
                    ShowErrorMessageBox(pData->hWnd, GetLastError());
                    break;
                }
                Sleep(1000);
                continue;
            }

            armed = FALSE;
            consecutiveErrors = 0;

            auto desktopNumber = GetCurrentDesktopNumber(hKey);
            PostMessage(pData->hWnd, APP_WM_DESKTOP_CHANGE, 0, MAKELPARAM(desktopNumber, 0));
        }

        // We most likely will not hit this unless _TIDY_TIMEOUT is defined at compile time or
        // an error condition occurred.
        CloseHandle(hEvent);
        RegCloseKey(hKey);

        return TRUE;
    }

}