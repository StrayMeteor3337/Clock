#include "FocusClockApp.h"

void FocusClockApp::LoadAppSettings() {
    std::wstring path = GetSettingsPath();
    maxFocusMinutes_ = GetPrivateProfileIntW(L"Settings", L"MaxMinutes", kDefaultMaxFocusMinutes, path.c_str());
    maxFocusMinutes_ = std::clamp(maxFocusMinutes_, kMinFocusMinutes, kAbsoluteMaxFocusMinutes);

    defaultFocusMinutes_ = GetPrivateProfileIntW(L"Settings", L"DefaultMinutes", kDefaultFocusMinutes, path.c_str());
    defaultFocusMinutes_ = std::clamp(defaultFocusMinutes_, kMinFocusMinutes, maxFocusMinutes_);

    selectedMinutes_ = defaultFocusMinutes_;
}

void FocusClockApp::SaveAppSettings() const {
    std::wstring path = GetSettingsPath();
    wchar_t buffer[64]{};

    swprintf_s(buffer, L"%d", maxFocusMinutes_);
    WritePrivateProfileStringW(L"Settings", L"MaxMinutes", buffer, path.c_str());

    swprintf_s(buffer, L"%d", defaultFocusMinutes_);
    WritePrivateProfileStringW(L"Settings", L"DefaultMinutes", buffer, path.c_str());
}

void FocusClockApp::ResetAppSettings() {
    maxFocusMinutes_ = kDefaultMaxFocusMinutes;
    defaultFocusMinutes_ = kDefaultFocusMinutes;
    selectedMinutes_ = kDefaultFocusMinutes;
    SaveAppSettings();
}

void FocusClockApp::SetMaxFocusMinutes(int minutes) {
    minutes = std::clamp(minutes, kMinFocusMinutes, kAbsoluteMaxFocusMinutes);
    if (minutes >= defaultFocusMinutes_) {
        maxFocusMinutes_ = minutes;
        if (selectedMinutes_ > maxFocusMinutes_) {
            selectedMinutes_ = maxFocusMinutes_;
        }
        SaveAppSettings();
    }
}

