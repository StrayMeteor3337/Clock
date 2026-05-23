#include "FocusClockApp.h"

extern FocusClockApp* gKeyboardHookApp;

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

void FocusClockApp::AddScheduledFocusTask() {
    int startMinute = ScheduleDraftStartMinute();
    int endMinute = ScheduleDraftEndMinute();
    if (!IsValidScheduleRange(startMinute, endMinute)) {
        int duration = ScheduleDurationMinutes(startMinute, endMinute);
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

int FocusClockApp::ScheduleDraftStartMinute() const {
    return scheduleDraftStartHour_ * 60 + scheduleDraftStartMinute_;
}

int FocusClockApp::ScheduleDraftEndMinute() const {
    return scheduleDraftEndHour_ * 60 + scheduleDraftEndMinute_;
}

bool FocusClockApp::HasScheduleConflict(int startMinute, int endMinute, int ignoreIndex) const {
    auto appendSegments = [](int start, int end, std::vector<std::pair<int, int>>& segments) {
        if (start <= end) {
            segments.emplace_back(start, end);
        } else {
            segments.emplace_back(start, 24 * 60);
            segments.emplace_back(0, end);
        }
    };

    std::vector<std::pair<int, int>> candidate;
    appendSegments(startMinute, endMinute, candidate);

    for (size_t i = 0; i < scheduledTasks_.size(); ++i) {
        if (static_cast<int>(i) == ignoreIndex) {
            continue;
        }

        const auto& task = scheduledTasks_[i];
        std::vector<std::pair<int, int>> existing;
        appendSegments(task.startMinute, task.endMinute, existing);

        for (const auto& left : candidate) {
            for (const auto& right : existing) {
                if (left.first < right.second && right.first < left.second) {
                    return true;
                }
            }
        }
    }

    return false;
}

bool FocusClockApp::IsValidScheduleRange(int startMinute, int endMinute) const {
    int duration = ScheduleDurationMinutes(startMinute, endMinute);
    return startMinute >= 0 && startMinute < 24 * 60 &&
        endMinute >= 0 && endMinute < 24 * 60 &&
        duration > 0 && duration <= maxFocusMinutes_;
}

int FocusClockApp::ScheduleDurationMinutes(int startMinute, int endMinute) const {
    if (startMinute < 0 || startMinute >= 24 * 60 || endMinute < 0 || endMinute >= 24 * 60 || startMinute == endMinute) {
        return 0;
    }

    int duration = endMinute - startMinute;
    if (duration < 0) {
        duration += 24 * 60;
    }

    return duration;
}

bool FocusClockApp::IsMinuteInScheduleRange(int minuteOfDay, int startMinute, int endMinute) const {
    if (endMinute > startMinute) {
        return minuteOfDay >= startMinute && minuteOfDay < endMinute;
    }

    if (endMinute < startMinute) {
        return minuteOfDay >= startMinute || minuteOfDay < endMinute;
    }

    return minuteOfDay == startMinute;
}

std::wstring FocusClockApp::FormatMinuteOfDay(int minute) const {
    int hour = minute / 60;
    int minuteOfHour = minute % 60;
    wchar_t buffer[16]{};
    swprintf_s(buffer, L"%02d:%02d", hour, minuteOfHour);
    return buffer;
}

int FocusClockApp::DateStamp(const SYSTEMTIME& time) {
    return time.wYear * 10000 + time.wMonth * 100 + time.wDay;
}

int FocusClockApp::PreviousDateStamp(const SYSTEMTIME& time) {
    FILETIME ft{};
    SystemTimeToFileTime(&time, &ft);

    ULARGE_INTEGER uli{};
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    uli.QuadPart -= 60ULL * 10000000ULL;

    ft.dwLowDateTime = uli.LowPart;
    ft.dwHighDateTime = uli.HighPart;

    SYSTEMTIME prev{};
    FileTimeToSystemTime(&ft, &prev);
    return DateStamp(prev);
}

int FocusClockApp::ScheduleRemainingSeconds(const ScheduledFocusTask& task, int secondOfDay) const {
    int endSecond = task.endMinute * 60;
    if (task.endMinute <= task.startMinute && secondOfDay >= task.startMinute * 60) {
        endSecond += 24 * 60 * 60;
    }
    return endSecond - secondOfDay;
}

int FocusClockApp::ScheduleStartDateStamp(const ScheduledFocusTask& task, const SYSTEMTIME& now) const {
    int minuteOfDay = now.wHour * 60 + now.wMinute;
    if (task.endMinute <= task.startMinute && minuteOfDay < task.endMinute) {
        return PreviousDateStamp(now);
    }
    return DateStamp(now);
}

void FocusClockApp::CheckScheduledFocusTasks(bool forceResumeActiveRange) {
    if (focusActive_ || scheduledTasks_.empty()) {
        return;
    }

    SYSTEMTIME now{};
    GetLocalTime(&now);
    int minuteOfDay = now.wHour * 60 + now.wMinute;
    int secondOfDay = minuteOfDay * 60 + now.wSecond;

    for (auto& task : scheduledTasks_) {
        int scheduleStartDate = ScheduleStartDateStamp(task, now);
        if ((!forceResumeActiveRange && task.lastStartedDate == scheduleStartDate) ||
            !IsMinuteInScheduleRange(minuteOfDay, task.startMinute, task.endMinute)) {
            continue;
        }

        int remainingSeconds = ScheduleRemainingSeconds(task, secondOfDay);
        long long totalSeconds = std::max(1LL, static_cast<long long>(ScheduleDurationMinutes(task.startMinute, task.endMinute)) * 60LL);
        int remainingMinutes = std::max(1, (remainingSeconds + 59) / 60);
        selectedMinutes_ = std::clamp(remainingMinutes, kMinFocusMinutes, maxFocusMinutes_);
        task.lastStartedDate = scheduleStartDate;
        SaveScheduledFocusTasks();
        scheduleMessage_ = L"计划 " + FormatMinuteOfDay(task.startMinute) + L" - " + FormatMinuteOfDay(task.endMinute) + L" 已自动开始。";
        scheduleMessageIsError_ = false;
        StartFocusForSeconds(remainingSeconds, true, totalSeconds);
        return;
    }
}