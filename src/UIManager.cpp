#include "FocusClockApp.h"

void FocusClockApp::EnsurePaintFonts(int width) {
    if (!uiFontFamily_ || !monoFontFamily_) {
        FontCollection* collection = nullptr;
        int count = 0;
        InstalledFontCollection installed;
        installed.GetFamilies(0, nullptr, &count);
        if (count > 0) {
            std::vector<FontFamily> families(static_cast<size_t>(count));
            installed.GetFamilies(count, families.data(), &count);

            auto findFamily = [&](const std::wstring& name) -> FontFamily* {
                for (auto& f : families) {
                    wchar_t nameBuffer[LF_FACESIZE]{};
                    f.GetFamilyName(nameBuffer);
                    if (name == nameBuffer) {
                        return new FontFamily(name.c_str());
                    }
                }
                return nullptr;
            };

            uiFontFamily_.reset(findFamily(L"Microsoft YaHei UI"));
            if (!uiFontFamily_) {
                uiFontFamily_.reset(findFamily(L"Microsoft YaHei"));
            }
            if (!uiFontFamily_) {
                uiFontFamily_.reset(findFamily(L"Segoe UI Variable Display"));
            }
            if (!uiFontFamily_) {
                uiFontFamily_.reset(findFamily(L"Segoe UI"));
            }
            if (!uiFontFamily_) {
                uiFontFamily_.reset(findFamily(L"Arial"));
            }

            monoFontFamily_.reset(findFamily(L"Cascadia Code"));
            if (!monoFontFamily_) {
                monoFontFamily_.reset(findFamily(L"Consolas"));
            }
            if (!monoFontFamily_) {
                monoFontFamily_.reset(findFamily(L"Courier New"));
            }
        }

        if (!uiFontFamily_) {
            uiFontFamily_ = std::make_unique<FontFamily>(L"Microsoft YaHei");
        }
        if (!monoFontFamily_) {
            monoFontFamily_ = std::make_unique<FontFamily>(L"Consolas");
        }
    }

    if (cachedPaintFontWidth_ == width) {
        return;
    }
    cachedPaintFontWidth_ = width;

    REAL digitalSize = static_cast<REAL>(std::max(56, std::min(132, width / 10)));
    digitalFont_ = std::make_unique<Font>(monoFontFamily_.get(), digitalSize, FontStyleRegular, UnitPixel);
    dateFont_ = std::make_unique<Font>(uiFontFamily_.get(), static_cast<REAL>(std::max(20, std::min(34, width / 48))), FontStyleRegular, UnitPixel);
    statusFont_ = std::make_unique<Font>(uiFontFamily_.get(), static_cast<REAL>(std::max(16, std::min(28, width / 64))), FontStyleRegular, UnitPixel);
    remainFont_ = std::make_unique<Font>(uiFontFamily_.get(), static_cast<REAL>(std::max(28, std::min(64, width / 24))), FontStyleRegular, UnitPixel);
    focusHintFont_ = std::make_unique<Font>(uiFontFamily_.get(), 14.0f, FontStyleRegular, UnitPixel);
    idleHintFont_ = std::make_unique<Font>(uiFontFamily_.get(), 16.0f, FontStyleRegular, UnitPixel);
}

void FocusClockApp::ReleaseRenderResources() {
    digitalFont_.reset();
    dateFont_.reset();
    statusFont_.reset();
    remainFont_.reset();
    focusHintFont_.reset();
    idleHintFont_.reset();
    cachedPaintFontWidth_ = 0;
}

