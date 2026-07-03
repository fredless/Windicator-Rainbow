#pragma once

#include "aboutdialog.h"
#include "BaseWindow.h"
#include "desktopWatcher.h"

class MainWindow : public BaseWindow<MainWindow> {
protected:
    void AmendWindowClass(WNDCLASSEXW* wc) override;

    [[nodiscard]] PCWSTR ClassName() const override;

    DesktopWatcher::DesktopWatcherData m_stWatcherData{};

public:
    MainWindow() = default;
    virtual ~MainWindow() = default;

    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) override;

    DWORD m_dwThreadId{};
    HANDLE m_hDesktopThread{};
    BOOL m_isVisible{FALSE};
    UINT m_uMsgTaskbarCreated{};
};
