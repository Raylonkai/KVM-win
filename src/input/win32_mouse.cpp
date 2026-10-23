#include "sharekeys/win32_mouse.hpp"
#include <system_error>

namespace sharekeys {
namespace {
std::uint8_t edgeAt(POINT point) {
    const auto left = GetSystemMetrics(SM_XVIRTUALSCREEN), top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    if (point.x <= left) return 1;
    if (point.x >= left + GetSystemMetrics(SM_CXVIRTUALSCREEN) - 1) return 2;
    if (point.y <= top) return 3;
    if (point.y >= top + GetSystemMetrics(SM_CYVIRTUALSCREEN) - 1) return 4;
    return 0;
}
}
std::uint8_t cursorEdge() { POINT cursor{}; return GetCursorPos(&cursor) ? edgeAt(cursor) : 0; }
bool centerPointer() {
    INPUT input{}; input.type = INPUT_MOUSE; input.mi.dwExtraInfo = injection_tag;
    input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
    input.mi.dx = 32768; input.mi.dy = 32768;
    return SendInput(1, &input, sizeof(INPUT)) == 1;
}
thread_local MouseHook* MouseHook::current_ = nullptr;
MouseHook::MouseHook(Callback callback) : callback_(std::move(callback)) {
    std::promise<DWORD> ready; auto result = ready.get_future();
    thread_ = std::thread([this, ready = std::move(ready)]() mutable { run(std::move(ready)); });
    const auto error = result.get();
    if (error) { thread_.join(); throw std::system_error(static_cast<int>(error), std::system_category(), "mouse hook"); }
}
MouseHook::~MouseHook() {
    if (thread_.joinable()) { PostThreadMessageW(threadId_.load(), WM_QUIT, 0, 0); thread_.join(); }
}
void MouseHook::run(std::promise<DWORD> ready) {
    current_ = this; MSG message{};
    PeekMessageW(&message, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
    threadId_.store(GetCurrentThreadId());
    const auto hook = SetWindowsHookExW(WH_MOUSE_LL, procedure, GetModuleHandleW(nullptr), 0);
    if (!hook) { ready.set_value(GetLastError()); current_ = nullptr; return; }
    ready.set_value(ERROR_SUCCESS);
    while (GetMessageW(&message, nullptr, 0, 0) > 0) { TranslateMessage(&message); DispatchMessageW(&message); }
    UnhookWindowsHookEx(hook); current_ = nullptr;
}
LRESULT CALLBACK MouseHook::procedure(int code, WPARAM message, LPARAM data) {
    auto* self = current_;
    if (code != HC_ACTION || !self) return CallNextHookEx(nullptr, code, message, data);
    const auto& raw = *reinterpret_cast<const MSLLHOOKSTRUCT*>(data);
    if ((raw.flags & (LLMHF_INJECTED | LLMHF_LOWER_IL_INJECTED)) != 0 || raw.dwExtraInfo == injection_tag) return CallNextHookEx(nullptr, code, message, data);
    MouseEvent event;
    if (message == WM_MOUSEMOVE) {
        POINT cursor{}; if (!GetCursorPos(&cursor)) return CallNextHookEx(nullptr, code, message, data);
        event = {MouseKind::Move, raw.pt.x - cursor.x, raw.pt.y - cursor.y};
        event.edge = edgeAt(raw.pt);
    } else if (message == WM_MOUSEWHEEL || message == WM_MOUSEHWHEEL) {
        const auto delta = static_cast<short>(HIWORD(raw.mouseData));
        event = {MouseKind::Wheel, message == WM_MOUSEWHEEL ? delta : 0, message == WM_MOUSEHWHEEL ? delta : 0};
    } else {
        event.kind = MouseKind::Button;
        switch (message) {
            case WM_LBUTTONDOWN: case WM_LBUTTONUP: event.button = 0; event.down = message == WM_LBUTTONDOWN; break;
            case WM_RBUTTONDOWN: case WM_RBUTTONUP: event.button = 1; event.down = message == WM_RBUTTONDOWN; break;
            case WM_MBUTTONDOWN: case WM_MBUTTONUP: event.button = 2; event.down = message == WM_MBUTTONDOWN; break;
            case WM_XBUTTONDOWN: case WM_XBUTTONUP: event.button = HIWORD(raw.mouseData) == XBUTTON1 ? 3 : 4; event.down = message == WM_XBUTTONDOWN; break;
            default: return CallNextHookEx(nullptr, code, message, data);
        }
        self->physical_[event.button].store(event.down);
    }
    try { if (self->callback_(event)) return 1; } catch (...) {}
    return CallNextHookEx(nullptr, code, message, data);
}
bool Win32MouseSink::sendMouse(const MouseEvent& event) {
    INPUT input{}; input.type = INPUT_MOUSE; input.mi.dwExtraInfo = injection_tag;
    if (event.kind == MouseKind::Move) {
        input.mi.dwFlags = MOUSEEVENTF_MOVE | (event.absolute ? MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK : 0U);
        input.mi.dx = event.x; input.mi.dy = event.y;
    }
    else if (event.kind == MouseKind::Button) {
        constexpr DWORD down[] = {MOUSEEVENTF_LEFTDOWN, MOUSEEVENTF_RIGHTDOWN, MOUSEEVENTF_MIDDLEDOWN, MOUSEEVENTF_XDOWN, MOUSEEVENTF_XDOWN};
        constexpr DWORD up[] = {MOUSEEVENTF_LEFTUP, MOUSEEVENTF_RIGHTUP, MOUSEEVENTF_MIDDLEUP, MOUSEEVENTF_XUP, MOUSEEVENTF_XUP};
        if (event.button >= 5) return false;
        input.mi.dwFlags = event.down ? down[event.button] : up[event.button];
        if (event.button >= 3) input.mi.mouseData = event.button == 3 ? XBUTTON1 : XBUTTON2;
    } else {
        if (event.x != 0 && event.y != 0) return sendMouse({MouseKind::Wheel, event.x, 0}) && sendMouse({MouseKind::Wheel, 0, event.y});
        input.mi.dwFlags = event.y != 0 ? MOUSEEVENTF_HWHEEL : MOUSEEVENTF_WHEEL;
        input.mi.mouseData = static_cast<DWORD>(event.y != 0 ? event.y : event.x);
    }
    return SendInput(1, &input, sizeof(INPUT)) == 1;
}
}
