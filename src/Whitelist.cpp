#include "FocusClockApp.h"

namespace focus_clock {

bool FocusClockApp::LoadWhitelistIfNeeded(bool force) {
    std::wstring path = GetWhitelistPath();

    WIN32_FILE_ATTRIBUTE_DATA data{};
    bool exists = GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data) != FALSE;
    if (!exists) {
        if (force || whitelistKnown_) {
            DestroyWhitelistIconCache();
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
                entry.icon = LoadIconForPath(entry.iconPath);
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

    DestroyWhitelistIconCache();
    whitelistEntries_ = std::move(entries);
    whitelistWriteTime_ = data.ftLastWriteTime;
    whitelistKnown_ = true;
    return true;
}

std::wstring FocusClockApp::GetWhitelistPath() const {
    return GetExecutableDirectory() + L"\\Whitelist.txt";
}

void FocusClockApp::DestroyWhitelistIconCache() {
    for (auto& entry : whitelistEntries_) {
        if (entry.icon) {
            DestroyIcon(entry.icon);
            entry.icon = nullptr;
        }
    }
}

bool FocusClockApp::SaveWhitelistEntries(const std::vector<std::wstring>& launchSpecs) {
    std::wstring text;
    text.reserve(launchSpecs.size() * 96);
    for (const auto& spec : launchSpecs) {
        std::wstring line = Trim(spec);
        if (line.empty()) {
            continue;
        }
        text += line;
        text += L"\r\n";
    }

    int byteCount = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (byteCount <= 0 && !text.empty()) {
        return false;
    }

    std::vector<char> bytes;
    bytes.reserve(text.empty() ? 0 : static_cast<size_t>(byteCount) + 3);
    if (byteCount > 0) {
        bytes.push_back(static_cast<char>(0xEF));
        bytes.push_back(static_cast<char>(0xBB));
        bytes.push_back(static_cast<char>(0xBF));
        size_t offset = bytes.size();
        bytes.resize(offset + static_cast<size_t>(byteCount));
        int writtenText = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), bytes.data() + offset, byteCount, nullptr, nullptr);
        if (writtenText != byteCount) {
            return false;
        }
    }

    std::wstring path = GetWhitelistPath();
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD written = 0;
    BOOL ok = bytes.empty() || WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr);
    CloseHandle(file);
    return ok && written == bytes.size();
}

bool FocusClockApp::IsWhitelistPathDuplicate(const std::wstring& path) const {
    std::wstring normalizedPath = ToLower(Trim(path));
    std::wstring resolvedPath = ToLower(ResolveLaunchPath(path));
    for (const auto& entry : whitelistEntries_) {
        if (entry.normalized == normalizedPath || ToLower(entry.iconPath) == resolvedPath) {
            return true;
        }
    }
    return false;
}

