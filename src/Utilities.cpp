#include "FocusClockApp.h"

namespace focus_clock {

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

bool FocusClockApp::IsExecutableFileName(const std::wstring& filename) {
    std::wstring lower = ToLower(filename);
    return lower.size() > 4 && lower.substr(lower.size() - 4) == L".exe";
}

std::wstring FocusClockApp::JoinPath(const std::wstring& directory, const std::wstring& name) {
    if (directory.empty()) {
        return name;
    }

    wchar_t last = directory.back();
    if (last == L'\\' || last == L'/') {
        return directory + name;
    }
    return directory + L"\\" + name;
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
    if (offset >= static_cast<int>(bytes.size())) {
        return L"";
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

bool FocusClockApp::TryRunCommand(std::wstring command, DWORD timeoutMs, DWORD& exitCode) {
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
        return false;
    }

    DWORD waitResult = WaitForSingleObject(process.hProcess, timeoutMs);
    bool ok = false;
    if (waitResult == WAIT_OBJECT_0) {
        ok = GetExitCodeProcess(process.hProcess, &exitCode) != FALSE;
    } else if (waitResult == WAIT_TIMEOUT) {
        TerminateProcess(process.hProcess, 1);
        exitCode = kRerunTaskTimedOut;
    }

    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return ok;
}

bool FocusClockApp::LaunchDetachedCommand(std::wstring command) {
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
        return false;
    }

    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return true;
}

bool FocusClockApp::LaunchDetachedProcess(const std::wstring& application, const std::wstring& arguments) {
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION process{};
    std::wstring command = arguments;
    std::vector<wchar_t> commandBuffer(command.begin(), command.end());
    commandBuffer.push_back(L'\0');

    BOOL created = CreateProcessW(
        application.c_str(),
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
        return false;
    }

    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return true;
}

std::wstring FocusClockApp::QuoteCommandArgument(const std::wstring& value) {
    std::wstring quoted = L"\"";
    for (wchar_t ch : value) {
        if (ch == L'"') {
            quoted += L"\\\"";
        } else {
            quoted.push_back(ch);
        }
    }
    quoted += L"\"";
    return quoted;
}

std::wstring FocusClockApp::QuotePowerShellString(const std::wstring& value) {
    std::wstring quoted = L"'";
    for (wchar_t ch : value) {
        if (ch == L'\'') {
            quoted += L"''";
        } else {
            quoted.push_back(ch);
        }
    }
    quoted += L"'";
    return quoted;
}

std::wstring FocusClockApp::SanitizeFileName(std::wstring value) {
    value = BaseName(value);
    const std::wstring invalid = L"<>:\"/\\|?*";
    for (wchar_t& ch : value) {
        if (ch < 32 || invalid.find(ch) != std::wstring::npos) {
            ch = L'_';
        }
    }
    return Trim(value);
}

} // namespace focus_clock
