#pragma once
#include "sharekeys/protocol.hpp"
#include <array>

namespace sharekeys {
enum class MouseKind { Move, Button, Wheel };
struct MouseEvent {
    MouseKind kind{};
    std::int32_t x{}, y{}; // Relative motion, or vertical/horizontal wheel deltas.
    std::uint8_t button{}; // Left, right, middle, X1, X2.
    bool down{};
    bool absolute{}; // Normalized 0..65535 coordinates for desktop transitions.
    std::uint8_t edge{}; // Capture metadata only; not part of a mouse command.
};
protocol::Type mouseType(MouseKind kind);
std::vector<std::uint8_t> encodeMouse(const MouseEvent& event);
MouseEvent decodeMouse(const protocol::Frame& frame);
class MouseSink {
public:
    virtual ~MouseSink() = default;
    virtual bool sendMouse(const MouseEvent&) = 0;
    virtual bool physicalButton(std::uint8_t button) const = 0;
};
class InjectedMouse {
public:
    explicit InjectedMouse(MouseSink& sink) : sink_(sink) {}
    ~InjectedMouse() { releaseAll(); }
    bool apply(const MouseEvent& event);
    bool releaseAll();
private:
    MouseSink& sink_;
    std::array<bool, 5> pressed_{};
};
}
