#pragma once
#include <cstdint>
#include <cstddef>
#include <compare>
#include <string>

namespace sharekeys {
using DeviceId = std::string;
enum class ControlMode { Local, Remote, Mirror };
struct KeyId {
    std::uint16_t scan{};
    std::uint16_t vk{};
    bool extended{};
    auto operator<=>(const KeyId&) const = default;
};
struct KeyboardEvent {
    KeyId key;
    bool down{};
    bool injected{};
    std::uint64_t sequence{};
};
inline constexpr std::uintptr_t injection_tag = 0x534B4559;
inline constexpr std::size_t max_devices = 5;
}
