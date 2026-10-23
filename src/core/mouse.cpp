#include "sharekeys/mouse.hpp"
#include <bit>

namespace sharekeys {
protocol::Type mouseType(MouseKind kind) {
    switch (kind) {
        case MouseKind::Move: return protocol::Type::MouseMove;
        case MouseKind::Button: return protocol::Type::MouseButton;
        case MouseKind::Wheel: return protocol::Type::MouseWheel;
    }
    throw std::invalid_argument("invalid mouse kind");
}
std::vector<std::uint8_t> encodeMouse(const MouseEvent& event) {
    if (event.kind == MouseKind::Button) return {event.button, static_cast<std::uint8_t>(event.down)};
    std::vector<std::uint8_t> result;
    if (event.kind == MouseKind::Move) result.push_back(event.absolute ? 1 : 0);
    for (const auto signedValue : {event.x, event.y}) {
        const auto value = std::bit_cast<std::uint32_t>(signedValue);
        for (unsigned i = 4; i > 0; --i) result.push_back(static_cast<std::uint8_t>(value >> ((i - 1) * 8)));
    }
    return result;
}
MouseEvent decodeMouse(const protocol::Frame& frame) {
    protocol::validate(frame);
    if (frame.type == protocol::Type::MouseButton) return {MouseKind::Button, 0, 0, frame.payload[0], frame.payload[1] != 0};
    if (frame.type != protocol::Type::MouseMove && frame.type != protocol::Type::MouseWheel) throw std::invalid_argument("not mouse input");
    auto read = [&](std::size_t offset) {
        std::uint32_t value{};
        for (std::size_t i = 0; i < 4; ++i) value = (value << 8) | frame.payload[offset + i];
        return std::bit_cast<std::int32_t>(value);
    };
    const bool move = frame.type == protocol::Type::MouseMove;
    const bool absolute = move && frame.payload[0] != 0;
    const auto x = read(move ? 1 : 0), y = read(move ? 5 : 4);
    if (absolute ? (x < 0 || x > 65535 || y < 0 || y > 65535) : (x < -32767 || x > 32767 || y < -32767 || y > 32767)) throw std::runtime_error("mouse coordinate out of bounds");
    return {move ? MouseKind::Move : MouseKind::Wheel, x, y, 0, false, absolute};
}
bool InjectedMouse::apply(const MouseEvent& event) {
    if (event.kind != MouseKind::Button) return sink_.sendMouse(event);
    if (event.button >= pressed_.size()) return false;
    if (!event.down && !pressed_[event.button]) return true;
    if (sink_.physicalButton(event.button)) { if (!event.down) pressed_[event.button] = false; return true; }
    if (!sink_.sendMouse(event)) return false;
    pressed_[event.button] = event.down; return true;
}
bool InjectedMouse::releaseAll() {
    bool result = true;
    for (std::uint8_t i = 0; i < pressed_.size(); ++i) {
        if (!pressed_[i]) continue;
        if (sink_.physicalButton(i) || sink_.sendMouse({MouseKind::Button, 0, 0, i, false})) pressed_[i] = false;
        else result = false;
    }
    return result;
}
}
