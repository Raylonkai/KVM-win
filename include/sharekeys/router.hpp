#pragma once
#include "sharekeys/keyboard.hpp"
#include "sharekeys/protocol.hpp"
#include <functional>
#include <map>
#include <optional>

namespace sharekeys {
class InputRouter {
public:
    using Send = std::function<bool(const DeviceId&, protocol::Type, const std::vector<std::uint8_t>&)>;
    explicit InputRouter(Send send) : send_(std::move(send)) {}
    bool setOnline(const DeviceId& id, bool online);
    bool setMode(ControlMode mode);
    bool setActiveDevice(const DeviceId& id);
    bool setMirrorTargets(const std::vector<DeviceId>& targets);
    void processKeyboardEvent(const KeyboardEvent& event);
    void processMouse(protocol::Type type, const std::vector<std::uint8_t>& payload);
    void setMouseMirroring(bool enabled) { cleanup(); mouseMirror_ = enabled; }
    void emergencyStop();
    ControlMode mode() const { return mode_; }
    const DeviceId& activeDevice() const { return active_; }
    std::size_t onlineCount() const { return online_.size(); }
private:
    std::vector<DeviceId> recipients() const;
    void cleanup();
    Send send_;
    ControlMode mode_{ControlMode::Local};
    DeviceId active_;
    std::set<DeviceId> online_, targets_;
    std::set<KeyId> held_, quarantine_;
    std::array<bool, 5> mouseHeld_{}, mouseQuarantine_{};
    bool mouseMirror_{};
};
class DeviceGroups {
public:
    bool set(std::string name, std::vector<DeviceId> devices);
    std::vector<DeviceId> get(const std::string& name) const;
private:
    std::map<std::string, std::vector<DeviceId>> groups_;
};
enum class CaptureAction { Key, ToggleMirror, Emergency, NextDevice };
struct CapturedKey { CaptureAction action{}; KeyboardEvent event{}; };
struct GateResult { bool suppress{}; std::optional<CapturedKey> captured; };
// Hook-thread-only state. A key-up has the same local suppression as its down,
// even if mode changes while held. Shortcuts are consumed before forwarding.
class ShortcutGate {
public:
    GateResult process(const KeyboardEvent& event, bool exclusive);
    void quarantineHeld() { quarantine_ = held_; }
    void setKeys(std::uint16_t mirror, std::uint16_t emergency, std::uint16_t next) { mirror_ = mirror; emergency_ = emergency; next_ = next; }
private:
    ModifierState modifiers_;
    std::array<bool, 256> held_{}, suppressed_{}, shortcut_{}, quarantine_{};
    std::uint16_t mirror_{0x4D}, emergency_{0x1B}, next_{0x09};
};
}
