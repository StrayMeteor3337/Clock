#include "FocusClockApp.h"

namespace focus_clock {

namespace {

FocusClockApp* gKeyboardHookApp = nullptr;

} // namespace

LRESULT CALLBACK FocusClockApp::KeyboardHookProc(int code, WPARAM wparam, LPARAM lparam) {
    if (code == HC_ACTION && gKeyboardHookApp) {
        bool keyDown = wparam == WM_KEYDOWN || wparam == WM_SYSKEYDOWN;
        if (keyDown) {
            auto* info = reinterpret_cast<KBDLLHOOKSTRUCT*>(lparam);
            if (info && gKeyboardHookApp->ShouldBlockFocusShortcut(info->vkCode)) {
                MessageBeep(MB_ICONINFORMATION);
                return 1;
            }
        }
    }

    return CallNextHookEx(nullptr, code, wparam, lparam);
}

void FocusClockApp::InstallFocusKeyboardHook() {
    if (keyboardHook_) {
        return;
    }

    gKeyboardHookApp = this;
    keyboardHook_ = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardHookProc, instance_, 0);
    if (!keyboardHook_) {
        gKeyboardHookApp = nullptr;
    }
}

void FocusClockApp::RemoveFocusKeyboardHook() {
    if (!keyboardHook_) {
        if (gKeyboardHookApp == this) {
            gKeyboardHookApp = nullptr;
        }
        return;
    }

    UnhookWindowsHookEx(keyboardHook_);
    keyboardHook_ = nullptr;
    if (gKeyboardHookApp == this) {
        gKeyboardHookApp = nullptr;
    }
}

bool FocusClockApp::ShouldBlockFocusShortcut(DWORD vkCode) const {
    if (!focusActive_) {
        return false;
    }

    bool winDown = (GetAsyncKeyState(VK_LWIN) & 0x8000) != 0 || (GetAsyncKeyState(VK_RWIN) & 0x8000) != 0;
    bool ctrlDown = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    if (!winDown) {
        return false;
    }

    if (vkCode == VK_TAB) {
        return true;
    }

    return ctrlDown && (vkCode == 'D' || vkCode == VK_LEFT || vkCode == VK_RIGHT || vkCode == VK_F4);
}

} // namespace focus_clock
