#include "sharekeys/win32_keyboard.hpp"
#include <stdexcept>
#include <system_error>

namespace sharekeys {
thread_local KeyboardHook* KeyboardHook::current_ = nullptr;
KeyboardHook::KeyboardHook(Callback callback, bool observeInjected)
    : callback_(std::move(callback)), observeInjected_(observeInjected) {
    std::promise<DWORD> ready;
    auto result = ready.get_future();
    thread_ = std::thread([this, ready = std::move(ready)]() mutable { run(std::move(ready)); });
    const auto error = result.get();
    if (error != ERROR_SUCCESS) {
        thread_.join();
        throw std::system_error(static_cast<int>(error), std::system_category(), "keyboard hook");
    }
}
KeyboardHook::~KeyboardHook() {
    if (thread_.joinable()) {
        PostThreadMessageW(threadId_.load(), WM_QUIT, 0, 0);
        thread_.join();
    }
}
bool KeyboardHook::physicallyHeld(KeyId key) const {
    return key.vk < physical_.size() && physical_[key.vk].load(std::memory_order_relaxed);
}
void KeyboardHook::run(std::promise<DWORD> ready) {
    current_ = this;
    MSG message{};
    PeekMessageW(&message, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
    threadId_.store(GetCurrentThreadId());
    const auto hook = SetWindowsHookExW(WH_KEYBOARD_LL, procedure, GetModuleHandleW(nullptr), 0);
    if (!hook) { ready.set_value(GetLastError()); current_ = nullptr; return; }
    ready.set_value(ERROR_SUCCESS);
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    UnhookWindowsHookEx(hook);
    current_ = nullptr;
}
LRESULT CALLBACK KeyboardHook::procedure(int code, WPARAM message, LPARAM data) {
    auto* self = current_;
    if (code == HC_ACTION && self) {
        const auto& raw = *reinterpret_cast<const KBDLLHOOKSTRUCT*>(data);
        const bool injected = (raw.flags & (LLKHF_INJECTED | LLKHF_LOWER_IL_INJECTED)) != 0 || raw.dwExtraInfo == injection_tag;
        const bool down = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
        if (!injected && raw.vkCode < self->physical_.size()) self->physical_[raw.vkCode].store(down, std::memory_order_relaxed);
        if (!injected || self->observeInjected_) {
            const KeyboardEvent event{{static_cast<std::uint16_t>(raw.scanCode), static_cast<std::uint16_t>(raw.vkCode), (raw.flags & LLKHF_EXTENDED) != 0}, down, injected, ++self->sequence_};
            try { if (self->callback_(event)) return 1; }
            catch (...) { /* Never unwind through a Windows callback. Fail open locally. */ }
        }
    }
    return CallNextHookEx(nullptr, code, message, data);
}
bool Win32KeyboardSink::send(const KeyboardEvent& event) {
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = event.key.scan == 0 ? event.key.vk : 0;
    input.ki.wScan = event.key.scan;
    input.ki.dwFlags = (event.key.scan != 0 ? KEYEVENTF_SCANCODE : 0U)
        | (event.key.extended ? KEYEVENTF_EXTENDEDKEY : 0U) | (!event.down ? KEYEVENTF_KEYUP : 0U);
    input.ki.dwExtraInfo = injection_tag;
    SetLastError(ERROR_SUCCESS);
    const bool ok = SendInput(1, &input, sizeof(INPUT)) == 1;
    error_ = ok ? ERROR_SUCCESS : GetLastError();
    return ok;
}
}
