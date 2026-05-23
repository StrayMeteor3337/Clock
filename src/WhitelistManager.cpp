#include "FocusClockApp.h"

extern FocusClockApp* gKeyboardHookApp;

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
    std::wstring path = GetWhitelistPath();
    HANDLE file = CreateFileW(
        path.c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    std::string output;
    for (const auto& spec : launchSpecs) {
        std::string utf8;
        int wideLength = WideCharToMultiByte(CP_UTF8, 0, spec.c_str(), static_cast<int>(spec.size()), nullptr, 0, nullptr, nullptr);
        if (wideLength > 0) {
            utf8.resize(static_cast<size_t>(wideLength));
            WideCharToMultiByte(CP_UTF8, 0, spec.c_str(), static_cast<int>(spec.size()), utf8.data(), wideLength, nullptr, nullptr);
        }
        output += utf8;
        output += "\r\n";
    }

    DWORD written = 0;
    WriteFile(file, output.data(), static_cast<DWORD>(output.size()), &written, nullptr);
    CloseHandle(file);
    return true;
}

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
        if (end == std::wstring::npos) {
            end = text.size();
        }

        std::wstring line = text.substr(start, end - start);
        line = Trim(line);
        
        // ★★★ 关键修复：跳过注释行 ★★★
        if (!line.empty() && line[0] != L'#' && line[0] != L';') {
            // 处理引号包裹的路径
            if (line.size() >= 2 && line.front() == L'"' && line.back() == L'"') {
                line = Trim(line.substr(1, line.size() - 2));
            }
            
            if (!line.empty()) {
                WhitelistEntry entry{};
                entry.launchSpec = line;
                std::wstring resolved = ResolveLaunchPath(line);
                entry.normalized = ToLower(resolved);
                entry.exeName = BaseName(entry.normalized);
                if (!resolved.empty()) {
                    entry.icon = LoadIconForPath(resolved);
                    entry.iconPath = resolved;
                }
                entry.label = BaseName(line);
                entry.label = StripExtension(entry.label);
                entries.push_back(std::move(entry));
            }
        }

        if (end >= text.size()) {
            break;
        }
        start = end + 1;
        while (start < text.size() && (text[start] == L'\r' || text[start] == L'\n')) {
            ++start;
        }
    }

    DestroyWhitelistIconCache();
    whitelistEntries_ = std::move(entries);
    whitelistWriteTime_ = data.ftLastWriteTime;
    whitelistKnown_ = true;
    return true;
}

