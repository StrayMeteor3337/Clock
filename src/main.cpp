#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include <shellapi.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cwctype>
#include <map>
#include <string>
#include <utility>
#include <vector>

using namespace Gdiplus;

namespace {

constexpr UINT_PTR kClockTimer = 1;
constexpr UINT_PTR kGuardTimer = 2;
constexpr UINT_PTR kWhitelistLayoutTimer = 3;
constexpr int kClockTimerMs = 250;
constexpr int kGuardTimerMs = 1000;
constexpr int kWhitelistLayoutTimerMs = 2;
constexpr DWORD kWhitelistLayoutSaveDelayMs = 700;
constexpr double kPi = 3.14159265358979323846;
constexpr int kMinFocusMinutes = 1;
constexpr int kDefaultMaxFocusMinutes = 240;
constexpr int kAbsoluteMaxFocusMinutes = 1440;
constexpr int kDefaultFocusMinutes = 25;
constexpr int kStartButtonId = 1001;
constexpr int kExitButtonId = 1002;
constexpr int kCustomMinusFiveId = 1101;
constexpr int kCustomMinusOneId = 1102;
constexpr int kCustomPlusOneId = 1103;
constexpr int kCustomPlusFiveId = 1104;
constexpr int kCustomDisplayId = 1105;
constexpr int kWhitelistButtonBaseId = 2000;
constexpr int kWhitelistButtonLimit = 3000;
constexpr int kPanelButtonBaseId = 3000;
constexpr int kPanelCloseButtonId = 3001;
constexpr int kPanelResetSettingsId = 3002;
constexpr int kPanelTabButtonBaseId = 3100;
constexpr int kPanelTabOverviewId = 1;
constexpr int kPanelTabSettingsId = 2;
constexpr int kPanelTabScheduleId = 3;
constexpr int kPanelTabRerunId = 4;
constexpr int kSettingMaxMinusFiveId = 3201;
constexpr int kSettingMaxMinusOneId = 3202;
constexpr int kSettingMaxDisplayId = 3203;
constexpr int kSettingMaxPlusOneId = 3204;
constexpr int kSettingMaxPlusFiveId = 3205;
constexpr int kSettingDefaultMinusFiveId = 3211;
constexpr int kSettingDefaultMinusOneId = 3212;
constexpr int kSettingDefaultDisplayId = 3213;
constexpr int kSettingDefaultPlusOneId = 3214;
constexpr int kSettingDefaultPlusFiveId = 3215;
constexpr int kScheduleStartHourMinusId = 3301;
constexpr int kScheduleStartHourDisplayId = 3302;
constexpr int kScheduleStartHourPlusId = 3303;
constexpr int kScheduleStartMinuteMinusId = 3304;
constexpr int kScheduleStartMinuteDisplayId = 3305;
constexpr int kScheduleStartMinutePlusId = 3306;
constexpr int kScheduleEndHourMinusId = 3311;
constexpr int kScheduleEndHourDisplayId = 3312;
constexpr int kScheduleEndHourPlusId = 3313;
constexpr int kScheduleEndMinuteMinusId = 3314;
constexpr int kScheduleEndMinuteDisplayId = 3315;
constexpr int kScheduleEndMinutePlusId = 3316;
constexpr int kScheduleAddTaskId = 3320;
constexpr int kScheduleDeleteButtonBaseId = 3400;
constexpr int kScheduleDeleteButtonLimit = 3600;
constexpr int kRerunCreateTaskId = 3601;
constexpr int kPanelHotkeyId = 4001;
constexpr int kMinWhitelistIconSize = 48;
constexpr int kMaxWhitelistIconSize = 120;
constexpr int kDefaultWhitelistIconSize = 72;
constexpr int kWhitelistMoveStep = 10;
constexpr int kWhitelistResizeStep = 6;

constexpr DWORD kDwmwaUseImmersiveDarkMode = 20;

struct Theme {
    Color background;
    Color panel;
    Color primaryText;
    Color secondaryText;
    Color mutedText;
    Color accent;
    Color accentSoft;
    Color danger;
    Color line;
};

struct UiButton {
    RECT rect{};
    std::wstring label;
    std::wstring iconPath;
    int id = 0;
    bool enabled = true;
    bool selected = false;
    bool primary = false;
    bool danger = false;
    bool iconOnly = false;
};

struct PanelTabDefinition {
    int id = 0;
    const wchar_t* title = L"";
};

struct WhitelistEntry {
    std::wstring launchSpec;
    std::wstring normalized;
    std::wstring exeName;
    std::wstring iconPath;
    std::wstring label;
};

struct ScheduledFocusTask {
    int startMinute = 9 * 60;
    int endMinute = 9 * 60 + kDefaultFocusMinutes;
    int lastStartedDate = 0;
};

class FocusClockApp;

struct FindWindowContext {
    const FocusClockApp* app = nullptr;
    const WhitelistEntry* entry = nullptr;
    HWND found = nullptr;
};

class FocusClockApp {
public:
    int Run(HINSTANCE instance, int show, long long rerunResumeSeconds = 0);

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
    static LRESULT CALLBACK KeyboardHookProc(int code, WPARAM wparam, LPARAM lparam);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

    bool Create(HINSTANCE instance, int show);
    void InstallFocusKeyboardHook();
    void RemoveFocusKeyboardHook();
    bool ShouldBlockFocusShortcut(DWORD vkCode) const;
    void EnterFullscreenTopmost();
    void EnterFullscreenNotTopmost();
    void EnterFullscreenBelow(HWND aboveWindow);
    void ApplyDarkMode();
    void RefreshTheme();
    bool LoadWhitelistIfNeeded(bool force = false);
    void StartFocus();
    void StartFocusForSeconds(long long durationSeconds, bool updateSessionSettings = true);
    void FinishFocus();
    void RebuildLayout();
    void Paint();
    void DrawBackground(Graphics& g, const RECT& rc);
    void DrawAnalogClock(Graphics& g, const RectF& bounds, const SYSTEMTIME& now);
    void DrawHand(Graphics& g, const PointF& center, float radius, double angleRadians, float width, const Color& color);
    void DrawTextBlock(Graphics& g, const std::wstring& text, const RectF& rect, Font& font, const Color& color, StringAlignment align, StringAlignment lineAlign);
    void DrawButton(Graphics& g, const UiButton& button);
    void DrawButtonIcon(Graphics& g, const UiButton& button, const RectF& bounds);
    void HitTestAndClick(POINT pt);
    bool BeginWhitelistDrag(POINT pt);
    void UpdateWhitelistDrag(POINT pt);
    void EndWhitelistDrag();
    bool IsShiftDown() const;
    bool IsPointInWhitelistArea(POINT pt) const;
    void MoveWhitelistLayout(int dx, int dy);
    void ResizeWhitelistLayout(int delta);
    void RebuildWhitelistLayoutOnly();
    void MarkWhitelistLayoutChanged(bool needsRebuild);
    void ProcessPendingWhitelistLayout(bool forceSave = false);
    void InvalidateWhitelistArea(const RECT& previousBounds);
    void ClampWhitelistLayout(int clientWidth, int clientHeight);
    void LoadWhitelistLayoutSettings();
    void SaveWhitelistLayoutSettings() const;
    void LoadAppSettings();
    void SaveAppSettings() const;
    void ResetAppSettings();
    void SaveFocusSessionSettings(long long durationSeconds) const;
    void ClearFocusSessionSettings() const;
    void TogglePanel();
    void ClosePanel();
    void DrawPanel(Graphics& g, const RECT& rc);
    void DrawOverviewTab(Graphics& g, const RectF& contentRect, const Theme& theme, FontFamily& family);
    void DrawSettingsTab(Graphics& g, const RectF& contentRect, const Theme& theme, FontFamily& family);
    void DrawScheduleTab(Graphics& g, const RectF& contentRect, const Theme& theme, FontFamily& family);
    void DrawRerunTab(Graphics& g, const RectF& contentRect, const Theme& theme, FontFamily& family);
    void AddPanelButton(int id, const RECT& rect, const std::wstring& label, bool selected = false, bool enabled = true, bool primary = false, bool danger = false);
    void AddStepperButtons(int minusFiveId, int minusOneId, int displayId, int plusOneId, int plusFiveId, int left, int top, int value, const std::wstring& suffix, bool canDecrease, bool canIncrease);
    void AddTimeStepperButtons(int minusId, int displayId, int plusId, int left, int top, int value, int maxValue, const std::wstring& suffix);
    void HandlePanelCommand(int id);
    void SetMaxFocusMinutes(int minutes);
    void SetDefaultFocusMinutes(int minutes);
    void AdjustScheduleDraft(int id);
    void AddScheduledFocusTask();
    void DeleteScheduledFocusTask(size_t index);
    void ScrollScheduleTab(int delta);
    int ScheduleContentMaxScroll() const;
    RECT PanelContentViewport() const;
    void LoadScheduledFocusTasks();
    void SaveScheduledFocusTasks() const;
    void CheckScheduledFocusTasks(bool forceResumeActiveRange = false);
    void CreateRerunStartupTask();
    bool HasScheduleConflict(int startMinute, int endMinute, int ignoreIndex = -1) const;
    bool IsValidScheduleRange(int startMinute, int endMinute) const;
    int ScheduleDraftStartMinute() const;
    int ScheduleDraftEndMinute() const;
    std::wstring FormatMinuteOfDay(int minute) const;
    static int DateStamp(const SYSTEMTIME& time);
    std::wstring GetSettingsPath() const;
    void OpenWhitelistEntry(size_t index);
    HWND FindRunningWhitelistWindow(const WhitelistEntry& entry) const;
    void BringWindowToFront(HWND target);
    void PromoteWhitelistWindow(HWND target);
    void RestorePromotedWhitelistWindows();
    void RestoreWhitelistWindow(HWND target, LONG_PTR savedStyle);
    bool IsTrackedWhitelistWindowValid() const;
    bool TryResolvePendingWhitelistWindow();
    void UpdateRemaining();
    std::wstring FormatTime(const SYSTEMTIME& now) const;
    std::wstring FormatDate(const SYSTEMTIME& now) const;
    std::wstring FormatRemaining() const;
    bool IsSystemDarkMode() const;
    bool IsWhitelistedForegroundWindow();
    bool IsExecutableWhitelisted(const std::wstring& path) const;
    bool ShouldYieldToWhitelist() const;
    std::wstring GetExecutableDirectory() const;
    std::wstring GetProcessImagePath(HWND window) const;
    static std::wstring ToLower(std::wstring value);
    static std::wstring Trim(std::wstring value);
    static std::wstring BaseName(const std::wstring& path);
    static std::wstring StripExtension(const std::wstring& filename);
    static std::wstring ResolveLaunchPath(const std::wstring& launchSpec);
    static HICON LoadIconForPath(const std::wstring& path);
    static std::wstring FormatConfigDateTime(const SYSTEMTIME& time);
    static long long FileTimeToUnixSeconds(const FILETIME& fileTime);
    static std::wstring DecodeTextFile(const std::vector<char>& bytes);
    static BOOL CALLBACK EnumWhitelistWindows(HWND window, LPARAM param);
    static BOOL CALLBACK EnumWhitelistedWindowsForRestore(HWND window, LPARAM param);

    HWND hwnd_ = nullptr;
    HINSTANCE instance_ = nullptr;
    HHOOK keyboardHook_ = nullptr;
    ULONG_PTR gdiplusToken_ = 0;
    bool darkMode_ = false;
    bool focusActive_ = false;
    bool panelOpen_ = false;
    long long rerunResumeSeconds_ = 0;
    HWND activeWhitelistWindow_ = nullptr;
    int maxFocusMinutes_ = kDefaultMaxFocusMinutes;
    int defaultFocusMinutes_ = kDefaultFocusMinutes;
    int selectedMinutes_ = kDefaultFocusMinutes;
    long long remainingSeconds_ = kDefaultFocusMinutes * 60;
    int activePanelTab_ = kPanelTabSettingsId;
    RECT panelBounds_{};
    std::vector<PanelTabDefinition> panelTabs_{ { kPanelTabOverviewId, L"概览" }, { kPanelTabScheduleId, L"计划" }, { kPanelTabSettingsId, L"设置" } };
    std::vector<ScheduledFocusTask> scheduledTasks_;
    int scheduleScrollOffset_ = 0;
    int scheduleDraftStartHour_ = 9;
    int scheduleDraftStartMinute_ = 0;
    int scheduleDraftEndHour_ = 9;
    int scheduleDraftEndMinute_ = 25;
    std::wstring scheduleMessage_;
    bool scheduleMessageIsError_ = false;
    std::wstring rerunTaskMessage_;
    bool rerunTaskMessageIsError_ = false;
    std::chrono::steady_clock::time_point focusEnd_{};
    std::chrono::steady_clock::time_point whitelistYieldUntil_{};
    int pendingWhitelistIndex_ = -1;
    int whitelistLeft_ = -1;
    int whitelistTop_ = 92;
    int whitelistIconSize_ = kDefaultWhitelistIconSize;
    RECT whitelistBounds_{};
    RECT previousWhitelistBounds_{};
    bool whitelistLayoutDirty_ = false;
    bool whitelistLayoutNeedsRebuild_ = false;
    bool whitelistLayoutSavePending_ = false;
    DWORD whitelistLayoutLastChangeTick_ = 0;
    bool whitelistDragging_ = false;
    bool whitelistDragMoved_ = false;
    POINT whitelistDragStartPt_{};
    POINT whitelistDragStartOrigin_{};
    std::vector<UiButton> buttons_;
    std::vector<WhitelistEntry> whitelistEntries_;
    std::map<HWND, LONG_PTR> promotedWindows_;
    FILETIME whitelistWriteTime_{};
    bool whitelistKnown_ = false;
};

FocusClockApp* gKeyboardHookApp = nullptr;

int FocusClockApp::Run(HINSTANCE instance, int show, long long rerunResumeSeconds) {
    rerunResumeSeconds_ = rerunResumeSeconds;

    GdiplusStartupInput gdiplusStartupInput;
    if (GdiplusStartup(&gdiplusToken_, &gdiplusStartupInput, nullptr) != Ok) {
        MessageBoxW(nullptr, L"无法初始化 GDI+。", L"FocusClock", MB_ICONERROR);
        return 1;
    }

    if (!Create(instance, show)) {
        GdiplusShutdown(gdiplusToken_);
        return 1;
    }

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    GdiplusShutdown(gdiplusToken_);
    return static_cast<int>(msg.wParam);
}

bool FocusClockApp::Create(HINSTANCE instance, int show) {
    instance_ = instance;
    if (std::none_of(panelTabs_.begin(), panelTabs_.end(), [](const PanelTabDefinition& tab) {
        return tab.id == kPanelTabRerunId;
    })) {
        panelTabs_.push_back(PanelTabDefinition{ kPanelTabRerunId, L"防重启" });
    }

    const wchar_t* className = L"FocusClockWindow";

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = FocusClockApp::WindowProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hIconSm = LoadIconW(nullptr, IDI_APPLICATION);
    wc.lpszClassName = className;
    wc.hbrBackground = nullptr;

    if (!RegisterClassExW(&wc)) {
        MessageBoxW(nullptr, L"无法注册窗口类。", L"FocusClock", MB_ICONERROR);
        return false;
    }

    hwnd_ = CreateWindowExW(
        WS_EX_APPWINDOW,
        className,
        L"FocusClock",
        WS_POPUP,
        0,
        0,
        GetSystemMetrics(SM_CXSCREEN),
        GetSystemMetrics(SM_CYSCREEN),
        nullptr,
        nullptr,
        instance,
        this);

    if (!hwnd_) {
        MessageBoxW(nullptr, L"无法创建窗口。", L"FocusClock", MB_ICONERROR);
        return false;
    }

    RefreshTheme();
    LoadAppSettings();
    LoadScheduledFocusTasks();
    LoadWhitelistIfNeeded(true);
    LoadWhitelistLayoutSettings();
    ApplyDarkMode();
    EnterFullscreenNotTopmost();
    RebuildLayout();
    ShowWindow(hwnd_, show);
    UpdateWindow(hwnd_);

    RegisterHotKey(hwnd_, kPanelHotkeyId, 0, VK_F6);
    SetTimer(hwnd_, kClockTimer, kClockTimerMs, nullptr);
    SetTimer(hwnd_, kGuardTimer, kGuardTimerMs, nullptr);
    if (rerunResumeSeconds_ > 0) {
        StartFocusForSeconds(rerunResumeSeconds_, false);
    } else {
        CheckScheduledFocusTasks(true);
    }
    return true;
}

LRESULT CALLBACK FocusClockApp::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    FocusClockApp* app = nullptr;