void FocusClockApp::AddWhitelistPath(const std::wstring& path) {
    std::wstring trimmed = Trim(path);
    if (trimmed.empty()) {
        return;
    }

    if (IsWhitelistPathDuplicate(trimmed)) {
        whitelistMessage_ = L"添加失败：该程序已在白名单中。";
        whitelistMessageIsError_ = true;
    } else {
        std::vector<std::wstring> specs;
        specs.reserve(whitelistEntries_.size() + 1);
        for (const auto& entry : whitelistEntries_) {
            specs.push_back(entry.launchSpec);
        }
        specs.push_back(trimmed);

        if (SaveWhitelistEntries(specs)) {
            LoadWhitelistIfNeeded(true);
            whitelistMessage_ = L"已添加：" + BaseName(trimmed);
            whitelistMessageIsError_ = false;
        } else {
            whitelistMessage_ = L"添加失败：无法写入 Whitelist.txt。";
            whitelistMessageIsError_ = true;
        }
    }

    RebuildLayout();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void FocusClockApp::AddWhitelistFolder(const std::wstring& folder) {
    std::wstring trimmed = Trim(folder);
    if (trimmed.empty()) {
        return;
    }

    DWORD attributes = GetFileAttributesW(trimmed.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        whitelistMessage_ = L"添加失败：文件夹不可用。";
        whitelistMessageIsError_ = true;
        RebuildLayout();
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }

    std::vector<std::wstring> executablePaths = EnumerateExecutableFilesInDirectory(trimmed);
    if (executablePaths.empty()) {
        whitelistMessage_ = L"添加失败：该文件夹下没有找到 .exe 程序。";
        whitelistMessageIsError_ = true;
        RebuildLayout();
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }

    std::vector<std::wstring> specs;
    specs.reserve(whitelistEntries_.size() + executablePaths.size());
    std::map<std::wstring, bool> seen;
    for (const auto& entry : whitelistEntries_) {
        specs.push_back(entry.launchSpec);
        seen[entry.normalized] = true;
        seen[ToLower(entry.iconPath)] = true;
    }

    int added = 0;
    for (const auto& path : executablePaths) {
        std::wstring normalized = ToLower(path);
        if (seen.find(normalized) != seen.end()) {
            continue;
        }

        specs.push_back(path);
        seen[normalized] = true;
        ++added;
    }

    if (added == 0) {
        whitelistMessage_ = L"添加失败：文件夹内程序已在白名单中。";
        whitelistMessageIsError_ = true;
    } else if (SaveWhitelistEntries(specs)) {
        LoadWhitelistIfNeeded(true);
        whitelistMessage_ = L"已从文件夹添加 " + std::to_wstring(added) + L" 个程序。";
        whitelistMessageIsError_ = false;
    } else {
        whitelistMessage_ = L"添加失败：无法写入 Whitelist.txt。";
        whitelistMessageIsError_ = true;
    }

    RebuildLayout();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void FocusClockApp::AddWhitelistFromFileDialog() {
    wchar_t path[32768]{};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd_;
    ofn.lpstrFilter = L"程序文件 (*.exe)\0*.exe\0所有文件 (*.*)\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = static_cast<DWORD>(std::size(path));
    ofn.lpstrTitle = L"选择要加入白名单的程序";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameW(&ofn)) {
        AddWhitelistPath(path);
    }
}

void FocusClockApp::AddWhitelistFromFolderDialog() {
    HRESULT oleResult = OleInitialize(nullptr);
    bool shouldUninitializeOle = SUCCEEDED(oleResult);

    BROWSEINFOW browse{};
    browse.hwndOwner = hwnd_;
    browse.lpszTitle = L"选择要自动加入白名单的文件夹";
    browse.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_NONEWFOLDERBUTTON;

    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&browse);
    if (!pidl) {
        if (shouldUninitializeOle) {
            OleUninitialize();
        }
        return;
    }

    wchar_t folder[MAX_PATH]{};
    bool ok = SHGetPathFromIDListW(pidl, folder) != FALSE;
    CoTaskMemFree(pidl);
    if (shouldUninitializeOle) {
        OleUninitialize();
    }

    if (ok) {
        AddWhitelistFolder(folder);
    }
}

std::vector<std::wstring> FocusClockApp::EnumerateExecutableFilesInDirectory(const std::wstring& folder) const {
    std::vector<std::wstring> executablePaths;
    std::vector<std::wstring> pending{ folder };
    std::map<std::wstring, bool> visitedDirectories;

    while (!pending.empty()) {
        std::wstring current = pending.back();
        pending.pop_back();

        std::wstring normalizedDirectory = ToLower(current);
        if (visitedDirectories.find(normalizedDirectory) != visitedDirectories.end()) {
            continue;
        }
        visitedDirectories[normalizedDirectory] = true;

        WIN32_FIND_DATAW data{};
        std::wstring searchPattern = JoinPath(current, L"*");
        HANDLE find = FindFirstFileW(searchPattern.c_str(), &data);
        if (find == INVALID_HANDLE_VALUE) {
            continue;
        }

        do {
            std::wstring name = data.cFileName;
            if (name == L"." || name == L"..") {
                continue;
            }

            std::wstring path = JoinPath(current, name);
            if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                if ((data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0) {
                    pending.push_back(path);
                }
            } else if (IsExecutableFileName(name)) {
                executablePaths.push_back(path);
            }
        } while (FindNextFileW(find, &data));

        FindClose(find);
    }

    std::sort(executablePaths.begin(), executablePaths.end(), [](const std::wstring& left, const std::wstring& right) {
        return ToLower(left) < ToLower(right);
    });
    return executablePaths;
}

