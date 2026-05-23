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

    // 始终移除 WS_EX_TOPMOST 扩展样式，确保窗口不在 TOPMOST 层级
    LONG_PTR exStyle = GetWindowLongPtrW(hwnd_, GWL_EXSTYLE);
    SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, exStyle & ~WS_EX_TOPMOST);

    // 使用 HWND_NOTOPMOST 确保窗口退出 TOPMOST 层级
    // 这样所有已提升为 WS_EX_TOPMOST 的白名单窗口都将显示在它之上
    SetWindowPos(hwnd_, HWND_NOTOPMOST, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_SHOWWINDOW);
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
    auto* windows = reinterpret_cast<std::vector<HWND>*>(param);
    if (!windows || !window) {
        return TRUE;
    }
    windows->push_back(window);
    return TRUE;
}

void FocusClockApp::PromoteAllWhitelistWindows() {
    // 收集所有白名单窗口
    std::vector<HWND> whitelistWindows;
    EnumWindows(EnumWhitelistedWindowsForPromote, reinterpret_cast<LPARAM>(&whitelistWindows));

    // 过滤出有效的白名单窗口
    std::vector<HWND> validWindows;
    for (HWND w : whitelistWindows) {
        if (!w || w == hwnd_) {
            continue;
        }
        if (!IsWindowVisible(w) || GetWindow(w, GW_OWNER)) {
            continue;
        }
        std::wstring path = GetProcessImagePath(w);
        if (path.empty() || !IsExecutableWhitelisted(path)) {
            continue;
        }
        validWindows.push_back(w);
    }

    // EnumWindows 从上到下枚举，所以 validWindows 中第一个是顶层窗口，最后是最底层窗口
    // 为了保持相对位置不变，从最底层窗口开始逐个提升
    // 这样最底层的窗口被提升到 TOPMOST 层最底部，
    // 最后一个被处理的窗口（原来的顶层窗口）成为 TOPMOST 层最顶部
    for (auto it = validWindows.rbegin(); it != validWindows.rend(); ++it) {
        PromoteWhitelistWindow(*it);
    }
}