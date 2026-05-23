#include "FocusClockApp.h"

void FocusClockApp::EnterFullscreenTopmost() {
    HMONITOR monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    GetMonitorInfoW(monitor, &info);
    RECT rc = info.rcMonitor;

    LONG_PTR style = GetWindowLongPtrW(hwnd_, GWL_STYLE);
    style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
    SetWindowLongPtrW(hwnd_, GWL_STYLE, style);

    SetWindowPos(hwnd_, HWND_TOPMOST, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);
    SetFocus(hwnd_);

    fullscreen_ = true;
    topmost_ = true;
    belowWindow_ = nullptr;
}

void FocusClockApp::EnterFullscreenNotTopmost() {
    HMONITOR monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    GetMonitorInfoW(monitor, &info);
    RECT rc = info.rcMonitor;

    LONG_PTR style = GetWindowLongPtrW(hwnd_, GWL_STYLE);
    style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
    SetWindowLongPtrW(hwnd_, GWL_STYLE, style);

    HWND insertAfter = fullscreen_ ? HWND_TOP : HWND_NOTOPMOST;
    SetWindowPos(hwnd_, insertAfter, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);
    SetFocus(hwnd_);

    fullscreen_ = true;
    topmost_ = false;
    belowWindow_ = nullptr;
}

void FocusClockApp::EnterFullscreenBelow(HWND aboveWindow) {
    HMONITOR monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    GetMonitorInfoW(monitor, &info);
    RECT rc = info.rcMonitor;

    LONG_PTR style = GetWindowLongPtrW(hwnd_, GWL_STYLE);
    style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
    SetWindowLongPtrW(hwnd_, GWL_STYLE, style);

    SetWindowPos(hwnd_, aboveWindow, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);
    SetFocus(hwnd_);

    fullscreen_ = true;
    topmost_ = false;
    belowWindow_ = aboveWindow;
}

void FocusClockApp::ApplyDarkMode() {
    BOOL enabled = darkMode_ ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd_, static_cast<DWORD>(kDwmwaUseImmersiveDarkMode), &enabled, sizeof(enabled));
}

void FocusClockApp::RefreshTheme() {
    bool next = IsSystemDarkMode();
    if (next == darkMode_) {
        return;
    }

    darkMode_ = next;
    ReleaseRenderResources();
    ApplyDarkMode();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

bool FocusClockApp::IsSystemDarkMode() const {
    DWORD value = 0;
    DWORD size = sizeof(value);
    HKEY key{};
    if (RegOpenKeyExW(HKEY_CURRENT_USER, LR"(Software\Microsoft\Windows\CurrentVersion\Themes\Personalize)", 0, KEY_QUERY_VALUE | KEY_WOW64_64KEY, &key) == ERROR_SUCCESS) {
        RegQueryValueExW(key, L"AppsUseLightTheme", nullptr, nullptr, reinterpret_cast<LPBYTE>(&value), &size);
        RegCloseKey(key);
    }
    return value != 1;
}

void FocusClockApp::RestoreAllWhitelistWindows() {
    for (const auto& [target, savedStyle] : promotedWindows_) {
        RestoreWhitelistWindow(target, savedStyle);
    }
    promotedWindows_.clear();
}

BOOL CALLBACK FocusClockApp::EnumWhitelistedWindowsForPromote(HWND window, LPARAM param) {
    auto* app = reinterpret_cast<FocusClockApp*>(param);
    if (!app || !window || window == app->hwnd_) {
        return TRUE;
    }

    if (!IsWindowVisible(window) || GetWindow(window, GW_OWNER)) {
        return TRUE;
    }

    std::wstring path = app->GetProcessImagePath(window);
    if (path.empty() || !app->IsExecutableWhitelisted(path)) {
        return TRUE;
    }

    app->PromoteWhitelistWindow(window);
    return TRUE;
}

void FocusClockApp::PromoteAllWhitelistWindows() {
    EnumWindows(EnumWhitelistedWindowsForPromote, reinterpret_cast<LPARAM>(this));
}