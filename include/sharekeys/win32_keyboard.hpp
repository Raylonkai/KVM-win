#pragma once
#include "sharekeys/keyboard.hpp"
#include <windows.h>
#include <array>
#include <atomic>
#include <functional>
#include <future>
#include <thread>

namespace sharekeys {
// Callback runs on hook thread: it must perform no I/O or blocking work.
class KeyboardHook {
public:
    using Callback = std::function<bool(const KeyboardEvent&)>;
    explicit KeyboardHook(Callback callback, bool observeInjected = false);
    ~KeyboardHook();
    KeyboardHook(const KeyboardHook&) = delete;
    KeyboardHook& operator=(const KeyboardHook&) = delete;
    bool physicallyHeld(KeyId key) const;
private:
    static LRESULT CALLBACK procedure(int code, WPARAM message, LPARAM data);
    void run(std::promise<DWORD> ready);
    static thread_local KeyboardHook* current_;
    Callback callback_;
    bool observeInjected_;
    std::array<std::atomic_bool, 256> physical_{};
    std::uint64_t sequence_{};
    std::atomic<DWORD> threadId_{};
    std::thread thread_;
};
class Win32KeyboardSink final : public KeyboardSink {
public:
    explicit Win32KeyboardSink(const KeyboardHook& hook) : hook_(hook) {}
    bool send(const KeyboardEvent& event) override;
    bool physicallyHeld(KeyId key) const override { return hook_.physicallyHeld(key); }
    DWORD lastError() const { return error_; }
private:
    const KeyboardHook& hook_;
    DWORD error_{};
};
}