    if (msg == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        app = static_cast<FocusClockApp*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        app->hwnd_ = hwnd;
    } else {
        app = reinterpret_cast<FocusClockApp*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (app) {
        return app->HandleMessage(hwnd, msg, wparam, lparam);
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

LRESULT CALLBACK FocusClockApp::KeyboardHookProc(int code, WPARAM wparam, LPARAM lparam) {
    if (code == HC_ACTION && gKeyboardHookApp) {
        bool keyDown = wparam == WM_KEYDOWN || wparam == WM_SYSKEYDOWN;
        if (keyDown) {
            auto* info = reinterpret_cast<KBDLLHOOKSTRUCT*>(lparam);
            if (info && gKeyboardHookApp->ShouldBlockFocusShortcut(info->vkCode)) {
                MessageBeep(MB_ICONINFORMATION);
                return 1;
            }
        }
    }

    return CallNextHookEx(nullptr, code, wparam, lparam);
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

    case WM_TIMER:
        if (wparam == kClockTimer) {
            UpdateRemaining();
            InvalidateRect(hwnd, nullptr, FALSE);
        } else if (wparam == kGuardTimer) {
            if (LoadWhitelistIfNeeded()) {
                RebuildLayout();
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            CheckScheduledFocusTasks();
            if (focusActive_) {
                TryResolvePendingWhitelistWindow();
                if ((ShouldYieldToWhitelist() || IsWhitelistedForegroundWindow()) && IsTrackedWhitelistWindowValid()) {
                    EnterFullscreenBelow(activeWhitelistWindow_);
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

void FocusClockApp::EnterFullscreenTopmost() {
    HMONITOR monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    GetMonitorInfoW(monitor, &info);

    SetWindowLongPtrW(hwnd_, GWL_STYLE, WS_POPUP | WS_VISIBLE);
    SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, WS_EX_TOPMOST | WS_EX_APPWINDOW);
    SetWindowPos(
        hwnd_,
        HWND_TOPMOST,
        info.rcMonitor.left,
        info.rcMonitor.top,
        info.rcMonitor.right - info.rcMonitor.left,
        info.rcMonitor.bottom - info.rcMonitor.top,
        SWP_SHOWWINDOW | SWP_FRAMECHANGED);
}

void FocusClockApp::EnterFullscreenNotTopmost() {
    HMONITOR monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    GetMonitorInfoW(monitor, &info);

    SetWindowLongPtrW(hwnd_, GWL_STYLE, WS_POPUP | WS_VISIBLE);
    SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, WS_EX_APPWINDOW);
    SetWindowPos(
        hwnd_,
        HWND_NOTOPMOST,
        info.rcMonitor.left,
        info.rcMonitor.top,
        info.rcMonitor.right - info.rcMonitor.left,
        info.rcMonitor.bottom - info.rcMonitor.top,
        SWP_SHOWWINDOW | SWP_FRAMECHANGED);
}

void FocusClockApp::EnterFullscreenBelow(HWND aboveWindow) {
    HMONITOR monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    GetMonitorInfoW(monitor, &info);

    SetWindowLongPtrW(hwnd_, GWL_STYLE, WS_POPUP | WS_VISIBLE);
    SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, WS_EX_TOPMOST | WS_EX_APPWINDOW);
    SetWindowPos(
        hwnd_,
        aboveWindow,
        info.rcMonitor.left,
        info.rcMonitor.top,
        info.rcMonitor.right - info.rcMonitor.left,
        info.rcMonitor.bottom - info.rcMonitor.top,
        SWP_SHOWWINDOW | SWP_FRAMECHANGED | SWP_NOACTIVATE);
}

void FocusClockApp::ApplyDarkMode() {
    BOOL enabled = darkMode_ ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd_, kDwmwaUseImmersiveDarkMode, &enabled, sizeof(enabled));
}

void FocusClockApp::InstallFocusKeyboardHook() {
    if (keyboardHook_) {
        return;
    }

    gKeyboardHookApp = this;
    keyboardHook_ = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardHookProc, instance_, 0);
    if (!keyboardHook_) {
        gKeyboardHookApp = nullptr;
    }
}

void FocusClockApp::RemoveFocusKeyboardHook() {
    if (!keyboardHook_) {
        if (gKeyboardHookApp == this) {
            gKeyboardHookApp = nullptr;
        }
        return;
    }

    UnhookWindowsHookEx(keyboardHook_);
    keyboardHook_ = nullptr;
    if (gKeyboardHookApp == this) {
        gKeyboardHookApp = nullptr;
    }
}

bool FocusClockApp::ShouldBlockFocusShortcut(DWORD vkCode) const {
    if (!focusActive_) {
        return false;
    }

    bool winDown = (GetAsyncKeyState(VK_LWIN) & 0x8000) != 0 || (GetAsyncKeyState(VK_RWIN) & 0x8000) != 0;
    bool ctrlDown = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    if (!winDown) {
        return false;
    }

    if (vkCode == VK_TAB) {
        return true;
    }

    return ctrlDown && (vkCode == 'D' || vkCode == VK_LEFT || vkCode == VK_RIGHT || vkCode == VK_F4);
}

bool FocusClockApp::LoadWhitelistIfNeeded(bool force) {
    std::wstring path = GetExecutableDirectory() + L"\\Whitelist.txt";

    WIN32_FILE_ATTRIBUTE_DATA data{};
    bool exists = GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data) != FALSE;
    if (!exists) {
        if (force || whitelistKnown_) {
            whitelistEntries_.clear();
            whitelistWriteTime_ = FILETIME{};
            whitelistKnown_ = false;
            return true;
        }
        return false;
    }

    if (!force && whitelistKnown_ && CompareFileTime(&data.ftLastWriteTime, &whitelistWriteTime_) == 0) {
        return false;
    }

    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size) || size.QuadPart > 1024 * 1024) {
        CloseHandle(file);
        return false;
    }

    std::vector<char> bytes(static_cast<size_t>(size.QuadPart));
    DWORD read = 0;
    if (!bytes.empty()) {
        ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr);
        bytes.resize(read);
    }
    CloseHandle(file);

    std::wstring text = DecodeTextFile(bytes);
    std::vector<WhitelistEntry> entries;
    size_t start = 0;
    while (start <= text.size()) {
        size_t end = text.find_first_of(L"\r\n", start);
        std::wstring line = Trim(text.substr(start, end == std::wstring::npos ? std::wstring::npos : end - start));
        if (!line.empty() && line[0] != L'#' && line[0] != L';') {
            if (line.size() >= 2 && line.front() == L'"' && line.back() == L'"') {
                line = Trim(line.substr(1, line.size() - 2));
            }
            if (!line.empty()) {
                WhitelistEntry entry{};
                entry.launchSpec = line;
                entry.normalized = ToLower(line);
                entry.exeName = BaseName(entry.normalized);
                entry.iconPath = ResolveLaunchPath(line);
                entry.label = StripExtension(BaseName(line));
                if (entry.label.empty()) {
                    entry.label = line;
                }
                entries.push_back(std::move(entry));
            }
        }

        if (end == std::wstring::npos) {
            break;
        }
        start = text.find_first_not_of(L"\r\n", end);
        if (start == std::wstring::npos) {
            break;
        }
    }

    whitelistEntries_ = std::move(entries);
    whitelistWriteTime_ = data.ftLastWriteTime;
    whitelistKnown_ = true;
    return true;
}