void FocusClockApp::AddWhitelistPath(const std::wstring& path) {
    if (!IsExecutableFileName(path)) {
        whitelistMessage_ = L"添加失败：不是 .exe 文件。";
        whitelistMessageIsError_ = true;
        RebuildLayout();
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }

    if (IsWhitelistPathDuplicate(path)) {
        whitelistMessage_ = L"添加失败：路径已存在于白名单。";
        whitelistMessageIsError_ = true;
        RebuildLayout();
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }

    std::vector<std::wstring> specs;
    specs.reserve(whitelistEntries_.size() + 1);
    for (const auto& entry : whitelistEntries_) {
        specs.push_back(entry.launchSpec);
    }
    specs.push_back(path);

    if (SaveWhitelistEntries(specs)) {
        LoadWhitelistIfNeeded(true);
        whitelistMessage_ = L"已添加：" + BaseName(path);
        whitelistMessageIsError_ = false;
    } else {
        whitelistMessage_ = L"添加失败：无法写入 Whitelist.txt。";
        whitelistMessageIsError_ = true;
    }

    RebuildLayout();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void FocusClockApp::AddWhitelistFolder(const std::wstring& folder) {
    std::vector<std::wstring> found = EnumerateExecutableFilesInDirectory(folder);
    if (found.empty()) {
        whitelistMessage_ = L"添加失败：该文件夹中没有 .exe 文件。";
        whitelistMessageIsError_ = true;
        RebuildLayout();
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }

    std::vector<std::wstring> specs;
    specs.reserve(whitelistEntries_.size() + found.size());
    for (const auto& entry : whitelistEntries_) {
        specs.push_back(entry.launchSpec);
    }

    size_t added = 0;
    for (const auto& filePath : found) {
        if (!IsWhitelistPathDuplicate(filePath)) {
            specs.push_back(filePath);
            ++added;
        }
    }

    if (added == 0) {
        whitelistMessage_ = L"添加失败：所有文件均已存在于白名单。";
        whitelistMessageIsError_ = true;
        RebuildLayout();
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }

    if (SaveWhitelistEntries(specs)) {
        LoadWhitelistIfNeeded(true);
        whitelistMessage_ = L"已添加 " + std::to_wstring(added) + L" 个程序。";
        whitelistMessageIsError_ = false;
    } else {
        whitelistMessage_ = L"添加失败：无法写入 Whitelist.txt。";
        whitelistMessageIsError_ = true;
    }

    RebuildLayout();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void FocusClockApp::AddWhitelistFromFileDialog() {
    std::wstring path(32768, L'\0');
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd_;
    ofn.lpstrFilter = L"可执行文件 (*.exe)\0*.exe\0所有文件 (*.*)\0*.*\0";
    ofn.lpstrFile = path.data();
    ofn.nMaxFile = static_cast<DWORD>(path.size());
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR | OFN_DONTADDTORECENT;

    if (GetOpenFileNameW(&ofn)) {
        path.resize(wcslen(path.c_str()));
        AddWhitelistPath(path);
    }
}

void FocusClockApp::AddWhitelistFromFolderDialog() {
    std::wstring path(32768, L'\0');
    BROWSEINFOW bi{};
    bi.hwndOwner = hwnd_;
    bi.lpszTitle = L"选择包含 .exe 文件的文件夹";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (pidl) {
        if (SHGetPathFromIDListW(pidl, path.data())) {
            path.resize(wcslen(path.c_str()));
            AddWhitelistFolder(path);
        }
        CoTaskMemFree(pidl);
    }
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

bool FocusClockApp::IsWhitelistPathDuplicate(const std::wstring& path) const {
    std::wstring normalizedNew = ToLower(path);
    std::wstring exeNameNew = BaseName(normalizedNew);

    for (const auto& entry : whitelistEntries_) {
        if (entry.normalized == normalizedNew || entry.exeName == exeNameNew) {
            return true;
        }
    }
    return false;
}

std::vector<std::wstring> FocusClockApp::EnumerateExecutableFilesInDirectory(const std::wstring& folder) const {
    std::vector<std::wstring> results;
    std::wstring searchPath = folder + L"\\*.exe";

    WIN32_FIND_DATAW findData{};
    HANDLE findHandle = FindFirstFileW(searchPath.c_str(), &findData);
    if (findHandle == INVALID_HANDLE_VALUE) {
        return results;
    }

    do {
        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            results.push_back(JoinPath(folder, findData.cFileName));
        }
    } while (FindNextFileW(findHandle, &findData));

    FindClose(findHandle);

    std::wstring subSearch = folder + L"\\*";
    findHandle = FindFirstFileW(subSearch.c_str(), &findData);
    if (findHandle != INVALID_HANDLE_VALUE) {
        do {
            if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                wcscmp(findData.cFileName, L".") != 0 &&
                wcscmp(findData.cFileName, L"..") != 0) {
                std::wstring subFolder = JoinPath(folder, findData.cFileName);
                std::vector<std::wstring> subResults = EnumerateExecutableFilesInDirectory(subFolder);
                results.insert(results.end(), subResults.begin(), subResults.end());
            }
        } while (FindNextFileW(findHandle, &findData));
        FindClose(findHandle);
    }

    return results;
}

std::vector<ProcessPathChoice> FocusClockApp::EnumerateRunningProcessPaths() const {
    std::vector<ProcessPathChoice> choices;

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return choices;
    }

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);

    if (Process32FirstW(snapshot, &pe)) {
        do {
            HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
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
            std::wstring lowerPath = ToLower(path);
            if (lowerPath.find(L"\\windows\\") != std::wstring::npos ||
                lowerPath.find(L"\\program files\\windowsapps\\") != std::wstring::npos) {
                continue;
            }

            ProcessPathChoice choice{};
            choice.pid = pe.th32ProcessID;
            choice.path = path;
            choice.exeName = BaseName(path);
            choices.push_back(std::move(choice));
        } while (Process32NextW(snapshot, &pe));
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

bool FocusClockApp::HasAnyWhitelistWindowVisible() {
    for (const auto& entry : whitelistEntries_) {
        HWND running = FindRunningWhitelistWindow(entry);
        if (running) {
            return true;
        }
    }
    return false;
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

    std::wstring normalizedPath = FocusClockApp::ToLower(path);
    std::wstring exeName = FocusClockApp::BaseName(normalizedPath);
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