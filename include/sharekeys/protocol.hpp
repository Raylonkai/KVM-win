#pragma once
#include "sharekeys/types.hpp"
#include <span>
#include <vector>
#include <stdexcept>

namespace sharekeys::protocol {
constexpr std::uint16_t version = 1;
constexpr std::size_t header_size = 20;
constexpr std::size_t max_body = 4096;
enum class Type : std::uint16_t {
    Hello = 1, PairRequest, PairApproved, Authenticate, Heartbeat, KeyDown, KeyUp,
    MouseMove, MouseButton, MouseWheel, ReleaseAllKeys, MirrorStart, MirrorStop,
    DeviceStatus, Disconnect, Error
};
struct Frame {
    Type type{};
    std::uint64_t session{}, sequence{};
    std::vector<std::uint8_t> payload;
};
std::vector<std::uint8_t> encode(const Frame& frame);
Frame decode(std::span<const std::uint8_t> body);
std::uint32_t bodyLength(std::span<const std::uint8_t> prefix);
std::vector<std::uint8_t> encodeKey(const KeyboardEvent& event);
KeyboardEvent decodeKey(const Frame& frame);
void validate(const Frame& frame);
class Sequence {
public:
    bool accept(std::uint64_t value) {
        if (last_ == UINT64_MAX || value != last_ + 1) return false;
        last_ = value; return true;
    }
private:
    std::uint64_t last_{};
};
}
