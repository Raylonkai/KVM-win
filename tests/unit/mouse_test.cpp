#include "sharekeys/mouse.hpp"
#include <gtest/gtest.h>
using namespace sharekeys;
TEST(Mouse, SignedDeltasAndHorizontalWheelRoundtrip) {
    for (auto kind : {MouseKind::Move, MouseKind::Wheel}) {
        const MouseEvent event{kind, -120, 240};
        const auto decoded = decodeMouse({mouseType(kind), 1, 1, encodeMouse(event)});
        EXPECT_EQ(decoded.kind, kind); EXPECT_EQ(decoded.x, -120); EXPECT_EQ(decoded.y, 240);
    }
    EXPECT_THROW(decodeMouse((protocol::Frame{protocol::Type::MouseMove, 1, 1, encodeMouse({MouseKind::Move, INT32_MIN, 0})})), std::runtime_error);
    const auto absolute = decodeMouse({protocol::Type::MouseMove, 1, 1, encodeMouse({MouseKind::Move, 65535, 32768, 0, false, true})});
    EXPECT_TRUE(absolute.absolute); EXPECT_EQ(absolute.x, 65535); EXPECT_EQ(absolute.y, 32768);
}
TEST(Mouse, CleanupRetainsFailedReleasesAndProtectsPhysicalButtons) {
    struct Sink : MouseSink {
        bool fail{}, physical{}; unsigned releases{};
        bool sendMouse(const MouseEvent& event) override { if (fail) return false; if (!event.down) ++releases; return true; }
        bool physicalButton(std::uint8_t) const override { return physical; }
    } sink;
    InjectedMouse mouse(sink);
    ASSERT_TRUE(mouse.apply({MouseKind::Button, 0, 0, 0, true}));
    sink.fail = true; EXPECT_FALSE(mouse.releaseAll());
    sink.fail = false; EXPECT_TRUE(mouse.releaseAll()); EXPECT_EQ(sink.releases, 1U);
    ASSERT_TRUE(mouse.apply({MouseKind::Button, 0, 0, 0, true}));
    sink.physical = true; EXPECT_TRUE(mouse.releaseAll()); EXPECT_EQ(sink.releases, 1U);
}
