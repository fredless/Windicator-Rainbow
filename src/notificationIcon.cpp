#include "../include/notificationIcon.h"
#include "../include/messages.h"
#include "../resources/resource.h"

#include <shellapi.h>
#include <strsafe.h>
#include <memory>

namespace NotificationIcon {

    NOTIFYICONDATA nid = {};
    HMENU hMenuTrackPopup;
    HMENU hNotifyMenu;

    /// @brief Load the icon for a desktop number, falling back to the "X" icon
    /// @param hInst The application instance
    /// @param nDesktop The current desktop number
    /// @return icon handle
    static HICON LoadDesktopIcon(HINSTANCE hInst, UINT nDesktop)
    {
        // Numbered icons run from IDI_SMALL_START + 1 through + 10 (the "0"
        // icon stands in for desktop 10).  There is no icon at + 0, so an
        // unknown desktop (0) or one past 10 gets the "X" icon at + 11.
        if (nDesktop < 1 || nDesktop > 10) {
            nDesktop = 11;
        }

        return LoadIcon(hInst, MAKEINTRESOURCE(IDI_SMALL_START + nDesktop));
    }

    /// @brief Add the notification icon to the system tray
    /// @param hInst The application instance
    /// @param hWndMain The parent window handle
    /// @param nDesktop The current desktop number
    /// @return result
    HRESULT Add(HINSTANCE hInst, HWND hWndMain, UINT nDesktop)
    {
        nid.cbSize = sizeof(nid);
        nid.hWnd = hWndMain;
        nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;

        WCHAR szTitle[ARRAYSIZE(nid.szTip)];
        LoadStringW(hInst, IDS_APP_TITLE, szTitle, ARRAYSIZE(szTitle));
        StringCchCopy(nid.szTip, ARRAYSIZE(nid.szTip), szTitle);

        nid.hIcon = LoadDesktopIcon(hInst, nDesktop);
        nid.uCallbackMessage = APP_WM_ICON_NOTIFY;

        // During logon the shell can be too busy to accept the icon and
        // NIM_ADD fails with ERROR_TIMEOUT, so retry a few times.  The add
        // may also have succeeded even though the call reported a timeout,
        // or the icon may still be registered from a previous shell session,
        // so treat a successful NIM_MODIFY as success too.
        DWORD lastError = ERROR_SUCCESS;

        for (int attempt = 0; attempt < 5; attempt++) {
            if (attempt > 0) {
                Sleep(1000);
            }

            if (Shell_NotifyIcon(NIM_ADD, &nid)) {
                return S_OK;
            }

            lastError = GetLastError();

            if (Shell_NotifyIcon(NIM_MODIFY, &nid)) {
                return S_OK;
            }

            if (lastError != ERROR_TIMEOUT) {
                break;
            }
        }

        wchar_t err[256] = {};
        FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_FROM_HMODULE,
                nullptr, lastError,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), err, ARRAYSIZE(err) - 1, nullptr);

        OutputDebugString(err);

        return E_FAIL;
    }

    /// @brief Modify an existing notification icon
    /// @param hInst The application instance
    /// @param hWndMain The parent window handle
    /// @param nDesktop The current desktop number
    /// @return result
    HRESULT Modify(HINSTANCE hInst, HWND hWndMain, UINT nDesktop)
    {
        nid.uFlags = NIF_ICON;

        nid.hIcon = LoadDesktopIcon(hInst, nDesktop);

        auto result = Shell_NotifyIcon(NIM_MODIFY, &nid) ? S_OK : E_FAIL;

        return result;
    }

    /// @brief remove the notification icon from the tray
    /// @return result from Shell_NotifyIcon
    HRESULT Remove()
    {
        return Shell_NotifyIcon(NIM_DELETE, &nid) ? S_OK : E_FAIL;
    }

    /// @brief Display the notification icon context menu
    /// @param hInst Application instance handle
    /// @param hWnd Parent window handle
    /// @param point The location to show the menu
    VOID APIENTRY DisplayContextMenu(HINSTANCE hInst, HWND hWnd, POINT point)
    {
        if ((hNotifyMenu = LoadMenu(hInst, MAKEINTRESOURCE(IDR_NOTIFY_MENU))) ==
                nullptr)
            return;

        // TrackPopupMenu cannot display the menu bar so get
        // a handle to the first shortcut menu.
        hMenuTrackPopup = GetSubMenu(hNotifyMenu, 0);

        if (hMenuTrackPopup == nullptr) {
            DestroyMenu(hNotifyMenu);
            return;
        }

        SetForegroundWindow(hWnd);

        // Display the shortcut menu. Track the right mouse
        // button.
        TrackPopupMenu(hMenuTrackPopup,
                TPM_BOTTOMALIGN | TPM_RIGHTALIGN | TPM_RIGHTBUTTON, point.x,
                point.y, 0, hWnd, nullptr);

        // Required after TrackPopupMenu on a notification icon so the menu
        // dismisses when the user clicks elsewhere (see KB135788).
        PostMessage(hWnd, WM_NULL, 0, 0);

        DestroyMenu(hNotifyMenu);
    }

    /// @brief The notification icon window proc
    /// @return result
    INT_PTR CALLBACK WndProc(HINSTANCE hInst, HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (lParam) {
            case WM_LBUTTONDBLCLK:
// Disabled for now.  Might be re-enabled if configuration options are added.
//                SendMessage(hWnd, WM_COMMAND,
//                        MAKELPARAM(IDM_NOTIFY_TOGGLE_VISIBILITY, HIWORD(wParam)),
//                        lParam);
                break;
            case WM_RBUTTONUP:
                POINT pt;
                GetCursorPos(&pt);
                DisplayContextMenu(hInst, hWnd, pt);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
        }

        return 0;
    }
}