void FocusClockApp::SetDefaultFocusMinutes(int minutes) {
    minutes = std::clamp(minutes, kMinFocusMinutes, maxFocusMinutes_);
    defaultFocusMinutes_ = minutes;
    selectedMinutes_ = minutes;
    SaveAppSettings();
    RebuildLayout();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void FocusClockApp::LoadWhitelistLayoutSettings() {
    std::wstring path = GetSettingsPath();
    whitelistLeft_ = GetPrivateProfileIntW(L"WhitelistLayout", L"Left", -1, path.c_str());
    whitelistTop_ = GetPrivateProfileIntW(L"WhitelistLayout", L"Top", 92, path.c_str());
    whitelistIconSize_ = GetPrivateProfileIntW(L"WhitelistLayout", L"IconSize", kDefaultWhitelistIconSize, path.c_str());
    whitelistIconSize_ = std::clamp(whitelistIconSize_, kMinWhitelistIconSize, kMaxWhitelistIconSize);
}

void FocusClockApp::SaveWhitelistLayoutSettings() const {
    std::wstring path = GetSettingsPath();
    wchar_t buffer[64]{};

    swprintf_s(buffer, L"%d", whitelistLeft_);
    WritePrivateProfileStringW(L"WhitelistLayout", L"Left", buffer, path.c_str());

    swprintf_s(buffer, L"%d", whitelistTop_);
    WritePrivateProfileStringW(L"WhitelistLayout", L"Top", buffer, path.c_str());

    swprintf_s(buffer, L"%d", whitelistIconSize_);
    WritePrivateProfileStringW(L"WhitelistLayout", L"IconSize", buffer, path.c_str());
}

void FocusClockApp::SaveFocusSessionSettings(long long durationSeconds, long long totalSeconds) const {
    std::wstring path = GetSettingsPath();
    wchar_t buffer[64]{};

    long long now = CurrentUnixSeconds();
    long long end = now + durationSeconds;

    WritePrivateProfileStringW(L"FocusSession", L"Active", L"1", path.c_str());

    swprintf_s(buffer, L"%lld", now);
    WritePrivateProfileStringW(L"FocusSession", L"StartUnix", buffer, path.c_str());

    swprintf_s(buffer, L"%lld", end);
    WritePrivateProfileStringW(L"FocusSession", L"EndUnix", buffer, path.c_str());

    swprintf_s(buffer, L"%lld", totalSeconds > 0 ? totalSeconds : durationSeconds);
    WritePrivateProfileStringW(L"FocusSession", L"TotalSeconds", buffer, path.c_str());
}

void FocusClockApp::ClearFocusSessionSettings() const {
    std::wstring path = GetSettingsPath();
    WritePrivateProfileStringW(L"FocusSession", L"Active", L"0", path.c_str());
    WritePrivateProfileStringW(L"FocusSession", L"StartUnix", nullptr, path.c_str());
    WritePrivateProfileStringW(L"FocusSession", L"EndUnix", nullptr, path.c_str());
    WritePrivateProfileStringW(L"FocusSession", L"TotalSeconds", nullptr, path.c_str());
}

std::wstring FocusClockApp::GetSettingsPath() const {
    return GetExecutableDirectory() + L"\\FocusClock.ini";
}

std::wstring FocusClockApp::GetExecutableDirectory() const {
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

void FocusClockApp::CreateRerunStartupTask() {
    std::wstring exePath(MAX_PATH, L'\0');
    DWORD length = GetModuleFileNameW(nullptr, exePath.data(), static_cast<DWORD>(exePath.size()));
    if (length == 0 || length >= exePath.size()) {
        return;
    }
    exePath.resize(length);

    std::wstring taskName = L"FocusClockRerun";
    std::wstring command = L"schtasks /create /tn \"" + taskName + L"\" /tr \"\\\"" + exePath + L"\\\" -rerun\" /sc ONLOGON /delay 0000:30 /rl HIGHEST /f";

    rerunTaskMessage_ = L"正在创建计划任务...";
    rerunTaskMessageIsError_ = false;
    InvalidateRect(hwnd_, nullptr, FALSE);

    RunRerunTaskCommandAsync(command, kRerunTaskCreateCommand, 10000);
}

void FocusClockApp::CheckRerunStartupTaskStatus() {
    std::wstring taskName = L"FocusClockRerun";
    std::wstring command = L"schtasks /query /tn \"" + taskName + L"\" /fo LIST /v";

    rerunTaskMessage_ = L"正在查询计划任务状态...";
    rerunTaskMessageIsError_ = false;
    InvalidateRect(hwnd_, nullptr, FALSE);

    RunRerunTaskCommandAsync(command, kRerunTaskStatusCommand, 10000);
}

void FocusClockApp::RunRerunTaskCommandAsync(std::wstring command, WPARAM commandKind, DWORD timeoutMs) {
    std::thread([this, command, commandKind, timeoutMs]() mutable {
        SECURITY_ATTRIBUTES sa{};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;

        HANDLE readPipe = nullptr;
        HANDLE writePipe = nullptr;
        if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) {
            PostMessageW(hwnd_, kRerunTaskCommandFinishedMessage, commandKind, kRerunTaskLaunchFailed);
            return;
        }

        SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOW si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        si.hStdOutput = writePipe;
        si.hStdError = writePipe;

        PROCESS_INFORMATION pi{};
        bool created = CreateProcessW(nullptr, &command[0], nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
        if (!created) {
            CloseHandle(readPipe);
            CloseHandle(writePipe);
            PostMessageW(hwnd_, kRerunTaskCommandFinishedMessage, commandKind, kRerunTaskLaunchFailed);
            return;
        }

        CloseHandle(writePipe);

        DWORD waitResult = WaitForSingleObject(pi.hProcess, timeoutMs);
        if (waitResult != WAIT_OBJECT_0) {
            TerminateProcess(pi.hProcess, 1);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            CloseHandle(readPipe);
            PostMessageW(hwnd_, kRerunTaskCommandFinishedMessage, commandKind, kRerunTaskTimedOut);
            return;
        }

        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);

        std::vector<char> output(4096, 0);
        DWORD bytesRead = 0;
        if (ReadFile(readPipe, output.data(), static_cast<DWORD>(output.size()) - 1, &bytesRead, nullptr)) {
            output[bytesRead] = 0;
        }

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(readPipe);

        PostMessageW(hwnd_, kRerunTaskCommandFinishedMessage, commandKind, static_cast<LPARAM>(exitCode));
    }).detach();
}

void FocusClockApp::HandleRerunTaskCommandResult(WPARAM commandKind, LPARAM result) {
    if (commandKind == kRerunTaskCreateCommand) {
        if (result == 0) {
            rerunTaskMessage_ = L"计划任务创建成功！已设置为登录后延迟30秒启动。";
            rerunTaskMessageIsError_ = false;
        } else {
            rerunTaskMessage_ = L"创建计划任务失败，请尝试以管理员身份运行。";
            rerunTaskMessageIsError_ = true;
        }
    } else if (commandKind == kRerunTaskStatusCommand) {
        if (result == 0) {
            rerunTaskMessage_ = L"计划任务状态正常。";
            rerunTaskMessageIsError_ = false;
        } else {
            rerunTaskMessage_ = L"计划任务不存在或状态异常。";
            rerunTaskMessageIsError_ = true;
        }
    }

    InvalidateRect(hwnd_, nullptr, FALSE);
}