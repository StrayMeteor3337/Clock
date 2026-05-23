#include "FocusClockApp.h"

// ========== Free functions (globally accessible) ==========

std::wstring CurrentExecutableDirectory() {
    std::wstring path(MAX_PATH, L'\0');
    DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    while (length == path.size()) {
        path.resize(path.size() * 2);
        length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    }

    if (length == 0) {
        return L".";
    }

    path.resize(length);
    size_t slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos) {
        return L".";
    }
    return path.substr(0, slash);
}

long long CurrentUnixSeconds() {
    FILETIME nowFileTime{};
    GetSystemTimeAsFileTime(&nowFileTime);
    ULARGE_INTEGER value{};
    value.LowPart = nowFileTime.dwLowDateTime;
    value.HighPart = nowFileTime.dwHighDateTime;
    constexpr ULONGLONG unixEpochOffset = 116444736000000000ULL;
    if (value.QuadPart < unixEpochOffset) {
        return 0;
    }
    return static_cast<long long>((value.QuadPart - unixEpochOffset) / 10000000ULL);
}

bool HasCommandLineSwitch(const wchar_t* target) {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) {
        return false;
    }

    bool found = false;
    for (int i = 1; i < argc; ++i) {
        if (_wcsicmp(argv[i], target) == 0) {
            found = true;
            break;
        }
    }

    LocalFree(argv);
    return found;
}

long long RerunRemainingSecondsFromSettings() {
    std::wstring settingsPath = CurrentExecutableDirectory() + L"\\FocusClock.ini";
    int active = GetPrivateProfileIntW(L"FocusSession", L"Active", 0, settingsPath.c_str());
    if (active != 1) {
        return 0;
    }

    wchar_t buffer[64]{};
    GetPrivateProfileStringW(L"FocusSession", L"StartUnix", L"0", buffer, static_cast<DWORD>(std::size(buffer)), settingsPath.c_str());
    long long startUnix = _wtoi64(buffer);

    GetPrivateProfileStringW(L"FocusSession", L"EndUnix", L"0", buffer, static_cast<DWORD>(std::size(buffer)), settingsPath.c_str());
    long long endUnix = _wtoi64(buffer);

    long long now = CurrentUnixSeconds();
    if (startUnix <= 0 || endUnix <= startUnix || now < startUnix || now >= endUnix) {
        return 0;
    }

    return std::max(1LL, endUnix - now);
}

bool IsValidStoredScheduleRange(int startMinute, int endMinute) {
    return startMinute >= 0 && startMinute < 24 * 60 &&
        endMinute >= 0 && endMinute < 24 * 60 &&
        startMinute != endMinute;
}

bool IsMinuteInStoredScheduleRange(int minuteOfDay, int startMinute, int endMinute) {
    if (endMinute > startMinute) {
        return minuteOfDay >= startMinute && minuteOfDay < endMinute;
    }
    if (endMinute < startMinute) {
        return minuteOfDay >= startMinute || minuteOfDay < endMinute;
    }
    return false;
}

bool HasActiveScheduledFocusRange() {
    std::wstring settingsPath = CurrentExecutableDirectory() + L"\\FocusClock.ini";
    int count = GetPrivateProfileIntW(L"Schedule", L"Count", 0, settingsPath.c_str());
    count = std::clamp(count, 0, 64);

    SYSTEMTIME now{};
    GetLocalTime(&now);
    int minuteOfDay = now.wHour * 60 + now.wMinute;

    for (int i = 0; i < count; ++i) {
        std::wstring key = L"Task" + std::to_wstring(i);
        wchar_t buffer[64]{};
        GetPrivateProfileStringW(L"Schedule", key.c_str(), L"", buffer, static_cast<DWORD>(std::size(buffer)), settingsPath.c_str());

        std::wstring value = buffer;
        size_t comma = value.find(L',');
        if (comma == std::wstring::npos) {
            continue;
        }

        size_t secondComma = value.find(L',', comma + 1);
        int startMinute = _wtoi(value.substr(0, comma).c_str());
        int endMinute = _wtoi(value.substr(comma + 1, secondComma == std::wstring::npos ? std::wstring::npos : secondComma - comma - 1).c_str());
        if (IsValidStoredScheduleRange(startMinute, endMinute) &&
            IsMinuteInStoredScheduleRange(minuteOfDay, startMinute, endMinute)) {
            return true;
        }
    }

    return false;
}

