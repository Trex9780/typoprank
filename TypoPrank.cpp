#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <initializer_list>
#include <random>

namespace {
constexpr UINT trayMessage = WM_APP + 1;
constexpr UINT pauseCommand = 1;
constexpr UINT exitCommand = 2;
constexpr BYTE released = 254;
constexpr BYTE passed = 255;
// shortcut: QWERTY neighbors, detect the active layout if other layouts matter.
constexpr const char* rows[] = {"QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"};
HWND windowHandle;
HHOOK hook;
NOTIFYICONDATAW icon{};
UINT taskbarCreated;
bool trayAdded;
BYTE held[256]{};
bool paused;
int previousLetter;
int variedLetters;
std::mt19937 randomEngine{std::random_device{}()};
int lettersUntilTypo = std::uniform_int_distribution<int>(1, 5)(randomEngine);

bool down(int key) { return (GetAsyncKeyState(key) & 0x8000) != 0; }
bool modifierDown() { return down(VK_CONTROL) || down(VK_MENU) || down(VK_LWIN) || down(VK_RWIN); }
bool inject(BYTE key, bool up) {
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = key;
    input.ki.dwFlags = up ? KEYEVENTF_KEYUP : 0;
    return SendInput(1, &input, sizeof(input)) == 1;
}
BYTE replacementFor(int key, bool right) {
    for (const char* row : rows) {
        const char* found = std::strchr(row, key);
        if (!found) continue;
        int index = static_cast<int>(found - row);
        int last = static_cast<int>(std::strlen(row)) - 1;
        return static_cast<BYTE>(row[index == 0 ? 1 : index == last ? last - 1 : index + (right ? 1 : -1)]);
    }
    return 0;
}
void releaseKeys(bool stopping) {
    for (int key = 0; key < 256; ++key) {
        if (held[key] > 0 && held[key] < released) {
            inject(held[key], true);
            held[key] = stopping ? 0 : released;
        }
    }
}
void updateIcon() {
    wcscpy_s(icon.szTip, paused ? L"Typo Prank - paused" : L"Typo Prank - occasional letter swaps");
    Shell_NotifyIconW(NIM_MODIFY, &icon);
}
void togglePause() {
    paused = !paused;
    previousLetter = variedLetters = 0;
    if (paused) releaseKeys(false);
    updateIcon();
}
LRESULT CALLBACK keyboardHook(int code, WPARAM message, LPARAM data) {
    if (code < 0) return CallNextHookEx(hook, code, message, data);
    const auto* event = reinterpret_cast<const KBDLLHOOKSTRUCT*>(data);
    if (event->flags & LLKHF_INJECTED) return CallNextHookEx(hook, code, message, data);
    int key = static_cast<int>(event->vkCode);
    bool keyDown = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
    bool keyUp = message == WM_KEYUP || message == WM_SYSKEYUP;
    if (key < 0 || key >= 256 || (!keyDown && !keyUp)) return CallNextHookEx(hook, code, message, data);
    if (keyUp) {
        BYTE replacement = held[key];
        held[key] = 0;
        if (replacement > 0 && replacement < released) { inject(replacement, true); return 1; }
        if (replacement == released) return 1;
    } else {
        BYTE replacement = held[key];
        if (replacement == passed) return CallNextHookEx(hook, code, message, data);
        if (replacement == released) return 1;
        if (replacement != 0) { if (!modifierDown()) inject(replacement, false); return 1; }
        if (key < 'A' || key > 'Z' || modifierDown()) {
            if (key != VK_SHIFT && key != VK_LSHIFT && key != VK_RSHIFT) previousLetter = variedLetters = 0;
        } else {
            variedLetters = key == previousLetter ? 1 : std::min(3, variedLetters + 1);
            previousLetter = key;
            if (!paused && variedLetters >= 3 && --lettersUntilTypo == 0) {
                lettersUntilTypo = std::uniform_int_distribution<int>(1, 5)(randomEngine);
                replacement = replacementFor(key, std::uniform_int_distribution<int>(0, 1)(randomEngine) == 0);
                if (!down(replacement) && inject(replacement, false)) { held[key] = replacement; return 1; }
            }
        }
        held[key] = passed;
    }
    return CallNextHookEx(hook, code, message, data);
}
LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (taskbarCreated && message == taskbarCreated) { trayAdded = false; SetTimer(window, 2, 1000, nullptr); return 0; }
    if (message == WM_TIMER && wParam == 2) {
        if (Shell_NotifyIconW(NIM_ADD, &icon)) {
            trayAdded = true; KillTimer(window, 2);
            if (!hook) {
                hook = SetWindowsHookExW(WH_KEYBOARD_LL, keyboardHook, GetModuleHandleW(nullptr), 0);
                if (!hook) { MessageBoxW(nullptr, L"Could not start the keyboard prank.", L"Typo Prank", MB_OK | MB_ICONERROR); DestroyWindow(window); }
            }
        }
        return 0;
    }
    if (message == trayMessage && (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU)) {
        HMENU menu = CreatePopupMenu();
        AppendMenuW(menu, MF_STRING, pauseCommand, paused ? L"Resume" : L"Pause");
        AppendMenuW(menu, MF_STRING, exitCommand, L"Exit");
        POINT point{}; GetCursorPos(&point); SetForegroundWindow(window);
        TrackPopupMenu(menu, TPM_RIGHTBUTTON, point.x, point.y, 0, window, nullptr);
        DestroyMenu(menu);
        return 0;
    }
    if (message == WM_COMMAND) {
        if (LOWORD(wParam) == pauseCommand) togglePause();
        if (LOWORD(wParam) == exitCommand) DestroyWindow(window);
        return 0;
    }
    if (message == WM_DESTROY) { KillTimer(window, 2); PostQuitMessage(0); return 0; }
    return DefWindowProcW(window, message, wParam, lParam);
}
bool selfTest() {
    for (const char* row : rows) for (const char* key = row; *key; ++key)
        for (bool right : {false, true}) {
            BYTE result = replacementFor(*key, right);
            const char* found = std::strchr(row, result);
            if (!found || std::abs(static_cast<int>(found - key)) != 1) return false;
        }
    int previous = 0, streak = 0;
    for (int i = 0; i < 20; ++i) { streak = 'R' == previous ? 1 : std::min(3, streak + 1); previous = 'R'; }
    return streak == 1 && replacementFor('1', true) == 0 && sizeof(INPUT) == 40;
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR commandLine, int) {
    if (std::wcsstr(commandLine, L"--selftest")) return selfTest() ? 0 : 1;
    HANDLE mutex = CreateMutexW(nullptr, TRUE, L"Local\\TypoPrank-8B2D7795");
    if (!mutex || GetLastError() == ERROR_ALREADY_EXISTS) return 0;
    WNDCLASSW cls{}; cls.lpfnWndProc = windowProc; cls.hInstance = instance; cls.lpszClassName = L"TypoPrankWindow";
    if (!RegisterClassW(&cls)) return 1;
    taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    windowHandle = CreateWindowExW(0, cls.lpszClassName, L"", 0, 0, 0, 0, 0, nullptr, nullptr, instance, nullptr);
    if (!windowHandle) return 1;
    icon.cbSize = sizeof(icon); icon.hWnd = windowHandle; icon.uID = 1;
    icon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP; icon.uCallbackMessage = trayMessage;
    icon.hIcon = LoadIconW(nullptr, IDI_INFORMATION);
    wcscpy_s(icon.szTip, L"Typo Prank - occasional letter swaps");
    SetTimer(windowHandle, 2, 1000, nullptr);
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) { TranslateMessage(&message); DispatchMessageW(&message); }
    releaseKeys(true);
    if (hook) UnhookWindowsHookEx(hook);
    if (trayAdded) Shell_NotifyIconW(NIM_DELETE, &icon);
    ReleaseMutex(mutex); CloseHandle(mutex);
    return 0;
}
