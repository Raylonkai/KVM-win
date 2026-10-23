#include "sharekeys/protocol.hpp"

namespace sharekeys::protocol {
namespace {
void put(std::vector<std::uint8_t>& bytes, std::uint64_t value, unsigned count) {
    for (unsigned i = count; i > 0; --i) bytes.push_back(static_cast<std::uint8_t>(value >> ((i - 1) * 8)));
}
std::uint64_t get(std::span<const std::uint8_t> bytes, std::size_t offset, unsigned count) {
    if (offset + count > bytes.size()) throw std::runtime_error("truncated frame");
    std::uint64_t value{};
    for (unsigned i = 0; i < count; ++i) value = (value << 8) | bytes[offset + i];
    return value;
}
}
void validate(const Frame& frame) {
    const auto type = static_cast<std::uint16_t>(frame.type);
    if (type < 1 || type > 16 || frame.session == 0 || frame.sequence == 0) throw std::runtime_error("invalid header");
    if (frame.payload.size() > max_body - header_size) throw std::runtime_error("oversize frame");
    if (frame.type == Type::KeyDown || frame.type == Type::KeyUp) { (void)decodeKey(frame); return; }
    const auto size = frame.payload.size();
    if (frame.type == Type::MouseMove) { if (size != 9 || frame.payload[0] > 1) throw std::runtime_error("invalid mouse move"); }
    else if (frame.type == Type::MouseButton) {
        if (size != 2 || frame.payload[0] > 4 || frame.payload[1] > 1) throw std::runtime_error("invalid mouse button");
    } else if (frame.type == Type::MouseWheel) { if (size != 8) throw std::runtime_error("invalid wheel"); }
    else if (frame.type == Type::Heartbeat) {
        if (size != 0 && (size != 9 || frame.payload[0] > 1)) throw std::runtime_error("invalid heartbeat");
    } else if (frame.type == Type::DeviceStatus || frame.type == Type::Error) {
        if (size > 256) throw std::runtime_error("invalid status");
    } else if (size != 0) throw std::runtime_error("unexpected payload");
}
std::vector<std::uint8_t> encode(const Frame& frame) {
    validate(frame);
    std::vector<std::uint8_t> bytes;
    bytes.reserve(4 + header_size + frame.payload.size());
    put(bytes, header_size + frame.payload.size(), 4);
    put(bytes, version, 2); put(bytes, static_cast<std::uint16_t>(frame.type), 2);
    put(bytes, frame.session, 8); put(bytes, frame.sequence, 8);
    bytes.insert(bytes.end(), frame.payload.begin(), frame.payload.end());
    return bytes;
}
std::uint32_t bodyLength(std::span<const std::uint8_t> prefix) {
    if (prefix.size() != 4) throw std::runtime_error("invalid prefix");
    const auto size = static_cast<std::uint32_t>(get(prefix, 0, 4));
    if (size < header_size || size > max_body) throw std::runtime_error("invalid frame length");
    return size;
}
Frame decode(std::span<const std::uint8_t> body) {
    if (body.size() < header_size || body.size() > max_body || get(body, 0, 2) != version) throw std::runtime_error("invalid frame/version");
    Frame frame{static_cast<Type>(get(body, 2, 2)), get(body, 4, 8), get(body, 12, 8), {body.begin() + header_size, body.end()}};
    validate(frame); return frame;
}
std::vector<std::uint8_t> encodeKey(const KeyboardEvent& event) {
    std::vector<std::uint8_t> bytes;
    put(bytes, event.key.scan, 2); put(bytes, event.key.vk, 2); put(bytes, event.key.extended ? 1 : 0, 1);
    return bytes;
}
KeyboardEvent decodeKey(const Frame& frame) {
    if ((frame.type != Type::KeyDown && frame.type != Type::KeyUp) || frame.payload.size() != 5) throw std::runtime_error("invalid keyboard payload");
    const auto scan = get(frame.payload, 0, 2), vk = get(frame.payload, 2, 2);
    if (scan > 255 || vk == 0 || vk > 254 || frame.payload[4] > 1) throw std::runtime_error("invalid keyboard code");
    return {{static_cast<std::uint16_t>(scan), static_cast<std::uint16_t>(vk), frame.payload[4] != 0}, frame.type == Type::KeyDown, false, frame.sequence};
}
}
