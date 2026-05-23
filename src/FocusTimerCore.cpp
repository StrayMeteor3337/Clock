#include "FocusClockApp.h"

extern FocusClockApp* gKeyboardHookApp;

FocusClockApp::~FocusClockApp() {
    ReleaseRenderResources();
    DestroyWhitelistIconCache();
}

int FocusClockApp::Run(HINSTANCE instance, int show, long long rerunResumeSeconds) {
    rerunResumeSeconds_ = rerunResumeSeconds;

    GdiplusStartupInput gdiplusStartupInput;
    if (GdiplusStartup(&gdiplusToken_, &gdiplusStartupInput, nullptr) != Ok) {
        MessageBoxW(nullptr, L"无法初始化 GDI+。", L"FocusClock", MB_ICONERROR);
        return 1;
    }

    uiFontFamily_ = std::make_unique<FontFamily>(L"Segoe UI");
    monoFontFamily_ = std::make_unique<FontFamily>(L"Consolas");

    if (!Create(instance, show)) {
        ReleaseRenderResources();
        GdiplusShutdown(gdiplusToken_);
        return 1;
    }

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    ReleaseRenderResources();
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
        long long rerunTotalSeconds = rerunResumeSeconds_;
        wchar_t buffer[64]{};
        GetPrivateProfileStringW(
            L"FocusSession",
            L"TotalSeconds",
            L"0",
            buffer,
            static_cast<DWORD>(std::size(buffer)),
            GetSettingsPath().c_str());
        long long storedTotalSeconds = _wtoi64(buffer);
        if (storedTotalSeconds <= 0) {
            GetPrivateProfileStringW(
                L"FocusSession",
                L"DurationSeconds",
                L"0",
                buffer,
                static_cast<DWORD>(std::size(buffer)),
                GetSettingsPath().c_str());
            storedTotalSeconds = _wtoi64(buffer);
        }
        if (storedTotalSeconds > 0) {
            rerunTotalSeconds = std::max(rerunResumeSeconds_, storedTotalSeconds);
        }
        StartFocusForSeconds(rerunResumeSeconds_, false, rerunTotalSeconds);
    } else {
        CheckScheduledFocusTasks(true);
    }
    return true;
}

void FocusClockApp::StartFocus() {
    StartFocusForSeconds(static_cast<long long>(selectedMinutes_) * 60);
}

void FocusClockApp::StartFocusForSeconds(long long durationSeconds, bool updateSessionSettings, long long totalSeconds) {
    if (focusActive_) {
        return;
    }

    if (totalSeconds <= 0) {
        totalSeconds = durationSeconds;
    }

    focusActive_ = true;
    focusEnd_ = std::chrono::steady_clock::now() + std::chrono::seconds(durationSeconds);
    remainingSeconds_ = durationSeconds;
    focusTotalSeconds_ = totalSeconds;

    if (updateSessionSettings) {
        SaveFocusSessionSettings(durationSeconds, totalSeconds);
    } else {
        ClearFocusSessionSettings();
    }

    RefreshTheme();
    InstallFocusKeyboardHook();
    RebuildLayout();
    ClosePanel();
    EnterFullscreenTopmost();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void FocusClockApp::FinishFocus() {
    if (!focusActive_) {
        return;
    }

    focusActive_ = false;
    focusEnd_ = std::chrono::steady_clock::time_point{};
    remainingSeconds_ = 0;
    pendingWhitelistIndex_ = -1;
    whitelistYieldUntil_ = std::chrono::steady_clock::time_point{};

    RestorePromotedWhitelistWindows();
    RemoveFocusKeyboardHook();
    ClearFocusSessionSettings();
    RefreshTheme();
    RebuildLayout();
    EnterFullscreenNotTopmost();
    CheckScheduledFocusTasks();
    InvalidateRect(hwnd_, nullptr, FALSE);
}
