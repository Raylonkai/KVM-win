#include "sharekeys/keyboard.hpp"

namespace sharekeys {
void ModifierState::update(const KeyboardEvent& event) {
    if (event.key.vk < held_.size()) held_[event.key.vk] = event.down;
}
bool ModifierState::ctrl() const { return held_[0x11] || held_[0xA2] || held_[0xA3]; }
bool ModifierState::alt() const { return held_[0x12] || held_[0xA4] || held_[0xA5]; }
bool ModifierState::shift() const { return held_[0x10] || held_[0xA0] || held_[0xA1]; }
InjectedKeyboard::~InjectedKeyboard() { releaseAll(); }
bool InjectedKeyboard::apply(const KeyboardEvent& event) {
    if (!event.down && !pressed_.contains(event.key)) return true;
    if (sink_.physicallyHeld(event.key)) {
        if (!event.down) pressed_.erase(event.key);
        return true;
    }
    if (!sink_.send(event)) return false;
    if (event.down) pressed_.insert(event.key);
    else pressed_.erase(event.key);
    return true;
}
bool InjectedKeyboard::releaseAll() {
    bool success = true;
    for (auto it = pressed_.begin(); it != pressed_.end();) {
        if (sink_.physicallyHeld(*it) || sink_.send({*it, false, true, 0})) it = pressed_.erase(it);
        else { success = false; ++it; }
    }
    return success;
}
}