void FocusClockApp::RebuildLayout() {
    RECT rc{};
    GetClientRect(hwnd_, &rc);
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;

    buttons_.clear();

    // 非专注模式 - 主界面按钮
    if (!focusActive_) {
        const int buttonHeight = 64;
        const int gap = 18;
        const int bottom = height - 86;
        const int durationWidth = 132;
        const std::array<int, 4> minutes{ 25, 45, 60, 90 };

        int totalWidth = static_cast<int>(minutes.size()) * durationWidth + 
                         static_cast<int>(minutes.size() - 1) * gap;
        int startX = (width - totalWidth) / 2;

        for (size_t i = 0; i < minutes.size(); ++i) {
            int left = startX + static_cast<int>(i) * (durationWidth + gap);
            AddPanelButton(minutes[i],
                RECT{ left, bottom - buttonHeight - 178, left + durationWidth, bottom - 178 },
                std::to_wstring(minutes[i]) + L" 分钟",
                selectedMinutes_ == minutes[i],
                minutes[i] <= maxFocusMinutes_,
                false,
                false);
        }

        const int customSmallWidth = 74;
        const int customDisplayWidth = 218;
        const int customTotalWidth = customSmallWidth * 4 + customDisplayWidth + gap * 4;
        int customLeft = (width - customTotalWidth) / 2;
        int customTop = bottom - buttonHeight - 88;

        AddStepperButtons(
            kCustomMinusFiveId, kCustomMinusOneId, kCustomDisplayId, 
            kCustomPlusOneId, kCustomPlusFiveId,
            customLeft, customTop, selectedMinutes_, L"分钟",
            selectedMinutes_ > kMinFocusMinutes,
            selectedMinutes_ < maxFocusMinutes_);

        AddPanelButton(kStartButtonId,
            RECT{ width / 2 - 150, bottom - buttonHeight, width / 2 + 150, bottom },
            L"开始专注", false, true, true, false);

        AddPanelButton(kExitButtonId,
            RECT{ width - 172, height - 70, width - 32, height - 26 },
            L"退出", false, true, false, true);
    } else {
        // 专注模式 - 按钮
        int buttonWidth = 112;
        int buttonHeight = 60;
        int gap = 12;
        int totalWidth = buttonWidth * 2 + gap;
        int leftSide = (width - totalWidth) / 2;
        int bottom = height - 36;
    }

    // ========== 面板布局 - 这是缺失的关键部分 ==========
    if (panelOpen_ && !focusActive_) {
        int panelWidth = std::min(760, std::max(520, width - 96));
        int panelHeight = std::min(560, std::max(420, height - 120));
        int left = (width - panelWidth) / 2;
        int top = (height - panelHeight) / 2;
        panelBounds_ = RECT{ left, top, left + panelWidth, top + panelHeight };

        // 关闭按钮
        AddPanelButton(kPanelCloseButtonId,
            RECT{ panelBounds_.right - 58, panelBounds_.top + 24, 
                  panelBounds_.right - 24, panelBounds_.top + 58 },
            L"×", false, true, false, true);

        // 标签页按钮
        int tabLeft = panelBounds_.left + 32;
        int tabTop = panelBounds_.top + 84;
        for (size_t i = 0; i < panelTabs_.size(); ++i) {
            AddPanelButton(kPanelTabButtonBaseId + panelTabs_[i].id,
                RECT{ tabLeft, tabTop + static_cast<int>(i) * 52, 
                      tabLeft + 148, tabTop + static_cast<int>(i) * 52 + 40 },
                panelTabs_[i].title,
                activePanelTab_ == panelTabs_[i].id,
                true, false, false);
        }

        int contentLeft = panelBounds_.left + 216;
        
        // 设置标签页内容
        if (activePanelTab_ == kPanelTabSettingsId) {
            // 最大专注时长控制
            AddStepperButtons(
                kSettingMaxMinusFiveId, kSettingMaxMinusOneId, kSettingMaxDisplayId,
                kSettingMaxPlusOneId, kSettingMaxPlusFiveId,
                contentLeft, panelBounds_.top + 206,
                maxFocusMinutes_, L" 分钟",
                maxFocusMinutes_ > kMinFocusMinutes,
                maxFocusMinutes_ < kAbsoluteMaxFocusMinutes);

            // 默认专注时长控制
            AddStepperButtons(
                kSettingDefaultMinusFiveId, kSettingDefaultMinusOneId, kSettingDefaultDisplayId,
                kSettingDefaultPlusOneId, kSettingDefaultPlusFiveId,
                contentLeft, panelBounds_.top + 350,
                defaultFocusMinutes_, L" 分钟",
                defaultFocusMinutes_ > kMinFocusMinutes,
                defaultFocusMinutes_ < maxFocusMinutes_);

            // 恢复默认按钮
            AddPanelButton(kPanelResetSettingsId,
                RECT{ contentLeft, panelBounds_.bottom - 82, 
                      contentLeft + 154, panelBounds_.bottom - 38 },
                L"恢复默认", false, true, false, true);
        }
        // 计划标签页内容
        else if (activePanelTab_ == kPanelTabScheduleId) {
            int scrollY = scheduleScrollOffset_;
            
            // 开始时间选择
            AddTimeStepperButtons(kScheduleStartHourMinusId, kScheduleStartHourDisplayId, 
                                   kScheduleStartHourPlusId, contentLeft, 
                                   panelBounds_.top + 214 - scrollY, 
                                   scheduleDraftStartHour_, 23, L" 时");
            AddTimeStepperButtons(kScheduleStartMinuteMinusId, kScheduleStartMinuteDisplayId, 
                                   kScheduleStartMinutePlusId, contentLeft + 236, 
                                   panelBounds_.top + 214 - scrollY, 
                                   scheduleDraftStartMinute_, 59, L" 分");
            
            // 结束时间选择
            AddTimeStepperButtons(kScheduleEndHourMinusId, kScheduleEndHourDisplayId, 
                                   kScheduleEndHourPlusId, contentLeft, 
                                   panelBounds_.top + 316 - scrollY, 
                                   scheduleDraftEndHour_, 23, L" 时");
            AddTimeStepperButtons(kScheduleEndMinuteMinusId, kScheduleEndMinuteDisplayId, 
                                   kScheduleEndMinutePlusId, contentLeft + 236, 
                                   panelBounds_.top + 316 - scrollY, 
                                   scheduleDraftEndMinute_, 59, L" 分");

            // 添加计划按钮
            AddPanelButton(kScheduleAddTaskId,
                RECT{ contentLeft, panelBounds_.top + 378 - scrollY, 
                      contentLeft + 154, panelBounds_.top + 422 - scrollY },
                L"添加计划", false, true, true, false);

            // 计划列表和删除按钮
            int listTop = panelBounds_.top + 506 - scrollY;
            int rowHeight = 34;
            RECT viewport = PanelContentViewport();
            for (int i = 0; i < static_cast<int>(scheduledTasks_.size()); ++i) {
                int rowTop = listTop + i * rowHeight;
                int rowBottom = rowTop + 26;
                if (rowBottom < viewport.top || rowTop > viewport.bottom) {
                    continue;
                }
                AddPanelButton(kScheduleDeleteButtonBaseId + i,
                    RECT{ panelBounds_.right - 104, rowTop, 
                          panelBounds_.right - 40, rowBottom },
                    L"删除", false, true, false, true);
            }
        }
        // 白名单标签页内容
        else if (activePanelTab_ == kPanelTabWhitelistId) {
            int scrollY = whitelistScrollOffset_;
            
            AddPanelButton(kWhitelistAddFileId,
                RECT{ contentLeft, panelBounds_.top + 150 - scrollY, 
                      contentLeft + 126, panelBounds_.top + 194 - scrollY },
                L"选择文件添加", false, true, true, false);
            AddPanelButton(kWhitelistAddFolderId,
                RECT{ contentLeft + 138, panelBounds_.top + 150 - scrollY, 
                      contentLeft + 264, panelBounds_.top + 194 - scrollY },
                L"选择文件夹", false, true, false, false);
            AddPanelButton(kWhitelistAddProcessId,
                RECT{ contentLeft, panelBounds_.top + 200 - scrollY, 
                      contentLeft + 182, panelBounds_.top + 244 - scrollY },
                L"从运行进程添加", false, true, false, false);

            // 白名单列表和删除按钮
            int listTop = panelBounds_.top + 336 - scrollY;
            int rowHeight = 36;
            RECT viewport = PanelContentViewport();
            for (int i = 0; i < static_cast<int>(whitelistEntries_.size()); ++i) {
                int top = listTop + i * rowHeight;
                int rowBottom = top + 26;
                if (rowBottom < viewport.top || top > viewport.bottom) {
                    continue;
                }
                AddPanelButton(kWhitelistDeleteButtonBaseId + i,
                    RECT{ panelBounds_.right - 104, top, 
                          panelBounds_.right - 40, rowBottom },
                    L"删除", false, true, false, true);
            }
        }
        // 防重启标签页内容
        else if (activePanelTab_ == kPanelTabRerunId) {
            AddPanelButton(kRerunCreateTaskId,
                RECT{ contentLeft, panelBounds_.top + 184, 
                      contentLeft + 250, panelBounds_.top + 232 },
                L"添加开机自启任务", false, true, true, false);
        }
    } else {
        panelBounds_ = RECT{};
    }

    // 白名单图标布局
    RebuildWhitelistLayoutOnly();
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

    EnsurePaintFonts(width);
    FontFamily& family = *uiFontFamily_;

    REAL digitalSize = static_cast<REAL>(std::max(56, std::min(132, width / 10)));

    RectF digitalRect(0, static_cast<REAL>(height * 0.52), static_cast<REAL>(width), digitalSize + 18);
    if (focusActive_) {
        REAL clockBottom = clockRect.Y + clockRect.Height;
        REAL progressGap = digitalRect.Y - clockBottom;
        if (progressGap > 18.0f) {
            REAL progressWidth = static_cast<REAL>(std::max(160, width - 96));
            progressWidth = std::min(progressWidth, 560.0f);
            REAL progressHeight = std::min(12.0f, std::max(8.0f, progressGap - 12.0f));
            REAL progressY = clockBottom + (progressGap - progressHeight) / 2.0f;
            RectF progressRect(
                (static_cast<REAL>(width) - progressWidth) / 2.0f,
                progressY,
                progressWidth,
                progressHeight);
            DrawFocusProgressBar(g, progressRect, theme);
        }
    }
    DrawTextBlock(g, FormatTime(now), digitalRect, *digitalFont_, theme.primaryText, StringAlignmentCenter, StringAlignmentCenter);

    RectF dateRect(0, digitalRect.Y + digitalRect.Height + 4, static_cast<REAL>(width), 48);
    DrawTextBlock(g, FormatDate(now), dateRect, *dateFont_, theme.secondaryText, StringAlignmentCenter, StringAlignmentCenter);

    if (focusActive_) {
        RectF remainRect(0, dateRect.Y + 56, static_cast<REAL>(width), 92);
        DrawTextBlock(g, FormatRemaining(), remainRect, *remainFont_, theme.accent, StringAlignmentCenter, StringAlignmentCenter);

        RectF statusRect(0, remainRect.Y + remainRect.Height + 8, static_cast<REAL>(width), 52);
        DrawTextBlock(g, L"专注进行中", statusRect, *statusFont_, theme.secondaryText, StringAlignmentCenter, StringAlignmentCenter);

        RectF hintRect(0, static_cast<REAL>(height - 70), static_cast<REAL>(width), 28);
        DrawTextBlock(g, L"专注结束前会保持全屏和置顶", hintRect, *focusHintFont_, theme.mutedText, StringAlignmentCenter, StringAlignmentCenter);
    } else {
        RectF hintRect(0, dateRect.Y + 58, static_cast<REAL>(width), 34);
        DrawTextBlock(g, L"选择时长后开始", hintRect, *idleHintFont_, theme.mutedText, StringAlignmentCenter, StringAlignmentCenter);
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
                if (activePanelTab_ == kPanelTabWhitelistId &&
                    button.id >= kWhitelistAddFileId &&
                    button.id < kWhitelistDeleteButtonLimit &&
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

void FocusClockApp::DrawFocusProgressBar(Graphics& g, const RectF& bounds, const Theme& theme) const {
    if (bounds.Width <= 0.0f || bounds.Height <= 0.0f) {
        return;
    }

    auto addRoundedPath = [](GraphicsPath& path, const RectF& rect) {
        REAL radius = std::min(rect.Height / 2.0f, rect.Width / 2.0f);
        if (radius <= 0.0f) {
            return;
        }

        REAL diameter = radius * 2.0f;
        path.AddArc(rect.X, rect.Y, diameter, diameter, 180, 90);
        path.AddArc(rect.X + rect.Width - diameter, rect.Y, diameter, diameter, 270, 90);
        path.AddArc(rect.X + rect.Width - diameter, rect.Y + rect.Height - diameter, diameter, diameter, 0, 90);
        path.AddArc(rect.X, rect.Y + rect.Height - diameter, diameter, diameter, 90, 90);
        path.CloseFigure();
    };

    long long total = std::max(1LL, focusTotalSeconds_);
    long long remaining = std::clamp(remainingSeconds_, 0LL, total);
    REAL progress = static_cast<REAL>(total - remaining) / static_cast<REAL>(total);

    GraphicsPath trackPath;
    addRoundedPath(trackPath, bounds);
    SolidBrush trackBrush(darkMode_ ? Color(120, 54, 64, 78) : Color(150, 214, 220, 228));
    Pen borderPen(darkMode_ ? Color(120, 89, 180, 255) : Color(95, 0, 113, 188), 1.0f);
    g.FillPath(&trackBrush, &trackPath);

    REAL fillWidth = bounds.Width * progress;
    if (fillWidth > 0.5f) {
        RectF fillRect(bounds.X, bounds.Y, fillWidth, bounds.Height);
        GraphicsPath fillPath;
        addRoundedPath(fillPath, fillRect);
        SolidBrush fillBrush(theme.accent);
        g.FillPath(&fillBrush, &fillPath);
    }

    g.DrawPath(&borderPen, &trackPath);
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

    FontFamily& family = *uiFontFamily_;
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
    } else if (activePanelTab_ == kPanelTabWhitelistId) {
        GraphicsState state = g.Save();
        g.SetClip(contentRect);
        DrawWhitelistTab(g, contentRect, theme, family);
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
        int duration = ScheduleDurationMinutes(ScheduleDraftStartMinute(), ScheduleDraftEndMinute());
        if (duration > 0 && duration <= maxFocusMinutes_) {
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

void FocusClockApp::DrawWhitelistTab(Graphics& g, const RectF& contentRect, const Theme& theme, FontFamily& family) {
    Font headingFont(&family, 24, FontStyleRegular, UnitPixel);
    Font bodyFont(&family, 16, FontStyleRegular, UnitPixel);
    Font helpFont(&family, 15, FontStyleRegular, UnitPixel);
    Font rowFont(&family, 15, FontStyleRegular, UnitPixel);
    REAL y = -static_cast<REAL>(whitelistScrollOffset_);

    DrawTextBlock(g, L"白名单管理", RectF(contentRect.X, contentRect.Y + y, contentRect.Width, 34.0f), headingFont, theme.primaryText, StringAlignmentNear, StringAlignmentCenter);

    Color messageColor = whitelistMessageIsError_ ? theme.danger : theme.secondaryText;
    std::wstring message = whitelistMessage_.empty()
        ? (L"当前 " + std::to_wstring(whitelistEntries_.size()) + L" 个白名单程序。")
        : whitelistMessage_;
    DrawTextBlock(g, message, RectF(contentRect.X, contentRect.Y + 158.0f + y, contentRect.Width, 26.0f), helpFont, messageColor, StringAlignmentNear, StringAlignmentCenter);

    DrawTextBlock(g, L"已添加程序", RectF(contentRect.X, contentRect.Y + 198.0f + y, contentRect.Width, 28.0f), bodyFont, theme.primaryText, StringAlignmentNear, StringAlignmentCenter);
    if (whitelistEntries_.empty()) {
        DrawTextBlock(g, L"暂无白名单程序。", RectF(contentRect.X, contentRect.Y + 238.0f + y, contentRect.Width, 28.0f), helpFont, theme.mutedText, StringAlignmentNear, StringAlignmentCenter);
        return;
    }

    int listTop = static_cast<int>(contentRect.Y + 244.0f + y);
    int rowHeight = 36;
    RECT viewport = PanelContentViewport();
    int count = static_cast<int>(whitelistEntries_.size());
    for (int i = 0; i < count; ++i) {
        int top = listTop + i * rowHeight;
        if (top + 26 < viewport.top || top > viewport.bottom) {
            continue;
        }
        const auto& entry = whitelistEntries_[i];
        std::wstring line = entry.label;
        if (!entry.exeName.empty() && entry.exeName != ToLower(entry.label)) {
            line += L"  -  " + entry.exeName;
        }
        DrawTextBlock(g, line, RectF(contentRect.X, static_cast<REAL>(top), contentRect.Width - 100.0f, 26.0f), rowFont, theme.secondaryText, StringAlignmentNear, StringAlignmentCenter);
    }

    if (static_cast<int>(whitelistEntries_.size()) > count) {
        DrawTextBlock(
            g,
            L"还有 " + std::to_wstring(static_cast<int>(whitelistEntries_.size()) - count) + L" 项未显示，可直接编辑 Whitelist.txt 管理更多项目。",
            RectF(contentRect.X, contentRect.Y + contentRect.Height - 26.0f + y, contentRect.Width, 24.0f),
            helpFont,
            theme.mutedText,
            StringAlignmentNear,
            StringAlignmentCenter);
    }

    int maxScroll = WhitelistContentMaxScroll();
    if (maxScroll > 0) {
        REAL trackX = contentRect.X + contentRect.Width - 8.0f;
        REAL trackY = contentRect.Y;
        REAL trackHeight = contentRect.Height;
        SolidBrush trackBrush(darkMode_ ? Color(90, 68, 78, 94) : Color(90, 184, 194, 207));
        SolidBrush thumbBrush(theme.accent);
        g.FillRectangle(&trackBrush, trackX, trackY, 4.0f, trackHeight);

        REAL thumbHeight = std::max(40.0f, trackHeight * (trackHeight / (trackHeight + maxScroll)));
        REAL thumbY = trackY + (trackHeight - thumbHeight) * (static_cast<REAL>(whitelistScrollOffset_) / maxScroll);
        g.FillRectangle(&thumbBrush, trackX, thumbY, 4.0f, thumbHeight);
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

    FontFamily& family = *uiFontFamily_;
    int buttonHeight = static_cast<int>(button.rect.bottom - button.rect.top);
    REAL fontSize = static_cast<REAL>(std::max(18, std::min(24, buttonHeight / 3)));
    if (button.id >= kScheduleDeleteButtonBaseId && button.id < kScheduleDeleteButtonLimit) {
        fontSize = 14.0f;
    } else if (button.id >= kWhitelistDeleteButtonBaseId && button.id < kWhitelistDeleteButtonLimit) {
        fontSize = 12.5f;
    }
    Font font(&family, fontSize, FontStyleRegular, UnitPixel);
    DrawTextBlock(g, button.label, r, font, text, StringAlignmentCenter, StringAlignmentCenter);
}

void FocusClockApp::DrawButtonIcon(Graphics& g, const UiButton& button, const RectF& bounds) {
    if (!button.icon) {
        return;
    }

    int iconSize = static_cast<int>(std::min(bounds.Width, bounds.Height) * 0.62f);
    iconSize = std::max(28, std::min(iconSize, 48));
    int x = static_cast<int>(bounds.X + (bounds.Width - iconSize) / 2.0f);
    int y = static_cast<int>(bounds.Y + (bounds.Height - iconSize) / 2.0f);

    g.Flush();
    HDC hdc = g.GetHDC();
    DrawIconEx(hdc, x, y, button.icon, iconSize, iconSize, 0, nullptr, DI_NORMAL);
    g.ReleaseHDC(hdc);
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
            if (activePanelTab_ == kPanelTabWhitelistId &&
                button.id >= kWhitelistAddFileId &&
                button.id < kWhitelistDeleteButtonLimit &&
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
    const int smallWidth = 58;      // 注意：原始代码是 58
    const int displayWidth = 170;    // 注意：原始代码是 170
    const int gap = 10;              // 注意：原始代码是 10

    AddPanelButton(minusFiveId, RECT{ left, top, left + smallWidth, top + buttonHeight }, L"-5", false, canDecrease);
    left += smallWidth + gap;
    AddPanelButton(minusOneId, RECT{ left, top, left + smallWidth, top + buttonHeight }, L"-1", false, canDecrease);
    left += smallWidth + gap;
    
    // 显示当前时长的按钮
    AddPanelButton(displayId, RECT{ left, top, left + displayWidth, top + buttonHeight }, 
                   std::to_wstring(value) + L" " + suffix, true, false);
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

void FocusClockApp::TogglePanel() {
    if (focusActive_) {
        return;
    }

    panelOpen_ = !panelOpen_;
    if (panelOpen_) {
        LoadWhitelistIfNeeded();
        LoadScheduledFocusTasks();
    }
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

void FocusClockApp::HandlePanelCommand(int id) {
    if (id == kPanelCloseButtonId) {
        ClosePanel();
    } else if (id == kPanelResetSettingsId) {
        ResetAppSettings();
        RebuildLayout();
        InvalidateRect(hwnd_, nullptr, FALSE);
    } else if (id >= kPanelTabButtonBaseId && id < kSettingMaxMinusFiveId) {
        activePanelTab_ = id - kPanelTabButtonBaseId;
        if (activePanelTab_ == kPanelTabRerunId) {
            CheckRerunStartupTaskStatus();
        }
        // 重置滚动偏移
        scheduleScrollOffset_ = 0;
        whitelistScrollOffset_ = 0;
        RebuildLayout();
        InvalidateRect(hwnd_, nullptr, FALSE);
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
    } else if (id == kWhitelistAddFileId) {
        AddWhitelistFromFileDialog();
    } else if (id == kWhitelistAddFolderId) {
        AddWhitelistFromFolderDialog();
    } else if (id == kWhitelistAddProcessId) {
        AddWhitelistFromRunningProcess();
    } else if (id >= kWhitelistDeleteButtonBaseId && id < kWhitelistDeleteButtonLimit) {
        DeleteWhitelistEntry(static_cast<size_t>(id - kWhitelistDeleteButtonBaseId));
    }
}

void FocusClockApp::ScrollScheduleTab(int delta) {
    int maxScroll = ScheduleContentMaxScroll();
    scheduleScrollOffset_ = std::clamp(scheduleScrollOffset_ - delta, 0, maxScroll);
    RebuildLayout();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

int FocusClockApp::ScheduleContentMaxScroll() const {
    int contentHeight = static_cast<int>(414 + scheduledTasks_.size() * 34);
    RECT viewport = PanelContentViewport();
    int viewHeight = viewport.bottom - viewport.top;
    return std::max(0, contentHeight - viewHeight);
}

void FocusClockApp::ScrollWhitelistTab(int delta) {
    int maxScroll = WhitelistContentMaxScroll();
    whitelistScrollOffset_ = std::clamp(whitelistScrollOffset_ - delta, 0, maxScroll);
    RebuildLayout();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

int FocusClockApp::WhitelistContentMaxScroll() const {
    int contentHeight = static_cast<int>(244 + whitelistEntries_.size() * 36);
    RECT viewport = PanelContentViewport();
    int viewHeight = viewport.bottom - viewport.top;
    return std::max(0, contentHeight - viewHeight);
}

RECT FocusClockApp::PanelContentViewport() const {
    if (!panelOpen_) {
        return RECT{};
    }

    return RECT{
        panelBounds_.left + 216,
        panelBounds_.top + 92,
        panelBounds_.right - 32,
        panelBounds_.bottom - 34
    };
}

void FocusClockApp::MoveWhitelistLayout(int dx, int dy) {
    whitelistLeft_ += dx;
    whitelistTop_ += dy;
    RECT rc{};
    GetClientRect(hwnd_, &rc);
    ClampWhitelistLayout(rc.right - rc.left, rc.bottom - rc.top);
    MarkWhitelistLayoutChanged(false);
}

void FocusClockApp::ResizeWhitelistLayout(int delta) {
    whitelistIconSize_ = std::clamp(whitelistIconSize_ + delta, kMinWhitelistIconSize, kMaxWhitelistIconSize);
    MarkWhitelistLayoutChanged(true);
}

void FocusClockApp::RebuildWhitelistLayoutOnly() {
    RECT rc{};
    GetClientRect(hwnd_, &rc);
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;

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
    const int appCapacity = std::max(0, std::min(kWhitelistButtonLimit - kWhitelistButtonBaseId, appRows * appColumns));
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
        app.icon = whitelistEntries_[i].icon;
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
        whitelistLayoutDirty_ = true;
        whitelistLayoutNeedsRebuild_ = needsRebuild;
        whitelistLayoutLastChangeTick_ = GetTickCount();
    }
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void FocusClockApp::ProcessPendingWhitelistLayout(bool forceSave) {
    if (whitelistLayoutDirty_) {
        DWORD elapsed = GetTickCount() - whitelistLayoutLastChangeTick_;
        if (forceSave || elapsed >= kWhitelistLayoutSaveDelayMs) {
            whitelistLayoutDirty_ = false;
            if (whitelistLayoutNeedsRebuild_ || forceSave) {
                RebuildLayout();
            } else {
                InvalidateRect(hwnd_, nullptr, FALSE);
            }
            SaveWhitelistLayoutSettings();
        }
    }
}

void FocusClockApp::InvalidateWhitelistArea(const RECT& previousBounds) {
    RECT oldRect = previousBounds;
    RECT newRect = whitelistBounds_;
    RECT invalid{};
    UnionRect(&invalid, &oldRect, &newRect);
    InvalidateRect(hwnd_, &invalid, FALSE);
}

void FocusClockApp::ClampWhitelistLayout(int clientWidth, int clientHeight) {
    whitelistIconSize_ = std::clamp(whitelistIconSize_, kMinWhitelistIconSize, kMaxWhitelistIconSize);

    const int margin = 16;
    const int appGap = 14;
    const int appColumns = 3;
    int minLeft = margin;
    int maxLeft = clientWidth - (appColumns * whitelistIconSize_ + (appColumns - 1) * appGap) - margin;
    whitelistLeft_ = std::clamp(whitelistLeft_, minLeft, std::max(minLeft, maxLeft));

    const int titleHeight = 40;
    const int appBottomLimit = clientHeight - 112;
    const int appRows = std::max(0, (appBottomLimit - whitelistTop_ - titleHeight) / (whitelistIconSize_ + appGap));
    int maxTop = appBottomLimit - titleHeight - appRows * (whitelistIconSize_ + appGap);
    whitelistTop_ = std::clamp(whitelistTop_, margin, std::max(margin, maxTop));
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

void FocusClockApp::OpenWhitelistEntry(size_t index) {
    if (index >= whitelistEntries_.size()) {
        return;
    }

    const auto& entry = whitelistEntries_[index];
    if (entry.launchSpec.empty()) {
        return;
    }

    std::wstring resolvedPath = ResolveLaunchPath(entry.launchSpec);
    ShellExecuteW(nullptr, L"open", resolvedPath.c_str(), nullptr, nullptr, SW_SHOW);
}

void FocusClockApp::UpdateRemaining() {
    if (!focusActive_) {
        return;
    }

    auto now = std::chrono::steady_clock::now();
    remainingSeconds_ = std::max(0LL, std::chrono::duration_cast<std::chrono::seconds>(focusEnd_ - now).count());
}

std::wstring FocusClockApp::FormatTime(const SYSTEMTIME& now) const {
    wchar_t buffer[16]{};
    swprintf_s(buffer, L"%02d:%02d", now.wHour, now.wMinute);
    return buffer;
}

std::wstring FocusClockApp::FormatDate(const SYSTEMTIME& now) const {
    wchar_t buffer[64]{};
    GetDateFormatW(LOCALE_USER_DEFAULT, DATE_LONGDATE, &now, nullptr, buffer, static_cast<int>(std::size(buffer)));
    return buffer;
}

std::wstring FocusClockApp::FormatRemaining() const {
    long long totalSec = remainingSeconds_;
    long long hours = totalSec / 3600;
    long long minutes = (totalSec % 3600) / 60;
    long long seconds = totalSec % 60;

    wchar_t buffer[32]{};
    if (hours > 0) {
        swprintf_s(buffer, L"%02lld:%02lld:%02lld", hours, minutes, seconds);
    } else {
        swprintf_s(buffer, L"%02lld:%02lld", minutes, seconds);
    }
    return buffer;
}