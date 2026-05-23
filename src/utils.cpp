#include "FocusClockApp.h"

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
