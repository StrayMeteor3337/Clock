#include "FocusClockApp.h"

namespace focus_clock {

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

    rerunTaskMessage_ = L"状态：正在添加...";
    rerunTaskMessageIsError_ = false;
    RunRerunTaskCommandAsync(command, kRerunTaskCreateCommand, 10000);
    RebuildLayout();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void FocusClockApp::CheckRerunStartupTaskStatus() {
    std::wstring command = L"\"C:\\Windows\\System32\\schtasks.exe\" /Query /TN \"FocusClock Rerun\"";

    rerunTaskMessage_ = L"状态：正在检查...";
    rerunTaskMessageIsError_ = false;
    RunRerunTaskCommandAsync(command, kRerunTaskStatusCommand, 5000);
    RebuildLayout();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void FocusClockApp::RunRerunTaskCommandAsync(std::wstring command, WPARAM commandKind, DWORD timeoutMs) {
    HWND target = hwnd_;
    std::thread([command = std::move(command), target, commandKind, timeoutMs]() mutable {
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

        DWORD result = kRerunTaskLaunchFailed;
        if (created) {
            DWORD waitResult = WaitForSingleObject(process.hProcess, timeoutMs);
            if (waitResult == WAIT_OBJECT_0) {
                DWORD exitCode = 1;
                if (GetExitCodeProcess(process.hProcess, &exitCode)) {
                    result = exitCode;
                }
            } else if (waitResult == WAIT_TIMEOUT) {
                TerminateProcess(process.hProcess, 1);
                result = kRerunTaskTimedOut;
            }

            CloseHandle(process.hThread);
            CloseHandle(process.hProcess);
        }

        PostMessageW(target, kRerunTaskCommandFinishedMessage, commandKind, static_cast<LPARAM>(result));
    }).detach();
}

void FocusClockApp::HandleRerunTaskCommandResult(WPARAM commandKind, LPARAM result) {
    DWORD exitCode = static_cast<DWORD>(result);
    if (commandKind == kRerunTaskCreateCommand) {
        if (exitCode == 0) {
            rerunTaskMessage_ = L"已添加计划任务：FocusClock Rerun。下次登录时会用 -rerun 参数检查是否需要恢复专注。";
            rerunTaskMessageIsError_ = false;
        } else if (exitCode == kRerunTaskTimedOut) {
            rerunTaskMessage_ = L"创建计划任务超时，请稍后重试。";
            rerunTaskMessageIsError_ = true;
        } else if (exitCode == kRerunTaskLaunchFailed) {
            rerunTaskMessage_ = L"创建计划任务失败，请确认系统允许当前用户使用任务计划程序。";
            rerunTaskMessageIsError_ = true;
        } else {
            rerunTaskMessage_ = L"计划任务命令执行失败，可以尝试以普通用户重新运行程序后再添加。";
            rerunTaskMessageIsError_ = true;
        }
    } else if (commandKind == kRerunTaskStatusCommand) {
        if (exitCode == 0) {
            rerunTaskMessage_ = L"状态：已添加";
            rerunTaskMessageIsError_ = false;
        } else if (exitCode == kRerunTaskTimedOut) {
            rerunTaskMessage_ = L"状态：检查超时";
            rerunTaskMessageIsError_ = true;
        } else if (exitCode == kRerunTaskLaunchFailed) {
            rerunTaskMessage_ = L"状态：无法检查计划任务";
            rerunTaskMessageIsError_ = true;
        } else {
            rerunTaskMessage_ = L"状态：未添加";
            rerunTaskMessageIsError_ = false;
        }
    }

    RebuildLayout();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

} // namespace focus_clock