void FocusClockApp::RefreshTheme() {
    bool next = IsSystemDarkMode();
    if (next != darkMode_) {
        darkMode_ = next;
        ApplyDarkMode();
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void FocusClockApp::StartFocus() {
    StartFocusForSeconds(selectedMinutes_ * 60LL);
}

void FocusClockApp::StartFocusForSeconds(long long durationSeconds, bool updateSessionSettings) {
    ClosePanel();
    focusActive_ = true;
    InstallFocusKeyboardHook();
    remainingSeconds_ = std::max(1LL, durationSeconds);
    focusEnd_ = std::chrono::steady_clock::now() + std::chrono::seconds(remainingSeconds_);
    if (updateSessionSettings) {
        SaveFocusSessionSettings(remainingSeconds_);
    }
    EnterFullscreenTopmost();
    RebuildLayout();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void FocusClockApp::FinishFocus() {
    focusActive_ = false;
    ClearFocusSessionSettings();
    RemoveFocusKeyboardHook();
    activeWhitelistWindow_ = nullptr;
    pendingWhitelistIndex_ = -1;
    whitelistYieldUntil_ = std::chrono::steady_clock::time_point{};
    remainingSeconds_ = selectedMinutes_ * 60LL;

    RestorePromotedWhitelistWindows();

    MessageBeep(MB_OK);
    EnterFullscreenNotTopmost();
    RebuildLayout();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void FocusClockApp::RebuildLayout() {
    RECT rc{};
    GetClientRect(hwnd_, &rc);
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;

    buttons_.clear();

    const int buttonHeight = 64;
    const int gap = 18;
    const int bottom = height - 86;
    const int durationWidth = 132;
    const std::array<int, 4> minutes{ 25, 45, 60, 90 };

    int totalWidth = static_cast<int>(minutes.size()) * durationWidth + static_cast<int>(minutes.size() - 1) * gap;
    int startX = (width - totalWidth) / 2;

    if (!focusActive_) {
        for (size_t i = 0; i < minutes.size(); ++i) {
            int left = startX + static_cast<int>(i) * (durationWidth + gap);
            UiButton b{};
            b.rect = RECT{ left, bottom - buttonHeight - 178, left + durationWidth, bottom - 178 };
            b.label = std::to_wstring(minutes[i]) + L" 分钟";
            b.id = minutes[i];
            b.enabled = minutes[i] <= maxFocusMinutes_;
            b.selected = selectedMinutes_ == minutes[i];
            buttons_.push_back(b);
        }

        const int customSmallWidth = 74;
        const int customDisplayWidth = 218;
        const int customTotalWidth = customSmallWidth * 4 + customDisplayWidth + gap * 4;
        int customLeft = (width - customTotalWidth) / 2;
        int customTop = bottom - buttonHeight - 88;

        const std::array<std::pair<int, const wchar_t*>, 5> customButtons{ {
            { kCustomMinusFiveId, L"-5" },
            { kCustomMinusOneId, L"-1" },
            { kCustomDisplayId, L"自定义 " },
            { kCustomPlusOneId, L"+1" },
            { kCustomPlusFiveId, L"+5" },
        } };

        for (size_t i = 0; i < customButtons.size(); ++i) {
            int buttonWidth = (customButtons[i].first == kCustomDisplayId) ? customDisplayWidth : customSmallWidth;
            UiButton custom{};
            custom.rect = RECT{ customLeft, customTop, customLeft + buttonWidth, customTop + buttonHeight };
            custom.id = customButtons[i].first;
            custom.label = customButtons[i].second;
            if (custom.id == kCustomDisplayId) {
                custom.label += std::to_wstring(selectedMinutes_) + L" 分钟";
                custom.selected = true;
                custom.enabled = false;
            } else if (custom.id == kCustomMinusFiveId || custom.id == kCustomMinusOneId) {
                custom.enabled = selectedMinutes_ > kMinFocusMinutes;
            } else {
                custom.enabled = selectedMinutes_ < maxFocusMinutes_;
            }
            buttons_.push_back(custom);
            customLeft += buttonWidth + gap;
        }

        UiButton start{};
        start.rect = RECT{ width / 2 - 150, bottom - buttonHeight, width / 2 + 150, bottom };
        start.label = L"开始专注";
        start.id = kStartButtonId;
        start.primary = true;
        buttons_.push_back(start);

        UiButton exit{};
        exit.rect = RECT{ width - 172, height - 70, width - 32, height - 26 };
        exit.label = L"退出";
        exit.id = kExitButtonId;
        exit.danger = true;
        buttons_.push_back(exit);
    }

    if (panelOpen_ && !focusActive_) {
        int panelWidth = std::min(760, std::max(520, width - 96));
        int panelHeight = std::min(560, std::max(420, height - 120));
        int left = (width - panelWidth) / 2;
        int top = (height - panelHeight) / 2;
        panelBounds_ = RECT{ left, top, left + panelWidth, top + panelHeight };

        UiButton close{};
        close.rect = RECT{ panelBounds_.right - 58, panelBounds_.top + 24, panelBounds_.right - 24, panelBounds_.top + 58 };
        close.label = L"×";
        close.id = kPanelCloseButtonId;
        close.danger = true;
        buttons_.push_back(close);

        int tabLeft = panelBounds_.left + 32;
        int tabTop = panelBounds_.top + 84;
        for (size_t i = 0; i < panelTabs_.size(); ++i) {
            UiButton tab{};
            tab.rect = RECT{ tabLeft, tabTop + static_cast<int>(i) * 52, tabLeft + 148, tabTop + static_cast<int>(i) * 52 + 40 };
            tab.label = panelTabs_[i].title;
            tab.id = kPanelTabButtonBaseId + panelTabs_[i].id;
            tab.selected = activePanelTab_ == panelTabs_[i].id;
            buttons_.push_back(tab);
        }

        int contentLeft = panelBounds_.left + 216;
        if (activePanelTab_ == kPanelTabScheduleId) {
            scheduleScrollOffset_ = std::clamp(scheduleScrollOffset_, 0, ScheduleContentMaxScroll());
            int scrollY = scheduleScrollOffset_;
            AddTimeStepperButtons(kScheduleStartHourMinusId, kScheduleStartHourDisplayId, kScheduleStartHourPlusId, contentLeft, panelBounds_.top + 214 - scrollY, scheduleDraftStartHour_, 23, L" 时");
            AddTimeStepperButtons(kScheduleStartMinuteMinusId, kScheduleStartMinuteDisplayId, kScheduleStartMinutePlusId, contentLeft + 236, panelBounds_.top + 214 - scrollY, scheduleDraftStartMinute_, 59, L" 分");
            AddTimeStepperButtons(kScheduleEndHourMinusId, kScheduleEndHourDisplayId, kScheduleEndHourPlusId, contentLeft, panelBounds_.top + 316 - scrollY, scheduleDraftEndHour_, 23, L" 时");
            AddTimeStepperButtons(kScheduleEndMinuteMinusId, kScheduleEndMinuteDisplayId, kScheduleEndMinutePlusId, contentLeft + 236, panelBounds_.top + 316 - scrollY, scheduleDraftEndMinute_, 59, L" 分");

            AddPanelButton(
                kScheduleAddTaskId,
                RECT{ contentLeft, panelBounds_.top + 378 - scrollY, contentLeft + 154, panelBounds_.top + 422 - scrollY },
                L"添加计划",
                false,
                true,
                true,
                false);

            int listTop = panelBounds_.top + 506 - scrollY;
            int rowHeight = 34;
            RECT viewport = PanelContentViewport();
            for (int i = 0; i < static_cast<int>(scheduledTasks_.size()); ++i) {
                int rowTop = listTop + i * rowHeight;
                int rowBottom = rowTop + 26;
                if (rowBottom < viewport.top || rowTop > viewport.bottom) {
                    continue;
                }
                AddPanelButton(
                    kScheduleDeleteButtonBaseId + i,
                    RECT{ panelBounds_.right - 104, rowTop, panelBounds_.right - 40, rowBottom },
                    L"删除",
                    false,
                    true,
                    false,
                    true);
            }
        } else if (activePanelTab_ == kPanelTabSettingsId) {
            AddStepperButtons(
                kSettingMaxMinusFiveId,
                kSettingMaxMinusOneId,
                kSettingMaxDisplayId,
                kSettingMaxPlusOneId,
                kSettingMaxPlusFiveId,
                contentLeft,
                panelBounds_.top + 206,
                maxFocusMinutes_,
                L" 分钟",
                maxFocusMinutes_ > kMinFocusMinutes,
                maxFocusMinutes_ < kAbsoluteMaxFocusMinutes);

            AddStepperButtons(
                kSettingDefaultMinusFiveId,
                kSettingDefaultMinusOneId,
                kSettingDefaultDisplayId,
                kSettingDefaultPlusOneId,
                kSettingDefaultPlusFiveId,
                contentLeft,
                panelBounds_.top + 350,
                defaultFocusMinutes_,
                L" 分钟",
                defaultFocusMinutes_ > kMinFocusMinutes,
                defaultFocusMinutes_ < maxFocusMinutes_);

            AddPanelButton(
                kPanelResetSettingsId,
                RECT{ contentLeft, panelBounds_.bottom - 82, contentLeft + 154, panelBounds_.bottom - 38 },
                L"恢复默认",
                false,
                true,
                false,
                true);
        } else if (activePanelTab_ == kPanelTabRerunId) {
            AddPanelButton(
                kRerunCreateTaskId,
                RECT{ contentLeft, panelBounds_.top + 184, contentLeft + 250, panelBounds_.top + 232 },
                L"添加开机自启任务",
                false,
                true,
                true,
                false);
        }
    } else {
        panelBounds_ = RECT{};
    }

    const int appButtonSize = whitelistIconSize_;
    const int appColumns = 3;
    const int appGap = 14;
    const int appGridWidth = appColumns * appButtonSize + (appColumns - 1) * appGap;
    if (whitelistLeft_ < 0) {
        whitelistLeft_ = width - appGridWidth - 36;
    }
    ClampWhitelistLayout(width, height);
    const int appLeft = whitelistLeft_;
    const int appTop = whitelistTop_;
    const int appBottomLimit = height - 112;
    const int appRows = std::max(0, (appBottomLimit - appTop - 40) / (appButtonSize + appGap));
    const int appCapacity = std::max(0, std::min(12, appRows * appColumns));
    const int appCount = std::min(static_cast<int>(whitelistEntries_.size()), appCapacity);
    whitelistBounds_ = RECT{ appLeft, appTop, appLeft + appGridWidth, appTop + 40 };

    for (int i = 0; i < appCount; ++i) {
        int row = i / appColumns;
        int column = i % appColumns;
        int left = appLeft + column * (appButtonSize + appGap);
        int top = appTop + 40 + row * (appButtonSize + appGap);
        UiButton app{};
        app.rect = RECT{ left, top, left + appButtonSize, top + appButtonSize };
        app.label = whitelistEntries_[i].label;
        app.iconPath = whitelistEntries_[i].iconPath;
        app.id = kWhitelistButtonBaseId + i;
        app.primary = focusActive_;
        app.iconOnly = true;
        buttons_.push_back(app);

        if (i == 0) {
            whitelistBounds_.top = std::min(whitelistBounds_.top, static_cast<LONG>(top));
        }
        whitelistBounds_.left = std::min(whitelistBounds_.left, static_cast<LONG>(left));
        whitelistBounds_.right = std::max(whitelistBounds_.right, static_cast<LONG>(left + appButtonSize));
        whitelistBounds_.bottom = std::max(whitelistBounds_.bottom, static_cast<LONG>(top + appButtonSize));
    }
}

void FocusClockApp::Paint() {
    PAINTSTRUCT ps{};
    HDC hdc = BeginPaint(hwnd_, &ps);
    RECT rc{};
    GetClientRect(hwnd_, &rc);
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;

    HDC mem = CreateCompatibleDC(hdc);
    HBITMAP bitmap = CreateCompatibleBitmap(hdc, width, height);
    HGDIOBJ oldBitmap = SelectObject(mem, bitmap);

    Graphics g(mem);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

    DrawBackground(g, rc);

    Theme theme{
        darkMode_ ? Color(255, 11, 15, 20) : Color(255, 245, 247, 250),
        darkMode_ ? Color(255, 24, 30, 39) : Color(255, 255, 255, 255),
        darkMode_ ? Color(255, 242, 245, 247) : Color(255, 24, 28, 35),
        darkMode_ ? Color(255, 178, 187, 199) : Color(255, 79, 88, 103),
        darkMode_ ? Color(255, 119, 131, 146) : Color(255, 104, 116, 132),
        darkMode_ ? Color(255, 89, 180, 255) : Color(255, 0, 113, 188),
        darkMode_ ? Color(50, 89, 180, 255) : Color(32, 0, 113, 188),
        darkMode_ ? Color(255, 255, 115, 115) : Color(255, 190, 46, 46),
        darkMode_ ? Color(255, 54, 64, 78) : Color(255, 214, 220, 228)
    };

    SYSTEMTIME now{};
    GetLocalTime(&now);

    int clockSize = std::min(width, height) / 3;
    clockSize = std::max(220, std::min(clockSize, 390));
    RectF clockRect(
        static_cast<REAL>((width - clockSize) / 2),
        static_cast<REAL>(height * 0.12),
        static_cast<REAL>(clockSize),
        static_cast<REAL>(clockSize));
    DrawAnalogClock(g, clockRect, now);

    FontFamily family(L"Segoe UI");
    FontFamily monoFamily(L"Consolas");

    REAL digitalSize = static_cast<REAL>(std::max(56, std::min(132, width / 10)));
    Font digitalFont(&monoFamily, digitalSize, FontStyleRegular, UnitPixel);
    Font dateFont(&family, static_cast<REAL>(std::max(20, std::min(34, width / 48))), FontStyleRegular, UnitPixel);
    Font statusFont(&family, static_cast<REAL>(std::max(22, std::min(40, width / 42))), FontStyleRegular, UnitPixel);
    Font remainFont(&monoFamily, static_cast<REAL>(std::max(48, std::min(96, width / 14))), FontStyleRegular, UnitPixel);

    RectF digitalRect(0, static_cast<REAL>(height * 0.52), static_cast<REAL>(width), digitalSize + 18);
    DrawTextBlock(g, FormatTime(now), digitalRect, digitalFont, theme.primaryText, StringAlignmentCenter, StringAlignmentCenter);

    RectF dateRect(0, digitalRect.Y + digitalRect.Height + 4, static_cast<REAL>(width), 48);
    DrawTextBlock(g, FormatDate(now), dateRect, dateFont, theme.secondaryText, StringAlignmentCenter, StringAlignmentCenter);

    if (focusActive_) {
        RectF remainRect(0, dateRect.Y + 56, static_cast<REAL>(width), 92);
        DrawTextBlock(g, FormatRemaining(), remainRect, remainFont, theme.accent, StringAlignmentCenter, StringAlignmentCenter);

        RectF statusRect(0, remainRect.Y + remainRect.Height + 8, static_cast<REAL>(width), 52);
        DrawTextBlock(g, L"专注进行中", statusRect, statusFont, theme.secondaryText, StringAlignmentCenter, StringAlignmentCenter);

        Font hintFont(&family, 18, FontStyleRegular, UnitPixel);
        RectF hintRect(0, static_cast<REAL>(height - 70), static_cast<REAL>(width), 28);
        DrawTextBlock(g, L"专注结束前会保持全屏和置顶", hintRect, hintFont, theme.mutedText, StringAlignmentCenter, StringAlignmentCenter);
    } else {
        Font hintFont(&family, 20, FontStyleRegular, UnitPixel);
        RectF hintRect(0, dateRect.Y + 58, static_cast<REAL>(width), 34);
        DrawTextBlock(g, L"选择时长后开始", hintRect, hintFont, theme.mutedText, StringAlignmentCenter, StringAlignmentCenter);
    }

    auto appButton = std::find_if(buttons_.begin(), buttons_.end(), [](const UiButton& button) {
        return button.id >= kWhitelistButtonBaseId && button.id < kWhitelistButtonLimit;
    });
    if (appButton != buttons_.end()) {
        Font appTitleFont(&family, 18, FontStyleRegular, UnitPixel);
        RectF titleRect(
            static_cast<REAL>(appButton->rect.left),
            static_cast<REAL>(appButton->rect.top - 38),
            static_cast<REAL>(width - appButton->rect.left - 36),
            28.0f);
        DrawTextBlock(g, L"白名单程序", titleRect, appTitleFont, theme.secondaryText, StringAlignmentNear, StringAlignmentCenter);
    }

    for (const auto& button : buttons_) {
        DrawButton(g, button);
    }

    if (panelOpen_ && !focusActive_) {
        DrawPanel(g, rc);
        RECT viewport = PanelContentViewport();
        for (const auto& button : buttons_) {
            if (button.id >= kPanelButtonBaseId) {
                if (activePanelTab_ == kPanelTabScheduleId &&
                    button.id >= kScheduleStartHourMinusId &&
                    button.id < kScheduleDeleteButtonLimit &&
                    (button.rect.bottom < viewport.top || button.rect.top > viewport.bottom)) {
                    continue;
                }
                DrawButton(g, button);
            }
        }
    }

    BitBlt(hdc, 0, 0, width, height, mem, 0, 0, SRCCOPY);
    SelectObject(mem, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(mem);
    EndPaint(hwnd_, &ps);
}

void FocusClockApp::DrawBackground(Graphics& g, const RECT& rc) {
    Color top = darkMode_ ? Color(255, 9, 13, 18) : Color(255, 241, 245, 249);
    Color bottom = darkMode_ ? Color(255, 18, 24, 31) : Color(255, 255, 255, 255);
    LinearGradientBrush brush(
        Point(0, rc.top),
        Point(0, rc.bottom),
        top,
        bottom);
    g.FillRectangle(
        &brush,
        static_cast<INT>(rc.left),
        static_cast<INT>(rc.top),
        static_cast<INT>(rc.right - rc.left),
        static_cast<INT>(rc.bottom - rc.top));
}

void FocusClockApp::DrawAnalogClock(Graphics& g, const RectF& bounds, const SYSTEMTIME& now) {
    Theme theme{
        darkMode_ ? Color(255, 11, 15, 20) : Color(255, 245, 247, 250),
        darkMode_ ? Color(255, 24, 30, 39) : Color(255, 255, 255, 255),
        darkMode_ ? Color(255, 242, 245, 247) : Color(255, 24, 28, 35),
        darkMode_ ? Color(255, 178, 187, 199) : Color(255, 79, 88, 103),
        darkMode_ ? Color(255, 119, 131, 146) : Color(255, 104, 116, 132),
        darkMode_ ? Color(255, 89, 180, 255) : Color(255, 0, 113, 188),
        darkMode_ ? Color(50, 89, 180, 255) : Color(32, 0, 113, 188),
        darkMode_ ? Color(255, 255, 115, 115) : Color(255, 190, 46, 46),
        darkMode_ ? Color(255, 54, 64, 78) : Color(255, 214, 220, 228)
    };

    PointF center(bounds.X + bounds.Width / 2.0f, bounds.Y + bounds.Height / 2.0f);
    float radius = bounds.Width / 2.0f;

    SolidBrush faceBrush(darkMode_ ? Color(235, 18, 24, 32) : Color(242, 255, 255, 255));
    Pen rimPen(theme.line, 2.0f);
    g.FillEllipse(&faceBrush, bounds);
    g.DrawEllipse(&rimPen, bounds);

    Pen tickPen(theme.secondaryText, 2.0f);
    Pen hourTickPen(theme.primaryText, 4.0f);
    for (int i = 0; i < 60; ++i) {
        double angle = (i / 60.0) * 2.0 * kPi - kPi / 2.0;
        float outer = radius - 16.0f;
        float inner = outer - ((i % 5 == 0) ? 16.0f : 8.0f);
        PointF p1(center.X + std::cos(angle) * inner, center.Y + std::sin(angle) * inner);
        PointF p2(center.X + std::cos(angle) * outer, center.Y + std::sin(angle) * outer);
        g.DrawLine(i % 5 == 0 ? &hourTickPen : &tickPen, p1, p2);
    }

    double second = now.wSecond + now.wMilliseconds / 1000.0;
    double minute = now.wMinute + second / 60.0;
    double hour = (now.wHour % 12) + minute / 60.0;

    DrawHand(g, center, radius * 0.50f, hour / 12.0 * 2.0 * kPi - kPi / 2.0, 7.0f, theme.primaryText);
    DrawHand(g, center, radius * 0.70f, minute / 60.0 * 2.0 * kPi - kPi / 2.0, 5.0f, theme.primaryText);
    DrawHand(g, center, radius * 0.78f, second / 60.0 * 2.0 * kPi - kPi / 2.0, 2.0f, theme.danger);

    SolidBrush centerBrush(theme.accent);
    g.FillEllipse(&centerBrush, center.X - 6.0f, center.Y - 6.0f, 12.0f, 12.0f);
}

void FocusClockApp::DrawHand(Graphics& g, const PointF& center, float radius, double angleRadians, float width, const Color& color) {
    Pen pen(color, width);
    pen.SetStartCap(LineCapRound);
    pen.SetEndCap(LineCapRound);
    PointF end(center.X + std::cos(angleRadians) * radius, center.Y + std::sin(angleRadians) * radius);
    g.DrawLine(&pen, center, end);
}

void FocusClockApp::DrawTextBlock(Graphics& g, const std::wstring& text, const RectF& rect, Font& font, const Color& color, StringAlignment align, StringAlignment lineAlign) {
    StringFormat format;
    format.SetAlignment(align);
    format.SetLineAlignment(lineAlign);
    format.SetTrimming(StringTrimmingEllipsisCharacter);
    SolidBrush brush(color);
    g.DrawString(text.c_str(), static_cast<INT>(text.size()), &font, rect, &format, &brush);
}

void FocusClockApp::DrawPanel(Graphics& g, const RECT& rc) {
    Theme theme{
        darkMode_ ? Color(255, 11, 15, 20) : Color(255, 245, 247, 250),
        darkMode_ ? Color(255, 24, 30, 39) : Color(255, 255, 255, 255),
        darkMode_ ? Color(255, 242, 245, 247) : Color(255, 24, 28, 35),
        darkMode_ ? Color(255, 178, 187, 199) : Color(255, 79, 88, 103),
        darkMode_ ? Color(255, 119, 131, 146) : Color(255, 104, 116, 132),
        darkMode_ ? Color(255, 89, 180, 255) : Color(255, 0, 113, 188),
        darkMode_ ? Color(50, 89, 180, 255) : Color(32, 0, 113, 188),
        darkMode_ ? Color(255, 255, 115, 115) : Color(255, 190, 46, 46),
        darkMode_ ? Color(255, 54, 64, 78) : Color(255, 214, 220, 228)
    };

    SolidBrush scrim(darkMode_ ? Color(170, 0, 0, 0) : Color(125, 22, 29, 38));
    g.FillRectangle(
        &scrim,
        static_cast<INT>(rc.left),
        static_cast<INT>(rc.top),
        static_cast<INT>(rc.right - rc.left),
        static_cast<INT>(rc.bottom - rc.top));

    RectF panel(
        static_cast<REAL>(panelBounds_.left),
        static_cast<REAL>(panelBounds_.top),
        static_cast<REAL>(panelBounds_.right - panelBounds_.left),
        static_cast<REAL>(panelBounds_.bottom - panelBounds_.top));

    GraphicsPath path;
    const REAL radius = 10.0f;
    path.AddArc(panel.X, panel.Y, radius * 2, radius * 2, 180, 90);
    path.AddArc(panel.X + panel.Width - radius * 2, panel.Y, radius * 2, radius * 2, 270, 90);
    path.AddArc(panel.X + panel.Width - radius * 2, panel.Y + panel.Height - radius * 2, radius * 2, radius * 2, 0, 90);
    path.AddArc(panel.X, panel.Y + panel.Height - radius * 2, radius * 2, radius * 2, 90, 90);
    path.CloseFigure();

    SolidBrush panelBrush(theme.panel);
    Pen borderPen(theme.line, 1.5f);
    g.FillPath(&panelBrush, &path);
    g.DrawPath(&borderPen, &path);

    FontFamily family(L"Segoe UI");
    Font titleFont(&family, 26, FontStyleRegular, UnitPixel);
    Font hintFont(&family, 15, FontStyleRegular, UnitPixel);
    RectF titleRect(panel.X + 32.0f, panel.Y + 24.0f, panel.Width - 112.0f, 34.0f);
    DrawTextBlock(g, L"功能面板", titleRect, titleFont, theme.primaryText, StringAlignmentNear, StringAlignmentCenter);

    RectF hintRect(panel.X + 32.0f, panel.Y + 56.0f, panel.Width - 112.0f, 22.0f);
    DrawTextBlock(g, L"按 F6 或 Esc 关闭，专注时段不可打开", hintRect, hintFont, theme.mutedText, StringAlignmentNear, StringAlignmentCenter);

    Pen divider(theme.line, 1.0f);
    g.DrawLine(&divider, PointF(panel.X + 184.0f, panel.Y + 84.0f), PointF(panel.X + 184.0f, panel.Y + panel.Height - 32.0f));

    RectF contentRect(panel.X + 216.0f, panel.Y + 92.0f, panel.Width - 248.0f, panel.Height - 126.0f);
    if (activePanelTab_ == kPanelTabOverviewId) {
        DrawOverviewTab(g, contentRect, theme, family);
    } else if (activePanelTab_ == kPanelTabScheduleId) {
        GraphicsState state = g.Save();
        g.SetClip(contentRect);
        DrawScheduleTab(g, contentRect, theme, family);
        g.Restore(state);
    } else if (activePanelTab_ == kPanelTabSettingsId) {
        DrawSettingsTab(g, contentRect, theme, family);
    } else if (activePanelTab_ == kPanelTabRerunId) {
        DrawRerunTab(g, contentRect, theme, family);
    }
}

void FocusClockApp::DrawOverviewTab(Graphics& g, const RectF& contentRect, const Theme& theme, FontFamily& family) {
    Font headingFont(&family, 24, FontStyleRegular, UnitPixel);
    Font bodyFont(&family, 17, FontStyleRegular, UnitPixel);
    Font smallFont(&family, 15, FontStyleRegular, UnitPixel);

    DrawTextBlock(g, L"概览", RectF(contentRect.X, contentRect.Y, contentRect.Width, 34.0f), headingFont, theme.primaryText, StringAlignmentNear, StringAlignmentCenter);
    DrawTextBlock(g, L"这里是可扩展标签页框架的占位页，后续功能可以通过新增标签定义接入。", RectF(contentRect.X, contentRect.Y + 48.0f, contentRect.Width, 60.0f), bodyFont, theme.secondaryText, StringAlignmentNear, StringAlignmentNear);

    std::wstring summary = L"当前默认时长 " + std::to_wstring(defaultFocusMinutes_) + L" 分钟，最大时长 " + std::to_wstring(maxFocusMinutes_) + L" 分钟。";
    DrawTextBlock(g, summary, RectF(contentRect.X, contentRect.Y + 132.0f, contentRect.Width, 28.0f), smallFont, theme.mutedText, StringAlignmentNear, StringAlignmentCenter);
}

void FocusClockApp::DrawSettingsTab(Graphics& g, const RectF& contentRect, const Theme& theme, FontFamily& family) {
    Font headingFont(&family, 24, FontStyleRegular, UnitPixel);
    Font labelFont(&family, 18, FontStyleRegular, UnitPixel);
    Font helpFont(&family, 15, FontStyleRegular, UnitPixel);

    DrawTextBlock(g, L"设置", RectF(contentRect.X, contentRect.Y, contentRect.Width, 34.0f), headingFont, theme.primaryText, StringAlignmentNear, StringAlignmentCenter);

    DrawTextBlock(g, L"最大专注时长", RectF(contentRect.X, contentRect.Y + 58.0f, contentRect.Width, 28.0f), labelFont, theme.primaryText, StringAlignmentNear, StringAlignmentCenter);
    DrawTextBlock(g, L"控制自定义时长可调到的上限，保存到 FocusClock.ini。", RectF(contentRect.X, contentRect.Y + 88.0f, contentRect.Width, 26.0f), helpFont, theme.mutedText, StringAlignmentNear, StringAlignmentCenter);

    DrawTextBlock(g, L"默认专注时长", RectF(contentRect.X, contentRect.Y + 202.0f, contentRect.Width, 28.0f), labelFont, theme.primaryText, StringAlignmentNear, StringAlignmentCenter);
    DrawTextBlock(g, L"启动时和恢复默认时使用的时长，会自动受最大时长限制。", RectF(contentRect.X, contentRect.Y + 232.0f, contentRect.Width, 26.0f), helpFont, theme.mutedText, StringAlignmentNear, StringAlignmentCenter);
}

void FocusClockApp::DrawScheduleTab(Graphics& g, const RectF& contentRect, const Theme& theme, FontFamily& family) {
    Font headingFont(&family, 24, FontStyleRegular, UnitPixel);
    Font labelFont(&family, 18, FontStyleRegular, UnitPixel);
    Font helpFont(&family, 15, FontStyleRegular, UnitPixel);
    Font rowFont(&family, 16, FontStyleRegular, UnitPixel);
    REAL y = -static_cast<REAL>(scheduleScrollOffset_);

    DrawTextBlock(g, L"计划专注", RectF(contentRect.X, contentRect.Y + y, contentRect.Width, 34.0f), headingFont, theme.primaryText, StringAlignmentNear, StringAlignmentCenter);
    DrawTextBlock(g, L"添加每天执行的计划，时间段不能重叠。到达计划时间后会自动开始专注。", RectF(contentRect.X, contentRect.Y + 38.0f + y, contentRect.Width, 42.0f), helpFont, theme.mutedText, StringAlignmentNear, StringAlignmentNear);

    DrawTextBlock(g, L"开始时间", RectF(contentRect.X, contentRect.Y + 96.0f + y, contentRect.Width, 28.0f), labelFont, theme.primaryText, StringAlignmentNear, StringAlignmentCenter);
    DrawTextBlock(g, L"结束时间", RectF(contentRect.X, contentRect.Y + 198.0f + y, contentRect.Width, 28.0f), labelFont, theme.primaryText, StringAlignmentNear, StringAlignmentCenter);

    Color messageColor = scheduleMessageIsError_ ? theme.danger : theme.secondaryText;
    std::wstring message = scheduleMessage_;
    if (message.empty()) {
        int duration = ScheduleDraftEndMinute() - ScheduleDraftStartMinute();
        if (duration > 0) {
            message = L"当前计划时长 " + std::to_wstring(duration) + L" 分钟。";
        } else {
            message = L"结束时间必须晚于开始时间。";
            messageColor = theme.danger;
        }
    }
    DrawTextBlock(g, message, RectF(contentRect.X + 170.0f, contentRect.Y + 292.0f + y, contentRect.Width - 170.0f, 44.0f), helpFont, messageColor, StringAlignmentNear, StringAlignmentCenter);

    DrawTextBlock(g, L"已添加计划", RectF(contentRect.X, contentRect.Y + 374.0f + y, contentRect.Width, 28.0f), labelFont, theme.primaryText, StringAlignmentNear, StringAlignmentCenter);
    if (scheduledTasks_.empty()) {
        DrawTextBlock(g, L"暂无计划。", RectF(contentRect.X, contentRect.Y + 408.0f + y, contentRect.Width, 28.0f), helpFont, theme.mutedText, StringAlignmentNear, StringAlignmentCenter);
        return;
    }

    int listTop = static_cast<int>(contentRect.Y + 414.0f + y);
    int rowHeight = 34;
    for (int i = 0; i < static_cast<int>(scheduledTasks_.size()); ++i) {
        int rowTop = listTop + i * rowHeight;
        if (rowTop + 26 < contentRect.Y || rowTop > contentRect.Y + contentRect.Height) {
            continue;
        }
        const auto& task = scheduledTasks_[i];
        std::wstring line = FormatMinuteOfDay(task.startMinute) + L" - " + FormatMinuteOfDay(task.endMinute) +
            L"  (" + std::to_wstring(task.endMinute - task.startMinute) + L" 分钟)";
        DrawTextBlock(g, line, RectF(contentRect.X, static_cast<REAL>(rowTop), contentRect.Width - 90.0f, 26.0f), rowFont, theme.secondaryText, StringAlignmentNear, StringAlignmentCenter);
    }

    int maxScroll = ScheduleContentMaxScroll();
    if (maxScroll > 0) {
        REAL trackX = contentRect.X + contentRect.Width - 8.0f;
        REAL trackY = contentRect.Y;
        REAL trackHeight = contentRect.Height;
        SolidBrush trackBrush(darkMode_ ? Color(90, 68, 78, 94) : Color(90, 184, 194, 207));
        SolidBrush thumbBrush(theme.accent);
        g.FillRectangle(&trackBrush, trackX, trackY, 4.0f, trackHeight);

        REAL thumbHeight = std::max(40.0f, trackHeight * (trackHeight / (trackHeight + maxScroll)));
        REAL thumbY = trackY + (trackHeight - thumbHeight) * (static_cast<REAL>(scheduleScrollOffset_) / maxScroll);
        g.FillRectangle(&thumbBrush, trackX, thumbY, 4.0f, thumbHeight);
    }
}

void FocusClockApp::DrawRerunTab(Graphics& g, const RectF& contentRect, const Theme& theme, FontFamily& family) {
    Font headingFont(&family, 24, FontStyleRegular, UnitPixel);
    Font bodyFont(&family, 17, FontStyleRegular, UnitPixel);
    Font helpFont(&family, 15, FontStyleRegular, UnitPixel);

    DrawTextBlock(g, L"防重启", RectF(contentRect.X, contentRect.Y, contentRect.Width, 34.0f), headingFont, theme.primaryText, StringAlignmentNear, StringAlignmentCenter);
    std::wstring status = rerunTaskMessage_.empty() ? L"状态：等待添加" : rerunTaskMessage_;
    Color statusColor = rerunTaskMessageIsError_ ? theme.danger : theme.secondaryText;
    DrawTextBlock(g, status, RectF(contentRect.X, contentRect.Y + 164.0f, contentRect.Width, 44.0f), helpFont, statusColor, StringAlignmentNear, StringAlignmentCenter);
    return;

    DrawTextBlock(g, L"防重启", RectF(contentRect.X, contentRect.Y, contentRect.Width, 34.0f), headingFont, theme.primaryText, StringAlignmentNear, StringAlignmentCenter);
    DrawTextBlock(
        g,
        L"添加一个当前用户登录时运行的计划任务。它会用 -rerun 参数启动 FocusClock，并只在未完成的专注时段内恢复窗口。",
        RectF(contentRect.X, contentRect.Y + 48.0f, contentRect.Width, 72.0f),
        bodyFont,
        theme.secondaryText,
        StringAlignmentNear,
        StringAlignmentNear);
    DrawTextBlock(
        g,
        L"如果电脑在专注期间重启，登录后会根据 FocusClock.ini 的 FocusSession 记录继续剩余时间；不在时段内则自动退出。",
        RectF(contentRect.X, contentRect.Y + 132.0f, contentRect.Width, 64.0f),
        helpFont,
        theme.mutedText,
        StringAlignmentNear,
        StringAlignmentNear);

    if (!rerunTaskMessage_.empty()) {
        Color messageColor = rerunTaskMessageIsError_ ? theme.danger : theme.secondaryText;
        DrawTextBlock(g, rerunTaskMessage_, RectF(contentRect.X, contentRect.Y + 286.0f, contentRect.Width, 44.0f), helpFont, messageColor, StringAlignmentNear, StringAlignmentCenter);
    }
}

void FocusClockApp::DrawButton(Graphics& g, const UiButton& button) {
    Theme theme{
        darkMode_ ? Color(255, 11, 15, 20) : Color(255, 245, 247, 250),
        darkMode_ ? Color(255, 24, 30, 39) : Color(255, 255, 255, 255),
        darkMode_ ? Color(255, 242, 245, 247) : Color(255, 24, 28, 35),
        darkMode_ ? Color(255, 178, 187, 199) : Color(255, 79, 88, 103),
        darkMode_ ? Color(255, 119, 131, 146) : Color(255, 104, 116, 132),
        darkMode_ ? Color(255, 89, 180, 255) : Color(255, 0, 113, 188),
        darkMode_ ? Color(50, 89, 180, 255) : Color(32, 0, 113, 188),
        darkMode_ ? Color(255, 255, 115, 115) : Color(255, 190, 46, 46),
        darkMode_ ? Color(255, 54, 64, 78) : Color(255, 214, 220, 228)
    };

    RectF r(
        static_cast<REAL>(button.rect.left),
        static_cast<REAL>(button.rect.top),
        static_cast<REAL>(button.rect.right - button.rect.left),
        static_cast<REAL>(button.rect.bottom - button.rect.top));

    Color fill = theme.panel;
    Color border = theme.line;
    Color text = theme.primaryText;

    if (button.primary) {
        fill = theme.accent;
        border = theme.accent;
        text = Color(255, 255, 255, 255);
    } else if (button.selected) {
        fill = theme.accentSoft;
        border = theme.accent;
        text = theme.primaryText;
    } else if (button.danger) {
        text = theme.danger;
        border = Color(120, theme.danger.GetR(), theme.danger.GetG(), theme.danger.GetB());
    }

    if (!button.enabled) {
        text = theme.secondaryText;
        border = theme.line;
        if (!button.selected) {
            fill = darkMode_ ? Color(160, 24, 30, 39) : Color(170, 245, 247, 250);
        }
    }

    GraphicsPath path;
    const REAL radius = 8.0f;
    path.AddArc(r.X, r.Y, radius * 2, radius * 2, 180, 90);
    path.AddArc(r.X + r.Width - radius * 2, r.Y, radius * 2, radius * 2, 270, 90);
    path.AddArc(r.X + r.Width - radius * 2, r.Y + r.Height - radius * 2, radius * 2, radius * 2, 0, 90);
    path.AddArc(r.X, r.Y + r.Height - radius * 2, radius * 2, radius * 2, 90, 90);
    path.CloseFigure();

    SolidBrush fillBrush(fill);
    Pen borderPen(border, 1.5f);
    g.FillPath(&fillBrush, &path);
    g.DrawPath(&borderPen, &path);

    if (button.iconOnly) {
        DrawButtonIcon(g, button, r);
        return;
    }

    FontFamily family(L"Segoe UI");
    int buttonHeight = static_cast<int>(button.rect.bottom - button.rect.top);
    REAL fontSize = static_cast<REAL>(std::max(18, std::min(24, buttonHeight / 3)));
    if (button.id >= kScheduleDeleteButtonBaseId && button.id < kScheduleDeleteButtonLimit) {
        fontSize = 14.0f;
    }
    Font font(&family, fontSize, FontStyleRegular, UnitPixel);
    DrawTextBlock(g, button.label, r, font, text, StringAlignmentCenter, StringAlignmentCenter);
}

void FocusClockApp::DrawButtonIcon(Graphics& g, const UiButton& button, const RectF& bounds) {
    HICON icon = LoadIconForPath(button.iconPath);
    if (!icon) {
        return;
    }

    int iconSize = static_cast<int>(std::min(bounds.Width, bounds.Height) * 0.62f);
    iconSize = std::max(28, std::min(iconSize, 48));
    int x = static_cast<int>(bounds.X + (bounds.Width - iconSize) / 2.0f);
    int y = static_cast<int>(bounds.Y + (bounds.Height - iconSize) / 2.0f);

    g.Flush();
    HDC hdc = g.GetHDC();
    DrawIconEx(hdc, x, y, icon, iconSize, iconSize, 0, nullptr, DI_NORMAL);
    g.ReleaseHDC(hdc);
    DestroyIcon(icon);
}

void FocusClockApp::HitTestAndClick(POINT pt) {
    if (panelOpen_) {
        RECT viewport = PanelContentViewport();
        for (const auto& button : buttons_) {
            if (button.id < kPanelButtonBaseId || !button.enabled || !PtInRect(&button.rect, pt)) {
                continue;
            }
            if (activePanelTab_ == kPanelTabScheduleId &&
                button.id >= kScheduleStartHourMinusId &&
                button.id < kScheduleDeleteButtonLimit &&
                !PtInRect(&viewport, pt)) {
                continue;
            }

            HandlePanelCommand(button.id);
            return;
        }

        return;
    }

    for (const auto& button : buttons_) {
        if (!button.enabled || !PtInRect(&button.rect, pt)) {
            continue;
        }

        if (button.id == kStartButtonId) {
            StartFocus();
        } else if (button.id == kExitButtonId) {
            DestroyWindow(hwnd_);
        } else if (button.id == kCustomMinusFiveId) {
            selectedMinutes_ = std::max(kMinFocusMinutes, selectedMinutes_ - 5);
            remainingSeconds_ = selectedMinutes_ * 60LL;
            RebuildLayout();
            InvalidateRect(hwnd_, nullptr, FALSE);
        } else if (button.id == kCustomMinusOneId) {
            selectedMinutes_ = std::max(kMinFocusMinutes, selectedMinutes_ - 1);
            remainingSeconds_ = selectedMinutes_ * 60LL;
            RebuildLayout();
            InvalidateRect(hwnd_, nullptr, FALSE);
        } else if (button.id == kCustomPlusOneId) {
            selectedMinutes_ = std::min(maxFocusMinutes_, selectedMinutes_ + 1);
            remainingSeconds_ = selectedMinutes_ * 60LL;
            RebuildLayout();
            InvalidateRect(hwnd_, nullptr, FALSE);
        } else if (button.id == kCustomPlusFiveId) {
            selectedMinutes_ = std::min(maxFocusMinutes_, selectedMinutes_ + 5);
            remainingSeconds_ = selectedMinutes_ * 60LL;
            RebuildLayout();
            InvalidateRect(hwnd_, nullptr, FALSE);
        } else if (button.id >= kWhitelistButtonBaseId && button.id < kWhitelistButtonLimit) {
            OpenWhitelistEntry(static_cast<size_t>(button.id - kWhitelistButtonBaseId));
        } else {
            selectedMinutes_ = std::clamp(button.id, kMinFocusMinutes, maxFocusMinutes_);
            remainingSeconds_ = selectedMinutes_ * 60LL;
            RebuildLayout();
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
        return;
    }
}

bool FocusClockApp::BeginWhitelistDrag(POINT pt) {
    if (!IsShiftDown() || !IsPointInWhitelistArea(pt)) {
        return false;
    }

    whitelistDragging_ = true;
    whitelistDragMoved_ = false;
    whitelistDragStartPt_ = pt;
    whitelistDragStartOrigin_ = POINT{ whitelistLeft_, whitelistTop_ };
    previousWhitelistBounds_ = whitelistBounds_;
    SetCapture(hwnd_);
    return true;
}

void FocusClockApp::UpdateWhitelistDrag(POINT pt) {
    if (!whitelistDragging_) {
        return;
    }

    int dx = pt.x - whitelistDragStartPt_.x;
    int dy = pt.y - whitelistDragStartPt_.y;
    if (std::abs(dx) >= 2 || std::abs(dy) >= 2) {
        whitelistDragMoved_ = true;
    }

    whitelistLeft_ = whitelistDragStartOrigin_.x + dx;
    whitelistTop_ = whitelistDragStartOrigin_.y + dy;
    RECT rc{};
    GetClientRect(hwnd_, &rc);
    ClampWhitelistLayout(rc.right - rc.left, rc.bottom - rc.top);
    MarkWhitelistLayoutChanged(false);
}

void FocusClockApp::EndWhitelistDrag() {
    if (!whitelistDragging_) {
        return;
    }

    whitelistDragging_ = false;
    ReleaseCapture();
    ProcessPendingWhitelistLayout(true);
}

bool FocusClockApp::IsShiftDown() const {
    return (GetKeyState(VK_SHIFT) & 0x8000) != 0;
}

bool FocusClockApp::IsPointInWhitelistArea(POINT pt) const {
    return whitelistBounds_.right > whitelistBounds_.left &&
        whitelistBounds_.bottom > whitelistBounds_.top &&
        PtInRect(&whitelistBounds_, pt);
}

void FocusClockApp::MoveWhitelistLayout(int dx, int dy) {
    whitelistLeft_ += dx;
    whitelistTop_ += dy;
    RECT rc{};
    GetClientRect(hwnd_, &rc);
    ClampWhitelistLayout(rc.right - rc.left, rc.bottom - rc.top);
    MarkWhitelistLayoutChanged(false);
    ProcessPendingWhitelistLayout();
}

void FocusClockApp::ResizeWhitelistLayout(int delta) {
    whitelistIconSize_ = std::clamp(whitelistIconSize_ + delta, kMinWhitelistIconSize, kMaxWhitelistIconSize);
    RECT rc{};
    GetClientRect(hwnd_, &rc);
    ClampWhitelistLayout(rc.right - rc.left, rc.bottom - rc.top);
    MarkWhitelistLayoutChanged(true);
    ProcessPendingWhitelistLayout();
}

void FocusClockApp::RebuildWhitelistLayoutOnly() {
    RECT rc{};
    GetClientRect(hwnd_, &rc);
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;

    ClampWhitelistLayout(width, height);

    buttons_.erase(
        std::remove_if(buttons_.begin(), buttons_.end(), [](const UiButton& button) {
            return button.id >= kWhitelistButtonBaseId && button.id < kWhitelistButtonLimit;
        }),
        buttons_.end());

    const int appButtonSize = whitelistIconSize_;
    const int appColumns = 3;
    const int appGap = 14;
    const int appGridWidth = appColumns * appButtonSize + (appColumns - 1) * appGap;
    const int appLeft = whitelistLeft_;
    const int appTop = whitelistTop_;
    const int appBottomLimit = height - 112;
    const int appRows = std::max(0, (appBottomLimit - appTop - 40) / (appButtonSize + appGap));
    const int appCapacity = std::max(0, std::min(12, appRows * appColumns));
    const int appCount = std::min(static_cast<int>(whitelistEntries_.size()), appCapacity);
    whitelistBounds_ = RECT{ appLeft, appTop, appLeft + appGridWidth, appTop + 40 };

    for (int i = 0; i < appCount; ++i) {
        int row = i / appColumns;
        int column = i % appColumns;
        int left = appLeft + column * (appButtonSize + appGap);
        int top = appTop + 40 + row * (appButtonSize + appGap);
        UiButton app{};
        app.rect = RECT{ left, top, left + appButtonSize, top + appButtonSize };
        app.label = whitelistEntries_[i].label;
        app.iconPath = whitelistEntries_[i].iconPath;
        app.id = kWhitelistButtonBaseId + i;
        app.primary = focusActive_;
        app.iconOnly = true;
        buttons_.push_back(app);

        if (i == 0) {
            whitelistBounds_.top = std::min(whitelistBounds_.top, static_cast<LONG>(top));
        }
        whitelistBounds_.left = std::min(whitelistBounds_.left, static_cast<LONG>(left));
        whitelistBounds_.right = std::max(whitelistBounds_.right, static_cast<LONG>(left + appButtonSize));
        whitelistBounds_.bottom = std::max(whitelistBounds_.bottom, static_cast<LONG>(top + appButtonSize));
    }
}

void FocusClockApp::MarkWhitelistLayoutChanged(bool needsRebuild) {
    if (!whitelistLayoutDirty_) {
        previousWhitelistBounds_ = whitelistBounds_;
    }

    whitelistLayoutDirty_ = true;
    whitelistLayoutNeedsRebuild_ = whitelistLayoutNeedsRebuild_ || needsRebuild;
    whitelistLayoutSavePending_ = true;
    whitelistLayoutLastChangeTick_ = GetTickCount();
    SetTimer(hwnd_, kWhitelistLayoutTimer, kWhitelistLayoutTimerMs, nullptr);
}

void FocusClockApp::ProcessPendingWhitelistLayout(bool forceSave) {
    if (whitelistLayoutDirty_) {
        RECT oldBounds = previousWhitelistBounds_;
        if (whitelistLayoutNeedsRebuild_) {
            RebuildWhitelistLayoutOnly();
        } else {
            RECT rc{};
            GetClientRect(hwnd_, &rc);
            ClampWhitelistLayout(rc.right - rc.left, rc.bottom - rc.top);
            RebuildWhitelistLayoutOnly();
        }

        InvalidateWhitelistArea(oldBounds);
        whitelistLayoutDirty_ = false;
        whitelistLayoutNeedsRebuild_ = false;
    }

    bool shouldSave = forceSave;
    if (!shouldSave && whitelistLayoutSavePending_) {
        DWORD elapsed = GetTickCount() - whitelistLayoutLastChangeTick_;
        shouldSave = elapsed >= kWhitelistLayoutSaveDelayMs;
    }

    if (shouldSave && whitelistLayoutSavePending_) {
        SaveWhitelistLayoutSettings();
        whitelistLayoutSavePending_ = false;
    }

    if (!whitelistLayoutDirty_ && !whitelistLayoutSavePending_) {
        KillTimer(hwnd_, kWhitelistLayoutTimer);
    }
}

void FocusClockApp::InvalidateWhitelistArea(const RECT& previousBounds) {
    RECT oldRect = previousBounds;
    RECT newRect = whitelistBounds_;
    InflateRect(&oldRect, kMaxWhitelistIconSize + 64, 64);
    InflateRect(&newRect, kMaxWhitelistIconSize + 64, 64);
    InvalidateRect(hwnd_, &oldRect, FALSE);
    InvalidateRect(hwnd_, &newRect, FALSE);
}

void FocusClockApp::ClampWhitelistLayout(int clientWidth, int clientHeight) {
    whitelistIconSize_ = std::clamp(whitelistIconSize_, kMinWhitelistIconSize, kMaxWhitelistIconSize);

    const int appColumns = 3;
    const int appGap = 14;
    int gridWidth = appColumns * whitelistIconSize_ + (appColumns - 1) * appGap;
    int minLeft = 12;
    int minTop = 12;
    int maxLeft = std::max(minLeft, clientWidth - gridWidth - 12);
    int maxTop = std::max(minTop, clientHeight - whitelistIconSize_ - 52);

    if (whitelistLeft_ < 0) {
        whitelistLeft_ = maxLeft;
    }
    whitelistLeft_ = std::clamp(whitelistLeft_, minLeft, maxLeft);
    whitelistTop_ = std::clamp(whitelistTop_, minTop, maxTop);
}

void FocusClockApp::LoadWhitelistLayoutSettings() {
    std::wstring path = GetExecutableDirectory() + L"\\FocusClock.ini";
    whitelistLeft_ = GetPrivateProfileIntW(L"Whitelist", L"Left", -1, path.c_str());
    whitelistTop_ = GetPrivateProfileIntW(L"Whitelist", L"Top", 92, path.c_str());
    whitelistIconSize_ = GetPrivateProfileIntW(L"Whitelist", L"IconSize", kDefaultWhitelistIconSize, path.c_str());
    whitelistIconSize_ = std::clamp(whitelistIconSize_, kMinWhitelistIconSize, kMaxWhitelistIconSize);
}

void FocusClockApp::SaveWhitelistLayoutSettings() const {
    std::wstring path = GetExecutableDirectory() + L"\\FocusClock.ini";
    wchar_t buffer[32]{};

    swprintf_s(buffer, L"%d", whitelistLeft_);
    WritePrivateProfileStringW(L"Whitelist", L"Left", buffer, path.c_str());

    swprintf_s(buffer, L"%d", whitelistTop_);
    WritePrivateProfileStringW(L"Whitelist", L"Top", buffer, path.c_str());

    swprintf_s(buffer, L"%d", whitelistIconSize_);
    WritePrivateProfileStringW(L"Whitelist", L"IconSize", buffer, path.c_str());
}

void FocusClockApp::LoadAppSettings() {
    std::wstring path = GetSettingsPath();
    maxFocusMinutes_ = GetPrivateProfileIntW(L"Focus", L"MaxMinutes", kDefaultMaxFocusMinutes, path.c_str());
    maxFocusMinutes_ = std::clamp(maxFocusMinutes_, kMinFocusMinutes, kAbsoluteMaxFocusMinutes);

    defaultFocusMinutes_ = GetPrivateProfileIntW(L"Focus", L"DefaultMinutes", kDefaultFocusMinutes, path.c_str());
    defaultFocusMinutes_ = std::clamp(defaultFocusMinutes_, kMinFocusMinutes, maxFocusMinutes_);

    selectedMinutes_ = defaultFocusMinutes_;
    remainingSeconds_ = selectedMinutes_ * 60LL;
}

void FocusClockApp::SaveAppSettings() const {
    std::wstring path = GetSettingsPath();
    wchar_t buffer[32]{};

    swprintf_s(buffer, L"%d", maxFocusMinutes_);
    WritePrivateProfileStringW(L"Focus", L"MaxMinutes", buffer, path.c_str());

    swprintf_s(buffer, L"%d", defaultFocusMinutes_);
    WritePrivateProfileStringW(L"Focus", L"DefaultMinutes", buffer, path.c_str());
}

void FocusClockApp::SaveFocusSessionSettings(long long durationSeconds) const {
    FILETIME startUtc{};
    GetSystemTimeAsFileTime(&startUtc);

    ULARGE_INTEGER endValue{};
    endValue.LowPart = startUtc.dwLowDateTime;
    endValue.HighPart = startUtc.dwHighDateTime;
    endValue.QuadPart += static_cast<ULONGLONG>(std::max(1LL, durationSeconds)) * 10000000ULL;

    FILETIME endUtc{};
    endUtc.dwLowDateTime = endValue.LowPart;
    endUtc.dwHighDateTime = endValue.HighPart;

    FILETIME startLocalFileTime{};
    FILETIME endLocalFileTime{};
    SYSTEMTIME startLocal{};
    SYSTEMTIME endLocal{};
    FileTimeToLocalFileTime(&startUtc, &startLocalFileTime);
    FileTimeToLocalFileTime(&endUtc, &endLocalFileTime);
    FileTimeToSystemTime(&startLocalFileTime, &startLocal);
    FileTimeToSystemTime(&endLocalFileTime, &endLocal);

    std::wstring path = GetSettingsPath();
    wchar_t buffer[64]{};

    WritePrivateProfileStringW(L"FocusSession", L"Active", L"1", path.c_str());

    swprintf_s(buffer, L"%lld", durationSeconds);
    WritePrivateProfileStringW(L"FocusSession", L"DurationSeconds", buffer, path.c_str());

    swprintf_s(buffer, L"%lld", (durationSeconds + 59) / 60);
    WritePrivateProfileStringW(L"FocusSession", L"DurationMinutes", buffer, path.c_str());

    swprintf_s(buffer, L"%lld", FileTimeToUnixSeconds(startUtc));
    WritePrivateProfileStringW(L"FocusSession", L"StartUnix", buffer, path.c_str());

    swprintf_s(buffer, L"%lld", FileTimeToUnixSeconds(endUtc));
    WritePrivateProfileStringW(L"FocusSession", L"EndUnix", buffer, path.c_str());

    WritePrivateProfileStringW(L"FocusSession", L"StartLocal", FormatConfigDateTime(startLocal).c_str(), path.c_str());
    WritePrivateProfileStringW(L"FocusSession", L"EndLocal", FormatConfigDateTime(endLocal).c_str(), path.c_str());
}

void FocusClockApp::ClearFocusSessionSettings() const {
    WritePrivateProfileStringW(L"FocusSession", nullptr, nullptr, GetSettingsPath().c_str());
}

void FocusClockApp::ResetAppSettings() {
    SetMaxFocusMinutes(kDefaultMaxFocusMinutes);
    SetDefaultFocusMinutes(kDefaultFocusMinutes);
}

void FocusClockApp::TogglePanel() {
    if (focusActive_) {
        return;
    }

    panelOpen_ = !panelOpen_;
    RebuildLayout();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void FocusClockApp::ClosePanel() {
    if (!panelOpen_) {
        return;
    }

    panelOpen_ = false;
    RebuildLayout();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void FocusClockApp::AddPanelButton(int id, const RECT& rect, const std::wstring& label, bool selected, bool enabled, bool primary, bool danger) {
    UiButton button{};
    button.rect = rect;
    button.label = label;
    button.id = id;
    button.selected = selected;
    button.enabled = enabled;
    button.primary = primary;
    button.danger = danger;
    buttons_.push_back(button);
}

void FocusClockApp::AddStepperButtons(int minusFiveId, int minusOneId, int displayId, int plusOneId, int plusFiveId, int left, int top, int value, const std::wstring& suffix, bool canDecrease, bool canIncrease) {
    const int buttonHeight = 44;
    const int smallWidth = 58;
    const int displayWidth = 170;
    const int gap = 10;

    AddPanelButton(minusFiveId, RECT{ left, top, left + smallWidth, top + buttonHeight }, L"-5", false, canDecrease);
    left += smallWidth + gap;
    AddPanelButton(minusOneId, RECT{ left, top, left + smallWidth, top + buttonHeight }, L"-1", false, canDecrease);
    left += smallWidth + gap;
    AddPanelButton(displayId, RECT{ left, top, left + displayWidth, top + buttonHeight }, std::to_wstring(value) + suffix, true, false);
    left += displayWidth + gap;
    AddPanelButton(plusOneId, RECT{ left, top, left + smallWidth, top + buttonHeight }, L"+1", false, canIncrease);
    left += smallWidth + gap;
    AddPanelButton(plusFiveId, RECT{ left, top, left + smallWidth, top + buttonHeight }, L"+5", false, canIncrease);
}

void FocusClockApp::AddTimeStepperButtons(int minusId, int displayId, int plusId, int left, int top, int value, int maxValue, const std::wstring& suffix) {
    const int buttonHeight = 44;
    const int smallWidth = 42;
    const int displayWidth = 112;
    const int gap = 8;

    AddPanelButton(minusId, RECT{ left, top, left + smallWidth, top + buttonHeight }, L"-", false, true);
    left += smallWidth + gap;

    wchar_t buffer[32]{};
    swprintf_s(buffer, L"%02d%s", std::clamp(value, 0, maxValue), suffix.c_str());
    AddPanelButton(displayId, RECT{ left, top, left + displayWidth, top + buttonHeight }, buffer, true, false);
    left += displayWidth + gap;

    AddPanelButton(plusId, RECT{ left, top, left + smallWidth, top + buttonHeight }, L"+", false, true);
}

void FocusClockApp::HandlePanelCommand(int id) {
    if (id == kPanelCloseButtonId) {
        ClosePanel();
    } else if (id >= kPanelTabButtonBaseId && id < kSettingMaxMinusFiveId) {
        activePanelTab_ = id - kPanelTabButtonBaseId;
        RebuildLayout();
        InvalidateRect(hwnd_, nullptr, FALSE);
    } else if (id == kPanelResetSettingsId) {
        ResetAppSettings();
    } else if (id == kSettingMaxMinusFiveId) {
        SetMaxFocusMinutes(maxFocusMinutes_ - 5);
    } else if (id == kSettingMaxMinusOneId) {
        SetMaxFocusMinutes(maxFocusMinutes_ - 1);
    } else if (id == kSettingMaxPlusOneId) {
        SetMaxFocusMinutes(maxFocusMinutes_ + 1);
    } else if (id == kSettingMaxPlusFiveId) {
        SetMaxFocusMinutes(maxFocusMinutes_ + 5);
    } else if (id == kSettingDefaultMinusFiveId) {
        SetDefaultFocusMinutes(defaultFocusMinutes_ - 5);
    } else if (id == kSettingDefaultMinusOneId) {
        SetDefaultFocusMinutes(defaultFocusMinutes_ - 1);
    } else if (id == kSettingDefaultPlusOneId) {
        SetDefaultFocusMinutes(defaultFocusMinutes_ + 1);
    } else if (id == kSettingDefaultPlusFiveId) {
        SetDefaultFocusMinutes(defaultFocusMinutes_ + 5);
    } else if (id >= kScheduleStartHourMinusId && id <= kScheduleEndMinutePlusId) {
        AdjustScheduleDraft(id);
    } else if (id == kScheduleAddTaskId) {
        AddScheduledFocusTask();
    } else if (id >= kScheduleDeleteButtonBaseId && id < kScheduleDeleteButtonLimit) {
        DeleteScheduledFocusTask(static_cast<size_t>(id - kScheduleDeleteButtonBaseId));
    } else if (id == kRerunCreateTaskId) {
        CreateRerunStartupTask();
    }
}

void FocusClockApp::SetMaxFocusMinutes(int minutes) {
    maxFocusMinutes_ = std::clamp(minutes, kMinFocusMinutes, kAbsoluteMaxFocusMinutes);
    defaultFocusMinutes_ = std::clamp(defaultFocusMinutes_, kMinFocusMinutes, maxFocusMinutes_);
    selectedMinutes_ = std::clamp(selectedMinutes_, kMinFocusMinutes, maxFocusMinutes_);
    remainingSeconds_ = selectedMinutes_ * 60LL;
    SaveAppSettings();
    RebuildLayout();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void FocusClockApp::SetDefaultFocusMinutes(int minutes) {
    defaultFocusMinutes_ = std::clamp(minutes, kMinFocusMinutes, maxFocusMinutes_);
    selectedMinutes_ = defaultFocusMinutes_;
    remainingSeconds_ = selectedMinutes_ * 60LL;
    SaveAppSettings();
    RebuildLayout();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void FocusClockApp::AdjustScheduleDraft(int id) {
    auto wrap = [](int value, int maxValue) {
        if (value < 0) {
            return maxValue;
        }
        if (value > maxValue) {
            return 0;
        }
        return value;
    };

    if (id == kScheduleStartHourMinusId) {
        scheduleDraftStartHour_ = wrap(scheduleDraftStartHour_ - 1, 23);
    } else if (id == kScheduleStartHourPlusId) {
        scheduleDraftStartHour_ = wrap(scheduleDraftStartHour_ + 1, 23);
    } else if (id == kScheduleStartMinuteMinusId) {
        scheduleDraftStartMinute_ = wrap(scheduleDraftStartMinute_ - 5, 55);
    } else if (id == kScheduleStartMinutePlusId) {
        scheduleDraftStartMinute_ = wrap(scheduleDraftStartMinute_ + 5, 55);
    } else if (id == kScheduleEndHourMinusId) {
        scheduleDraftEndHour_ = wrap(scheduleDraftEndHour_ - 1, 23);
    } else if (id == kScheduleEndHourPlusId) {
        scheduleDraftEndHour_ = wrap(scheduleDraftEndHour_ + 1, 23);
    } else if (id == kScheduleEndMinuteMinusId) {
        scheduleDraftEndMinute_ = wrap(scheduleDraftEndMinute_ - 5, 55);
    } else if (id == kScheduleEndMinutePlusId) {
        scheduleDraftEndMinute_ = wrap(scheduleDraftEndMinute_ + 5, 55);
    }

    scheduleMessage_.clear();
    scheduleMessageIsError_ = false;
    RebuildLayout();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void FocusClockApp::AddScheduledFocusTask() {
    int startMinute = ScheduleDraftStartMinute();
    int endMinute = ScheduleDraftEndMinute();
    if (!IsValidScheduleRange(startMinute, endMinute)) {
        int duration = endMinute - startMinute;
        if (duration > maxFocusMinutes_) {
            scheduleMessage_ = L"添加失败：计划时长不能超过最大专注时长。";
        } else {
            scheduleMessage_ = L"添加失败：结束时间必须晚于开始时间。";
        }
        scheduleMessageIsError_ = true;
    } else if (HasScheduleConflict(startMinute, endMinute)) {
        scheduleMessage_ = L"添加失败：该时间段与已有计划冲突。";
        scheduleMessageIsError_ = true;
    } else {
        scheduledTasks_.push_back(ScheduledFocusTask{ startMinute, endMinute, 0 });
        std::sort(scheduledTasks_.begin(), scheduledTasks_.end(), [](const ScheduledFocusTask& left, const ScheduledFocusTask& right) {
            return left.startMinute < right.startMinute;
        });
        SaveScheduledFocusTasks();
        scheduleMessage_ = L"已添加计划 " + FormatMinuteOfDay(startMinute) + L" - " + FormatMinuteOfDay(endMinute) + L"。";
        scheduleMessageIsError_ = false;
    }

    RebuildLayout();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void FocusClockApp::DeleteScheduledFocusTask(size_t index) {
    if (index >= scheduledTasks_.size()) {
        return;
    }

    scheduledTasks_.erase(scheduledTasks_.begin() + static_cast<std::ptrdiff_t>(index));
    SaveScheduledFocusTasks();
    scheduleMessage_ = L"已删除计划。";
    scheduleMessageIsError_ = false;
    RebuildLayout();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void FocusClockApp::ScrollScheduleTab(int delta) {
    int maxScroll = ScheduleContentMaxScroll();
    int next = std::clamp(scheduleScrollOffset_ + delta, 0, maxScroll);
    if (next == scheduleScrollOffset_) {
        return;
    }

    scheduleScrollOffset_ = next;
    RebuildLayout();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

int FocusClockApp::ScheduleContentMaxScroll() const {
    if (panelBounds_.bottom <= panelBounds_.top) {
        return 0;
    }

    int viewportHeight = std::max(1, static_cast<int>(panelBounds_.bottom - panelBounds_.top - 126));
    int contentHeight = 450 + static_cast<int>(scheduledTasks_.size()) * 34;
    return std::max(0, contentHeight - viewportHeight);
}

RECT FocusClockApp::PanelContentViewport() const {
    if (panelBounds_.bottom <= panelBounds_.top) {
        return RECT{};
    }

    return RECT{
        panelBounds_.left + 216,
        panelBounds_.top + 92,
        panelBounds_.right - 32,
        panelBounds_.bottom - 34
    };
}

void FocusClockApp::LoadScheduledFocusTasks() {
    std::wstring path = GetSettingsPath();
    int count = GetPrivateProfileIntW(L"Schedule", L"Count", 0, path.c_str());
    count = std::clamp(count, 0, 64);

    scheduledTasks_.clear();
    for (int i = 0; i < count; ++i) {
        std::wstring key = L"Task" + std::to_wstring(i);
        wchar_t buffer[64]{};
        GetPrivateProfileStringW(L"Schedule", key.c_str(), L"", buffer, static_cast<DWORD>(std::size(buffer)), path.c_str());

        std::wstring value = buffer;
        size_t comma = value.find(L',');
        if (comma == std::wstring::npos) {
            continue;
        }

        size_t secondComma = value.find(L',', comma + 1);
        int startMinute = _wtoi(value.substr(0, comma).c_str());
        int endMinute = _wtoi(value.substr(comma + 1, secondComma == std::wstring::npos ? std::wstring::npos : secondComma - comma - 1).c_str());
        int lastStartedDate = secondComma == std::wstring::npos ? 0 : _wtoi(value.substr(secondComma + 1).c_str());
        if (!IsValidScheduleRange(startMinute, endMinute) || HasScheduleConflict(startMinute, endMinute)) {
            continue;
        }
        scheduledTasks_.push_back(ScheduledFocusTask{ startMinute, endMinute, lastStartedDate });
    }

    std::sort(scheduledTasks_.begin(), scheduledTasks_.end(), [](const ScheduledFocusTask& left, const ScheduledFocusTask& right) {
        return left.startMinute < right.startMinute;
    });
}

void FocusClockApp::SaveScheduledFocusTasks() const {
    std::wstring path = GetSettingsPath();
    wchar_t buffer[64]{};

    int oldCount = GetPrivateProfileIntW(L"Schedule", L"Count", 0, path.c_str());
    oldCount = std::clamp(oldCount, 0, 128);
    for (int i = 0; i < oldCount; ++i) {
        std::wstring key = L"Task" + std::to_wstring(i);
        WritePrivateProfileStringW(L"Schedule", key.c_str(), nullptr, path.c_str());
    }

    swprintf_s(buffer, L"%d", static_cast<int>(scheduledTasks_.size()));
    WritePrivateProfileStringW(L"Schedule", L"Count", buffer, path.c_str());

    for (size_t i = 0; i < scheduledTasks_.size(); ++i) {
        std::wstring key = L"Task" + std::to_wstring(i);
        swprintf_s(buffer, L"%d,%d,%d", scheduledTasks_[i].startMinute, scheduledTasks_[i].endMinute, scheduledTasks_[i].lastStartedDate);
        WritePrivateProfileStringW(L"Schedule", key.c_str(), buffer, path.c_str());
    }
}

void FocusClockApp::CheckScheduledFocusTasks(bool forceResumeActiveRange) {
    if (focusActive_ || scheduledTasks_.empty()) {
        return;
    }

    SYSTEMTIME now{};
    GetLocalTime(&now);
    int today = DateStamp(now);
    int minuteOfDay = now.wHour * 60 + now.wMinute;
    int secondOfDay = minuteOfDay * 60 + now.wSecond;

    for (auto& task : scheduledTasks_) {
        if ((!forceResumeActiveRange && task.lastStartedDate == today) ||
            minuteOfDay < task.startMinute ||
            minuteOfDay >= task.endMinute) {
            continue;
        }

        int remainingSeconds = std::max(1, task.endMinute * 60 - secondOfDay);
        int remainingMinutes = std::max(1, (remainingSeconds + 59) / 60);
        selectedMinutes_ = std::clamp(remainingMinutes, kMinFocusMinutes, maxFocusMinutes_);
        task.lastStartedDate = today;
        SaveScheduledFocusTasks();
        scheduleMessage_ = L"计划 " + FormatMinuteOfDay(task.startMinute) + L" - " + FormatMinuteOfDay(task.endMinute) + L" 已自动开始。";
        scheduleMessageIsError_ = false;
        StartFocusForSeconds(remainingSeconds);
        return;
    }
}

void FocusClockApp::CreateRerunStartupTask() {
    std::wstring exePath(MAX_PATH, L'\0');
    DWORD length = GetModuleFileNameW(nullptr, exePath.data(), static_cast<DWORD>(exePath.size()));
    while (length == exePath.size()) {
        exePath.resize(exePath.size() * 2);
        length = GetModuleFileNameW(nullptr, exePath.data(), static_cast<DWORD>(exePath.size()));
    }

    if (length == 0) {
        rerunTaskMessage_ = L"无法获取程序路径，计划任务未创建。";
        rerunTaskMessageIsError_ = true;
        RebuildLayout();
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }

    exePath.resize(length);
    std::wstring taskRun = L"\\\"" + exePath + L"\\\" -rerun";
    std::wstring command =
        L"\"C:\\Windows\\System32\\schtasks.exe\" /Create /TN \"FocusClock Rerun\" /SC ONLOGON /RL LIMITED /TR \"" +
        taskRun +
        L"\" /F";

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION process{};
    std::vector<wchar_t> commandBuffer(command.begin(), command.end());
    commandBuffer.push_back(L'\0');

    BOOL created = CreateProcessW(
        nullptr,
        commandBuffer.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &startup,
        &process);

    if (!created) {
        rerunTaskMessage_ = L"创建计划任务失败，请确认系统允许当前用户使用任务计划程序。";
        rerunTaskMessageIsError_ = true;
    } else {
        WaitForSingleObject(process.hProcess, 10000);
        DWORD exitCode = 1;
        GetExitCodeProcess(process.hProcess, &exitCode);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);

        if (exitCode == 0) {
            rerunTaskMessage_ = L"已添加计划任务：FocusClock Rerun。下次登录时会用 -rerun 参数检查是否需要恢复专注。";
            rerunTaskMessageIsError_ = false;
        } else {
            rerunTaskMessage_ = L"计划任务命令执行失败，可以尝试以普通用户重新运行程序后再添加。";
            rerunTaskMessageIsError_ = true;
        }
    }

    RebuildLayout();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

bool FocusClockApp::HasScheduleConflict(int startMinute, int endMinute, int ignoreIndex) const {
    for (size_t i = 0; i < scheduledTasks_.size(); ++i) {
        if (static_cast<int>(i) == ignoreIndex) {
            continue;
        }

        const auto& task = scheduledTasks_[i];
        if (startMinute < task.endMinute && endMinute > task.startMinute) {
            return true;
        }
    }
    return false;
}

bool FocusClockApp::IsValidScheduleRange(int startMinute, int endMinute) const {
    return startMinute >= 0 && startMinute < 24 * 60 &&
        endMinute > startMinute && endMinute <= 24 * 60 &&
        endMinute - startMinute <= maxFocusMinutes_;
}

int FocusClockApp::ScheduleDraftStartMinute() const {
    return scheduleDraftStartHour_ * 60 + scheduleDraftStartMinute_;
}

int FocusClockApp::ScheduleDraftEndMinute() const {
    return scheduleDraftEndHour_ * 60 + scheduleDraftEndMinute_;
}

std::wstring FocusClockApp::FormatMinuteOfDay(int minute) const {
    minute = std::clamp(minute, 0, 24 * 60);
    int hour = minute / 60;
    int min = minute % 60;
    wchar_t buffer[16]{};
    swprintf_s(buffer, L"%02d:%02d", hour, min);
    return buffer;
}

int FocusClockApp::DateStamp(const SYSTEMTIME& time) {
    return static_cast<int>(time.wYear) * 10000 + static_cast<int>(time.wMonth) * 100 + static_cast<int>(time.wDay);
}

std::wstring FocusClockApp::GetSettingsPath() const {
    return GetExecutableDirectory() + L"\\FocusClock.ini";
}

void FocusClockApp::OpenWhitelistEntry(size_t index) {
    if (index >= whitelistEntries_.size()) {
        return;
    }

    const WhitelistEntry& entry = whitelistEntries_[index];
    pendingWhitelistIndex_ = static_cast<int>(index);
    whitelistYieldUntil_ = std::chrono::steady_clock::now() + std::chrono::seconds(12);

    HWND running = FindRunningWhitelistWindow(entry);
    if (running) {
        pendingWhitelistIndex_ = -1;
        activeWhitelistWindow_ = running;
        BringWindowToFront(running);
        EnterFullscreenBelow(running);
        return;
    }

    HINSTANCE result = ShellExecuteW(hwnd_, L"open", entry.launchSpec.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<intptr_t>(result) <= 32) {
        MessageBoxW(hwnd_, L"无法启动该白名单程序，请检查 Whitelist.txt 中的路径。", L"FocusClock", MB_ICONWARNING);
        whitelistYieldUntil_ = std::chrono::steady_clock::time_point{};
        pendingWhitelistIndex_ = -1;
        activeWhitelistWindow_ = nullptr;
        EnterFullscreenTopmost();
    }
}

HWND FocusClockApp::FindRunningWhitelistWindow(const WhitelistEntry& entry) const {
    FindWindowContext context{};
    context.app = this;
    context.entry = &entry;
    EnumWindows(EnumWhitelistWindows, reinterpret_cast<LPARAM>(&context));
    return context.found;
}

void FocusClockApp::BringWindowToFront(HWND target) {
    if (!target) {
        return;
    }

    PromoteWhitelistWindow(target);

    if (IsIconic(target)) {
        ShowWindow(target, SW_RESTORE);
    } else {
        ShowWindow(target, SW_SHOWNORMAL);
    }

    SetWindowPos(target, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    SetForegroundWindow(target);
}

void FocusClockApp::PromoteWhitelistWindow(HWND target) {
    if (!target) {
        return;
    }

    if (promotedWindows_.find(target) == promotedWindows_.end()) {
        promotedWindows_[target] = GetWindowLongPtrW(target, GWL_EXSTYLE);
    }

    LONG_PTR exStyle = GetWindowLongPtrW(target, GWL_EXSTYLE);
    SetWindowLongPtrW(target, GWL_EXSTYLE, exStyle | WS_EX_TOPMOST);
    SetWindowPos(target, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void FocusClockApp::RestorePromotedWhitelistWindows() {
    EnumWindows(EnumWhitelistedWindowsForRestore, reinterpret_cast<LPARAM>(this));

    for (auto const& [window, style] : promotedWindows_) {
        RestoreWhitelistWindow(window, style);
    }
    promotedWindows_.clear();
}

void FocusClockApp::RestoreWhitelistWindow(HWND target, LONG_PTR savedStyle) {
    if (!target || !IsWindow(target)) {
        return;
    }

    LONG_PTR currentStyle = GetWindowLongPtrW(target, GWL_EXSTYLE);
    LONG_PTR nextStyle = savedStyle != 0 ? savedStyle : (currentStyle & ~WS_EX_TOPMOST);
    SetWindowLongPtrW(target, GWL_EXSTYLE, nextStyle & ~WS_EX_TOPMOST);
    SetWindowPos(
        target,
        HWND_NOTOPMOST,
        0,
        0,
        0,
        0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

bool FocusClockApp::IsTrackedWhitelistWindowValid() const {
    return activeWhitelistWindow_ &&
        IsWindow(activeWhitelistWindow_) &&
        IsWindowVisible(activeWhitelistWindow_) &&
        !IsIconic(activeWhitelistWindow_) &&
        activeWhitelistWindow_ != hwnd_;
}

bool FocusClockApp::TryResolvePendingWhitelistWindow() {
    if (pendingWhitelistIndex_ < 0 || pendingWhitelistIndex_ >= static_cast<int>(whitelistEntries_.size())) {
        return false;
    }

    if (!ShouldYieldToWhitelist()) {
        pendingWhitelistIndex_ = -1;
        return false;
    }

    HWND running = FindRunningWhitelistWindow(whitelistEntries_[static_cast<size_t>(pendingWhitelistIndex_)]);
    if (!running) {
        return false;
    }

    pendingWhitelistIndex_ = -1;
    activeWhitelistWindow_ = running;
    BringWindowToFront(running);
    return true;
}

void FocusClockApp::UpdateRemaining() {
    if (!focusActive_) {
        return;
    }

    auto now = std::chrono::steady_clock::now();
    if (now >= focusEnd_) {
        FinishFocus();
        return;
    }

    remainingSeconds_ = std::chrono::duration_cast<std::chrono::seconds>(focusEnd_ - now).count();
}

std::wstring FocusClockApp::FormatTime(const SYSTEMTIME& now) const {
    wchar_t buffer[32]{};
    swprintf_s(buffer, L"%02u:%02u:%02u", now.wHour, now.wMinute, now.wSecond);
    return buffer;
}

std::wstring FocusClockApp::FormatDate(const SYSTEMTIME& now) const {
    static const std::array<const wchar_t*, 7> weekdays{
        L"星期日", L"星期一", L"星期二", L"星期三", L"星期四", L"星期五", L"星期六"
    };

    wchar_t buffer[96]{};
    swprintf_s(buffer, L"%04u 年 %02u 月 %02u 日  %s", now.wYear, now.wMonth, now.wDay, weekdays[now.wDayOfWeek]);
    return buffer;
}

std::wstring FocusClockApp::FormatRemaining() const {
    long long seconds = std::max(0LL, remainingSeconds_);
    long long minutes = seconds / 60;
    long long secs = seconds % 60;

    wchar_t buffer[32]{};
    swprintf_s(buffer, L"%02lld:%02lld", minutes, secs);
    return buffer;
}

bool FocusClockApp::IsSystemDarkMode() const {
    DWORD value = 1;
    DWORD size = sizeof(value);
    LSTATUS status = RegGetValueW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme",
        RRF_RT_REG_DWORD,
        nullptr,
        &value,
        &size);

    return status == ERROR_SUCCESS && value == 0;
}

bool FocusClockApp::IsWhitelistedForegroundWindow() {
    HWND foreground = GetForegroundWindow();
    if (!foreground || foreground == hwnd_) {
        return false;
    }

    std::wstring path = GetProcessImagePath(foreground);
    if (!path.empty() && IsExecutableWhitelisted(path)) {
        activeWhitelistWindow_ = foreground;
        PromoteWhitelistWindow(foreground);
        return true;
    }

    return false;
}

bool FocusClockApp::IsExecutableWhitelisted(const std::wstring& path) const {
    std::wstring normalizedPath = ToLower(path);
    std::wstring exeName = BaseName(normalizedPath);

    for (const auto& entry : whitelistEntries_) {
        if (entry.normalized == normalizedPath || entry.exeName == exeName) {
            return true;
        }
    }
    return false;
}

bool FocusClockApp::ShouldYieldToWhitelist() const {
    return whitelistYieldUntil_ != std::chrono::steady_clock::time_point{} &&
        std::chrono::steady_clock::now() < whitelistYieldUntil_;
}

std::wstring FocusClockApp::GetExecutableDirectory() const {
    std::wstring buffer(MAX_PATH, L'\0');
    DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    while (length == buffer.size()) {
        buffer.resize(buffer.size() * 2);
        length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    }

    buffer.resize(length);
    size_t slash = buffer.find_last_of(L"\\/");
    if (slash == std::wstring::npos) {
        return L".";
    }
    return buffer.substr(0, slash);
}

std::wstring FocusClockApp::GetProcessImagePath(HWND window) const {
    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);
    if (!pid) {
        return L"";
    }

    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) {
        return L"";
    }

    std::wstring path(32768, L'\0');
    DWORD size = static_cast<DWORD>(path.size());
    BOOL ok = QueryFullProcessImageNameW(process, 0, path.data(), &size);
    CloseHandle(process);

    if (!ok || size == 0) {
        return L"";
    }

    path.resize(size);
    return path;
}

std::wstring FocusClockApp::ToLower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return value;
}

std::wstring FocusClockApp::Trim(std::wstring value) {
    size_t first = 0;
    while (first < value.size() && std::iswspace(value[first])) {
        ++first;
    }

    size_t last = value.size();
    while (last > first && std::iswspace(value[last - 1])) {
        --last;
    }

    return value.substr(first, last - first);
}

std::wstring FocusClockApp::BaseName(const std::wstring& path) {
    size_t slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos) {
        return path;
    }
    return path.substr(slash + 1);
}

std::wstring FocusClockApp::StripExtension(const std::wstring& filename) {
    size_t dot = filename.find_last_of(L'.');
    if (dot == std::wstring::npos || dot == 0) {
        return filename;
    }
    return filename.substr(0, dot);
}

std::wstring FocusClockApp::ResolveLaunchPath(const std::wstring& launchSpec) {
    std::wstring expanded = launchSpec;
    DWORD needed = ExpandEnvironmentStringsW(launchSpec.c_str(), nullptr, 0);
    if (needed > 0) {
        std::wstring buffer(needed, L'\0');
        DWORD written = ExpandEnvironmentStringsW(launchSpec.c_str(), buffer.data(), needed);
        if (written > 0 && written <= needed) {
            buffer.resize(written - 1);
            expanded = buffer;
        }
    }

    if (expanded.find_first_of(L"\\/") != std::wstring::npos) {
        return expanded;
    }

    DWORD length = SearchPathW(nullptr, expanded.c_str(), nullptr, 0, nullptr, nullptr);
    if (length == 0) {
        return expanded;
    }

    std::wstring resolved(length, L'\0');
    DWORD written = SearchPathW(nullptr, expanded.c_str(), nullptr, length, resolved.data(), nullptr);
    if (written == 0 || written >= length) {
        return expanded;
    }

    resolved.resize(written);
    return resolved;
}

HICON FocusClockApp::LoadIconForPath(const std::wstring& path) {
    SHFILEINFOW info{};
    DWORD_PTR ok = SHGetFileInfoW(
        path.c_str(),
        FILE_ATTRIBUTE_NORMAL,
        &info,
        sizeof(info),
        SHGFI_ICON | SHGFI_LARGEICON);

    if (!ok || !info.hIcon) {
        ok = SHGetFileInfoW(
            path.c_str(),
            FILE_ATTRIBUTE_NORMAL,
            &info,
            sizeof(info),
            SHGFI_ICON | SHGFI_LARGEICON | SHGFI_USEFILEATTRIBUTES);
    }

    if (ok && info.hIcon) {
        return info.hIcon;
    }

    return CopyIcon(LoadIconW(nullptr, IDI_APPLICATION));
}

std::wstring FocusClockApp::FormatConfigDateTime(const SYSTEMTIME& time) {
    wchar_t buffer[32]{};
    swprintf_s(
        buffer,
        L"%04u-%02u-%02u %02u:%02u:%02u",
        time.wYear,
        time.wMonth,
        time.wDay,
        time.wHour,
        time.wMinute,
        time.wSecond);
    return buffer;
}

long long FocusClockApp::FileTimeToUnixSeconds(const FILETIME& fileTime) {
    ULARGE_INTEGER value{};
    value.LowPart = fileTime.dwLowDateTime;
    value.HighPart = fileTime.dwHighDateTime;
    constexpr ULONGLONG unixEpochOffset = 116444736000000000ULL;
    if (value.QuadPart < unixEpochOffset) {
        return 0;
    }
    return static_cast<long long>((value.QuadPart - unixEpochOffset) / 10000000ULL);
}

std::wstring FocusClockApp::DecodeTextFile(const std::vector<char>& bytes) {
    if (bytes.empty()) {
        return L"";
    }

    UINT codePage = CP_UTF8;
    int offset = 0;
    if (bytes.size() >= 3 &&
        static_cast<unsigned char>(bytes[0]) == 0xEF &&
        static_cast<unsigned char>(bytes[1]) == 0xBB &&
        static_cast<unsigned char>(bytes[2]) == 0xBF) {
        offset = 3;
    }

    int wideLength = MultiByteToWideChar(codePage, MB_ERR_INVALID_CHARS, bytes.data() + offset, static_cast<int>(bytes.size() - offset), nullptr, 0);
    if (wideLength == 0) {
        codePage = CP_ACP;
        wideLength = MultiByteToWideChar(codePage, 0, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);
    }

    if (wideLength == 0) {
        return L"";
    }

    std::wstring text(static_cast<size_t>(wideLength), L'\0');
    MultiByteToWideChar(codePage, codePage == CP_UTF8 ? MB_ERR_INVALID_CHARS : 0, bytes.data() + (codePage == CP_UTF8 ? offset : 0), static_cast<int>(bytes.size() - (codePage == CP_UTF8 ? offset : 0)), text.data(), wideLength);
    return text;
}

BOOL CALLBACK FocusClockApp::EnumWhitelistWindows(HWND window, LPARAM param) {
    auto* context = reinterpret_cast<FindWindowContext*>(param);
    if (!context || !context->app || !context->entry || !window || window == context->app->hwnd_) {
        return TRUE;
    }

    if (!IsWindowVisible(window) || GetWindow(window, GW_OWNER)) {
        return TRUE;
    }

    std::wstring path = context->app->GetProcessImagePath(window);
    if (path.empty()) {
        return TRUE;
    }

    std::wstring normalizedPath = ToLower(path);
    std::wstring exeName = BaseName(normalizedPath);
    if (normalizedPath == context->entry->normalized || exeName == context->entry->exeName) {
        context->found = window;
        return FALSE;
    }

    return TRUE;
}

BOOL CALLBACK FocusClockApp::EnumWhitelistedWindowsForRestore(HWND window, LPARAM param) {
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

    auto saved = app->promotedWindows_.find(window);
    LONG_PTR savedStyle = saved == app->promotedWindows_.end() ? 0 : saved->second;
    app->RestoreWhitelistWindow(window, savedStyle);
    return TRUE;
}

} // namespace

namespace {

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
    return startMinute >= 0 && endMinute <= 24 * 60 && startMinute < endMinute;
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
            minuteOfDay >= startMinute &&
            minuteOfDay < endMinute) {
            return true;
        }
    }

    return false;
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    SetProcessDPIAware();

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
