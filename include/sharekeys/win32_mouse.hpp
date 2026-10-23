#pragma once
#include "sharekeys/mouse.hpp"
#include <windows.h>
#include <atomic>
#include <functional>
#include <future>
#include <thread>

namespace sharekeys {
std::uint8_t cursorEdge();
bool centerPointer();
class MouseHook {
public:
    using Callback = std::function<bool(const MouseEvent&)>;
    explicit MouseHook(Callback callback);
    ~MouseHook();
    MouseHook(const MouseHook&) = delete;
    MouseHook& operator=(const MouseHook&) = delete;
    bool physicallyHeld(std::uint8_t button) const { return button < physical_.size() && physical_[button].load(); }
private:
    static LRESULT CALLBACK procedure(int code, WPARAM message, LPARAM data);
    void run(std::promise<DWORD> ready);
    static thread_local MouseHook* current_;
    Callback callback_;
    std::array<std::atomic_bool, 5> physical_{};
    std::atomic<DWORD> threadId_{};
    std::thread thread_;
};
class Win32MouseSink final : public MouseSink {
public:
    explicit Win32MouseSink(const MouseHook& hook) : hook_(hook) {}
    bool sendMouse(const MouseEvent& event) override;
    bool physicalButton(std::uint8_t button) const override { return hook_.physicallyHeld(button); }
private:
    const MouseHook& hook_;
};
}
