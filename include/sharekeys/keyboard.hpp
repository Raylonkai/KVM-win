#pragma once
#include "sharekeys/types.hpp"
#include <set>
#include <array>

namespace sharekeys {
class ModifierState {
public:
    void update(const KeyboardEvent& event);
    [[nodiscard]] bool ctrl() const;
    [[nodiscard]] bool alt() const;
    [[nodiscard]] bool shift() const;
private:
    std::array<bool, 256> held_{};
};
class KeyboardSink {
public:
    virtual ~KeyboardSink() = default;
    virtual bool send(const KeyboardEvent& event) = 0;
    virtual bool physicallyHeld(KeyId key) const = 0;
};
// Single-thread confined. Retain failed releases for watchdog retries.
class InjectedKeyboard {
public:
    explicit InjectedKeyboard(KeyboardSink& sink) : sink_(sink) {}
    ~InjectedKeyboard();
    bool apply(const KeyboardEvent& event);
    bool releaseAll();
    [[nodiscard]] std::size_t pressedCount() const { return pressed_.size(); }
private:
    KeyboardSink& sink_;
    std::set<KeyId> pressed_;
};
}