std::vector<ProcessPathChoice> FocusClockApp::EnumerateRunningProcessPaths() const {
    std::vector<ProcessPathChoice> choices;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return choices;
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    std::map<std::wstring, bool> seen;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID);
            if (!process) {
                continue;
            }

            std::wstring path(32768, L'\0');
            DWORD size = static_cast<DWORD>(path.size());
            BOOL ok = QueryFullProcessImageNameW(process, 0, path.data(), &size);
            CloseHandle(process);
            if (!ok || size == 0) {
                continue;
            }

            path.resize(size);
            std::wstring key = ToLower(path);
            if (seen.find(key) != seen.end()) {
                continue;
            }
            seen.emplace(std::move(key), true);
            choices.push_back(ProcessPathChoice{ entry.th32ProcessID, path, BaseName(path) });
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    std::sort(choices.begin(), choices.end(), [](const ProcessPathChoice& left, const ProcessPathChoice& right) {
        std::wstring leftName = ToLower(left.exeName);
        std::wstring rightName = ToLower(right.exeName);
        if (leftName == rightName) {
            return left.pid < right.pid;
        }
        return leftName < rightName;
    });
    return choices;
}

void FocusClockApp::AddWhitelistFromRunningProcess() {
    std::vector<ProcessPathChoice> choices = EnumerateRunningProcessPaths();
    if (choices.empty()) {
        whitelistMessage_ = L"添加失败：没有可读取路径的运行进程。";
        whitelistMessageIsError_ = true;
        RebuildLayout();
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }

    HMENU menu = CreatePopupMenu();
    if (!menu) {
        return;
    }

    constexpr size_t maxChoices = 160;
    size_t count = std::min(choices.size(), maxChoices);
    for (size_t i = 0; i < count; ++i) {
        std::wstring label = choices[i].exeName + L"  (" + std::to_wstring(choices[i].pid) + L")";
        AppendMenuW(menu, MF_STRING, kProcessPopupBaseId + static_cast<UINT>(i), label.c_str());
    }

    POINT pt{};
    GetCursorPos(&pt);
    UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, hwnd_, nullptr);
    DestroyMenu(menu);
    if (command >= kProcessPopupBaseId && command < kProcessPopupBaseId + count) {
        AddWhitelistPath(choices[command - kProcessPopupBaseId].path);
    }
}

void FocusClockApp::DeleteWhitelistEntry(size_t index) {
    if (index >= whitelistEntries_.size()) {
        return;
    }

    std::wstring deleted = whitelistEntries_[index].label;
    std::vector<std::wstring> specs;
    specs.reserve(whitelistEntries_.size() - 1);
    for (size_t i = 0; i < whitelistEntries_.size(); ++i) {
        if (i != index) {
            specs.push_back(whitelistEntries_[i].launchSpec);
        }
    }

    if (SaveWhitelistEntries(specs)) {
        LoadWhitelistIfNeeded(true);
        whitelistMessage_ = L"已删除：" + deleted;
        whitelistMessageIsError_ = false;
    } else {
        whitelistMessage_ = L"删除失败：无法写入 Whitelist.txt。";
        whitelistMessageIsError_ = true;
    }

    RebuildLayout();
    InvalidateRect(hwnd_, nullptr, FALSE);
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

} // namespace focus_clock