LRESULT FocusClockApp::HandleMessage(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
    case WM_CREATE:
        return 0;

    case WM_SIZE:
        RebuildLayout();
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_DISPLAYCHANGE:
        if (focusActive_) {
            EnterFullscreenTopmost();
        } else {
            EnterFullscreenNotTopmost();
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_SETTINGCHANGE:
    case WM_THEMECHANGED:
        RefreshTheme();
        ApplyDarkMode();
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case kRerunTaskCommandFinishedMessage:
        HandleRerunTaskCommandResult(wparam, lparam);
        return 0;

    case WM_TIMER:
        if (wparam == kClockTimer) {
            UpdateRemaining();
            if (remainingSeconds_ <= 0 && focusActive_) {
                FinishFocus();
            }
            InvalidateRect(hwnd, nullptr, FALSE);
        } else if (wparam == kGuardTimer) {
            if (LoadWhitelistIfNeeded()) {
                RebuildLayout();
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            CheckScheduledFocusTasks();
            if (focusActive_) {
                TryResolvePendingWhitelistWindow();
                IsWhitelistedForegroundWindow();
                // 只要有任意白名单窗口存在，就保持非 TOPMOST 模式，
                // 使所有白名单窗口都能正常显示和交互
                if (ShouldYieldToWhitelist() || HasAnyWhitelistWindowVisible()) {
                    PromoteAllWhitelistWindows();
                    EnterFullscreenNotTopmost();
                } else {
                    activeWhitelistWindow_ = nullptr;
                    EnterFullscreenTopmost();
                }
            }
            RefreshTheme();
        } else if (wparam == kWhitelistLayoutTimer) {
            ProcessPendingWhitelistLayout();
        }
        return 0;

    case WM_WINDOWPOSCHANGING:
        // Disabled: forcing Z-order here interferes with normal window actions
        // such as minimizing whitelisted applications.
        //
        // if (focusActive_) {
        //     auto* pos = reinterpret_cast<WINDOWPOS*>(lparam);
        //     if (IsTrackedWhitelistWindowValid()) {
        //         pos->hwndInsertAfter = activeWhitelistWindow_;
        //     } else {
        //         pos->hwndInsertAfter = HWND_TOPMOST;
        //     }
        //     pos->flags &= ~SWP_NOZORDER;
        // }
        return DefWindowProcW(hwnd, msg, wparam, lparam);

    case WM_KEYDOWN:
        if (wparam == VK_F6) {
            if (!focusActive_) {
                TogglePanel();
            } else {
                MessageBeep(MB_ICONINFORMATION);
            }
            return 0;
        }

        if (panelOpen_) {
            if (wparam == VK_ESCAPE) {
                ClosePanel();
            }
            return 0;
        }

        if (IsShiftDown()) {
            bool handled = true;
            switch (wparam) {
            case VK_LEFT:
                MoveWhitelistLayout(-kWhitelistMoveStep, 0);
                break;
            case VK_RIGHT:
                MoveWhitelistLayout(kWhitelistMoveStep, 0);
                break;
            case VK_UP:
                MoveWhitelistLayout(0, -kWhitelistMoveStep);
                break;
            case VK_DOWN:
                MoveWhitelistLayout(0, kWhitelistMoveStep);
                break;
            case VK_OEM_PLUS:
            case VK_ADD:
                ResizeWhitelistLayout(kWhitelistResizeStep);
                break;
            case VK_OEM_MINUS:
            case VK_SUBTRACT:
                ResizeWhitelistLayout(-kWhitelistResizeStep);
                break;
            default:
                handled = false;
                break;
            }
            if (handled) {
                return 0;
            }
        }

        if (wparam == VK_ESCAPE) {
            if (!focusActive_) {
                DestroyWindow(hwnd);
            }
            return 0;
        }
        return 0;

    case WM_HOTKEY:
        if (wparam == kPanelHotkeyId) {
            if (!focusActive_) {
                TogglePanel();
                ShowWindow(hwnd_, SW_SHOW);
                SetForegroundWindow(hwnd_);
            } else {
                MessageBeep(MB_ICONINFORMATION);
            }
            return 0;
        }
        return 0;

    case WM_SYSKEYDOWN:
        if (wparam == VK_F4 && focusActive_) {
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wparam, lparam);

    case WM_LBUTTONDOWN: {
        if (panelOpen_) {
            return 0;
        }
        POINT pt{ GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
        if (BeginWhitelistDrag(pt)) {
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wparam, lparam);
    }

    case WM_MOUSEMOVE: {
        if (whitelistDragging_) {
            POINT pt{ GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
            UpdateWhitelistDrag(pt);
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wparam, lparam);
    }

    case WM_LBUTTONUP: {
        POINT pt{ GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
        if (whitelistDragging_) {
            bool moved = whitelistDragMoved_;
            EndWhitelistDrag();
            if (moved) {
                return 0;
            }
        }
        HitTestAndClick(pt);
        return 0;
    }

    case WM_MOUSEWHEEL: {
        if (panelOpen_) {
            if (activePanelTab_ == kPanelTabScheduleId) {
                int delta = GET_WHEEL_DELTA_WPARAM(wparam) > 0 ? -48 : 48;
                ScrollScheduleTab(delta);
            } else if (activePanelTab_ == kPanelTabWhitelistId) {
                int delta = GET_WHEEL_DELTA_WPARAM(wparam) > 0 ? -48 : 48;
                ScrollWhitelistTab(delta);
            }
            return 0;
        }
        if (IsShiftDown()) {
            POINT pt{ GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
            ScreenToClient(hwnd, &pt);
            if (IsPointInWhitelistArea(pt)) {
                int delta = GET_WHEEL_DELTA_WPARAM(wparam) > 0 ? kWhitelistResizeStep : -kWhitelistResizeStep;
                ResizeWhitelistLayout(delta);
                return 0;
            }
        }
        return DefWindowProcW(hwnd, msg, wparam, lparam);
    }

    case WM_PAINT:
        Paint();
        return 0;

    case WM_CLOSE:
        if (focusActive_) {
            MessageBeep(MB_ICONINFORMATION);
            return 0;
        }
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        ProcessPendingWhitelistLayout(true);
        KillTimer(hwnd, kWhitelistLayoutTimer);
        RemoveFocusKeyboardHook();
        UnregisterHotKey(hwnd, kPanelHotkeyId);
        KillTimer(hwnd, kClockTimer);
        KillTimer(hwnd, kGuardTimer);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

// ========== Entry point ==========

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    SetProcessDPIAware();

    // 初始化公共控件库（SHGetImageList 需要 comctl32.dll 版本 6）
    INITCOMMONCONTROLSEX icc = {};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_STANDARD_CLASSES | ICC_USEREX_CLASSES;
    InitCommonControlsEx(&icc);

    long long rerunResumeSeconds = 0;
    if (HasCommandLineSwitch(L"-rerun")) {
        rerunResumeSeconds = RerunRemainingSecondsFromSettings();
        if (rerunResumeSeconds <= 0 && !HasActiveScheduledFocusRange()) {
            return 0;
        }
    }

    FocusClockApp app;
    return app.Run(instance, show, rerunResumeSeconds);
}