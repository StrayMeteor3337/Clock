#include "FocusClockApp.h"

FocusClockApp* gKeyboardHookApp = nullptr;

void FocusClockApp::InstallFocusKeyboardHook() {
    if (keyboardHook_) {
        return;
    }

    HMODULE module = GetModuleHandleW(nullptr);
    gKeyboardHookApp = this;
    keyboardHook_ = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardHookProc, module, 0);
}

void FocusClockApp::RemoveFocusKeyboardHook() {
    if (keyboardHook_) {
        UnhookWindowsHookEx(keyboardHook_);
        keyboardHook_ = nullptr;
    }

    if (gKeyboardHookApp == this) {
        gKeyboardHookApp = nullptr;
    }
}

LRESULT CALLBACK FocusClockApp::KeyboardHookProc(int code, WPARAM wparam, LPARAM lparam) {
    if (code != HC_ACTION) {
        return CallNextHookEx(nullptr, code, wparam, lparam);
    }

    auto* info = reinterpret_cast<KBDLLHOOKSTRUCT*>(lparam);
    if (!info || !gKeyboardHookApp) {
        return CallNextHookEx(nullptr, code, wparam, lparam);
    }

    FocusClockApp* app = gKeyboardHookApp;

    if (wparam == WM_KEYDOWN || wparam == WM_SYSKEYDOWN) {
        DWORD vkCode = info->vkCode;

        // F6 - toggle panel
        if (vkCode == VK_F6 && (info->flags & LLKHF_ALTDOWN) == 0) {
            PostMessageW(app->hwnd_, WM_COMMAND, kPanelHotkeyId, 0);
            return 1;
        }

        // Win+Tab or Win+Ctrl+D - block
        if ((vkCode == VK_TAB && (GetAsyncKeyState(VK_LWIN) & 0x8000)) ||
            (vkCode == 'D' && (GetAsyncKeyState(VK_LWIN) & 0x8000) && (GetAsyncKeyState(VK_LCONTROL) & 0x8000))) {
            return 1;
        }

        // Alt+Tab - block
        if (vkCode == VK_TAB && (info->flags & LLKHF_ALTDOWN)) {
            return 1;
        }

        // Alt+Esc - block
        if (vkCode == VK_ESCAPE && (info->flags & LLKHF_ALTDOWN)) {
            return 1;
        }

        // Win+D, Win+M - block when focused
        if ((vkCode == 'D' || vkCode == 'M') && (GetAsyncKeyState(VK_LWIN) & 0x8000)) {
            return 1;
        }

        // Alt+F4 - block when focus active
        if (vkCode == VK_F4 && (info->flags & LLKHF_ALTDOWN) && app->focusActive_) {
            return 1;
        }

        // Esc - exit focus or close panel
        if (vkCode == VK_ESCAPE && (info->flags & LLKHF_ALTDOWN) == 0) {
            if (app->panelOpen_) {
                PostMessageW(app->hwnd_, WM_COMMAND, kPanelCloseButtonId, 0);
            } else if (app->focusActive_) {
                PostMessageW(app->hwnd_, WM_COMMAND, kExitButtonId, 0);
            }
        }
    }

    return CallNextHookEx(nullptr, code, wparam, lparam);
}

bool FocusClockApp::ShouldBlockFocusShortcut(DWORD vkCode) const {
    if (!focusActive_) {
        return false;
    }

    if (vkCode == VK_TAB && (GetAsyncKeyState(VK_LWIN) & 0x8000)) {
        return true;
    }
    if (vkCode == 'D' && (GetAsyncKeyState(VK_LWIN) & 0x8000) && (GetAsyncKeyState(VK_LCONTROL) & 0x8000)) {
        return true;
    }
    if (vkCode == VK_TAB && (GetAsyncKeyState(VK_LMENU) & 0x8000)) {
        return true;
    }
    if (vkCode == VK_ESCAPE && (GetAsyncKeyState(VK_LMENU) & 0x8000)) {
        return true;
    }
    if ((vkCode == 'D' || vkCode == 'M') && (GetAsyncKeyState(VK_LWIN) & 0x8000)) {
        return true;
    }
    if (vkCode == VK_F4 && (GetAsyncKeyState(VK_LMENU) & 0x8000)) {
        return true;
    }

    return false;
}