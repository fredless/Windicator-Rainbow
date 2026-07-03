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

        constexpr auto KEY_PATH = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\VirtualDesktops";

        DWORD dwFilter = REG_NOTIFY_CHANGE_NAME |
                REG_NOTIFY_CHANGE_ATTRIBUTES |
                REG_NOTIFY_CHANGE_LAST_SET |
                REG_NOTIFY_CHANGE_SECURITY;

        REGSAM samDesired = KEY_READ | KEY_NOTIFY;
        HKEY hKey = nullptr;

        auto status = RegOpenKeyEx(HKEY_CURRENT_USER, KEY_PATH, 0, samDesired, &hKey);

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

        // The change notification gives an instant reaction when it works, but
        // a registration can be silently lost (key recreation, shell churn),
        // so the icon must never depend on it: every wake -- notification or
        // timeout -- re-reads the desktop number and posts on change.  Worst
        // case the icon lags one poll interval.  The bounded wait also means
        // the exit flag is always noticed, so shutdown is clean.
        constexpr DWORD POLL_INTERVAL_MS = 1000;

        UINT lastPosted = GetCurrentDesktopNumber(hKey);

        // Report the desktop we start on before waiting for changes.
        PostMessage(pData->hWnd, APP_WM_DESKTOP_CHANGE, 0, MAKELPARAM(lastPosted, 0));

        // Only one notification may be armed per set of parameters at a time;
        // re-arming while one is pending piles up registrations.
        BOOL armed = FALSE;

        while (true) {

            {
                std::lock_guard lock(pData->lock);
                if (!pData->keepGoing) {
                    break;
                }
            }

            if (!armed) {

                // Reopen the key if a previous pass dropped a stale handle.
                if (hKey == nullptr) {
                    if (RegOpenKeyEx(HKEY_CURRENT_USER, KEY_PATH, 0, samDesired, &hKey) != ERROR_SUCCESS) {
                        hKey = nullptr;
                    }
                }

                if (hKey != nullptr) {
                    ResetEvent(hEvent);

                    status = RegNotifyChangeKeyValue(hKey,
                            TRUE,
                            dwFilter,
                            hEvent,
                            TRUE);

                    if (status == ERROR_SUCCESS) {
                        armed = TRUE;
                    }
                    else {
                        // The handle has likely gone stale (e.g. the key was
                        // recreated).  Drop it; polling carries on regardless
                        // and the next pass reopens it.
                        RegCloseKey(hKey);
                        hKey = nullptr;
                    }
                }
            }

            // Un-armed this is simply the poll interval.
            auto result = WaitForSingleObject(hEvent, POLL_INTERVAL_MS);

            if (result == WAIT_OBJECT_0) {
                // Notification consumed; re-arm on the next pass.
                armed = FALSE;
            }
            else if (result == WAIT_FAILED) {
                Sleep(POLL_INTERVAL_MS);
                continue;
            }

            // GetCurrentDesktopNumber opens its own handle when given null.
            auto desktopNumber = GetCurrentDesktopNumber(hKey);

            if (desktopNumber != 0 && desktopNumber != lastPosted) {

                if (result == WAIT_TIMEOUT) {
                    // The desktop changed without a notification -- the
                    // registration is presumed dead, so rebuild it.
                    armed = FALSE;
                }

                lastPosted = desktopNumber;
                PostMessage(pData->hWnd, APP_WM_DESKTOP_CHANGE, 0, MAKELPARAM(desktopNumber, 0));
            }
        }

        CloseHandle(hEvent);

        if (hKey != nullptr) {
            RegCloseKey(hKey);
        }

        return TRUE;
    }

}