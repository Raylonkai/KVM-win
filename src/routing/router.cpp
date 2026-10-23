#include "sharekeys/router.hpp"
#include <algorithm>

namespace sharekeys {
using protocol::Type;
std::vector<DeviceId> InputRouter::recipients() const {
    if (mode_ == ControlMode::Remote && online_.contains(active_)) return {active_};
    std::vector<DeviceId> result;
    if (mode_ == ControlMode::Mirror) for (const auto& id : targets_) if (online_.contains(id)) result.push_back(id);
    return result;
}
void InputRouter::cleanup() {
    for (const auto& id : recipients()) {
        send_(id, Type::ReleaseAllKeys, {});
        if (mode_ == ControlMode::Mirror) send_(id, Type::MirrorStop, {});
    }
    quarantine_ = held_;
    mouseQuarantine_ = mouseHeld_;
}
bool InputRouter::setOnline(const DeviceId& id, bool online) {
    if (online) {
        if (!online_.contains(id) && online_.size() >= max_devices) return false;
        // Reconnection never resumes forwarding without deliberate activation.
        if (!online_.contains(id)) emergencyStop();
        online_.insert(id);
    } else {
        if (online_.contains(id)) emergencyStop();
        online_.erase(id);
    }
    return true;
}
bool InputRouter::setMode(ControlMode mode) {
    if (mode == ControlMode::Remote && !online_.contains(active_)) return false;
    if (mode == ControlMode::Mirror && std::none_of(targets_.begin(), targets_.end(), [&](const auto& id) { return online_.contains(id); })) return false;
    if (mode_ == mode) return true;
    cleanup(); mode_ = mode;
    if (mode_ == ControlMode::Mirror) for (const auto& id : recipients()) if (!send_(id, Type::MirrorStart, {})) { emergencyStop(); return false; }
    return true;
}
bool InputRouter::setActiveDevice(const DeviceId& id) {
    if (!online_.contains(id)) return false;
    cleanup(); active_ = id; return true;
}
bool InputRouter::setMirrorTargets(const std::vector<DeviceId>& targets) {
    const std::set<DeviceId> unique(targets.begin(), targets.end());
    if (unique.size() > max_devices) return false;
    emergencyStop(); targets_ = unique; return true;
}
void InputRouter::emergencyStop() { cleanup(); mode_ = ControlMode::Local; }
void InputRouter::processKeyboardEvent(const KeyboardEvent& event) {
    if (event.injected) return;
    if (event.down) held_.insert(event.key); else held_.erase(event.key);
    if (quarantine_.contains(event.key)) { if (!event.down) quarantine_.erase(event.key); return; }
    for (const auto& id : recipients()) {
        if (!send_(id, event.down ? Type::KeyDown : Type::KeyUp, protocol::encodeKey(event))) { emergencyStop(); return; }
    }
}
void InputRouter::processMouse(Type type, const std::vector<std::uint8_t>& payload) {
    if (type == Type::MouseButton && payload.size() == 2 && payload[0] < mouseHeld_.size()) {
        const auto button = payload[0]; const bool down = payload[1] != 0;
        mouseHeld_[button] = down;
        if (mouseQuarantine_[button]) { if (!down) mouseQuarantine_[button] = false; return; }
    }
    if (mode_ == ControlMode::Local || (mode_ == ControlMode::Mirror && !mouseMirror_)) return;
    for (const auto& id : recipients()) if (!send_(id, type, payload)) { emergencyStop(); return; }
}
bool DeviceGroups::set(std::string name, std::vector<DeviceId> devices) {
    std::sort(devices.begin(), devices.end()); devices.erase(std::unique(devices.begin(), devices.end()), devices.end());
    if (name.empty() || devices.size() > max_devices) return false;
    groups_[std::move(name)] = std::move(devices); return true;
}
std::vector<DeviceId> DeviceGroups::get(const std::string& name) const {
    const auto it = groups_.find(name); return it == groups_.end() ? std::vector<DeviceId>{} : it->second;
}
GateResult ShortcutGate::process(const KeyboardEvent& event, bool exclusive) {
    if (event.injected || event.key.vk >= held_.size()) return {};
    const auto vk = event.key.vk;
    modifiers_.update(event);
    if (quarantine_[vk] && event.down) return {suppressed_[vk] || shortcut_[vk], {}};
    if (!event.down) quarantine_[vk] = false;
    if (shortcut_[vk]) {
        if (!event.down) { shortcut_[vk] = false; held_[vk] = false; }
        return {true, {}};
    }
    if (event.down && !held_[vk] && modifiers_.ctrl() && modifiers_.alt() && (vk == emergency_ || vk == mirror_ || vk == next_)) {
        held_[vk] = shortcut_[vk] = true;
        const auto action = vk == emergency_ ? CaptureAction::Emergency : (vk == mirror_ ? CaptureAction::ToggleMirror : CaptureAction::NextDevice);
        return {true, CapturedKey{action, event}};
    }
    if (event.down && !held_[vk]) suppressed_[vk] = exclusive;
    const bool suppress = suppressed_[vk];
    held_[vk] = event.down;
    if (!event.down) suppressed_[vk] = false;
    return {suppress, CapturedKey{CaptureAction::Key, event}};
}
}
