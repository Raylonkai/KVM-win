#include "sharekeys/keyboard.hpp"
#include "sharekeys/queue.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <vector>
using namespace sharekeys;
namespace {
struct Sink : KeyboardSink {
    bool fail{}, physical{};
    std::vector<KeyboardEvent> events;
    bool send(const KeyboardEvent& e) override { if (fail) return false; events.push_back(e); return true; }
    bool physicallyHeld(KeyId) const override { return physical; }
};
const KeyboardEvent down{{30, 65, false}, true, false, 1};
}
TEST(Keyboard, TracksAndReleasesOnlyOwnedKeys) {
    Sink sink;
    InjectedKeyboard keyboard(sink);
    EXPECT_TRUE(keyboard.apply(down));
    EXPECT_TRUE(keyboard.apply(down)); // Repeat is intentional.
    EXPECT_EQ(keyboard.pressedCount(), 1U);
    EXPECT_TRUE(keyboard.releaseAll());
    EXPECT_FALSE(sink.events.back().down);
    EXPECT_EQ(keyboard.pressedCount(), 0U);
    EXPECT_TRUE(keyboard.apply({{48, 66, false}, false, false, 2}));
    EXPECT_EQ(sink.events.size(), 3U);
}
TEST(Keyboard, FailedInjectionAndReleaseAreTrackedCorrectly) {
    Sink sink;
    InjectedKeyboard keyboard(sink);
    sink.fail = true;
    EXPECT_FALSE(keyboard.apply(down));
    EXPECT_EQ(keyboard.pressedCount(), 0U);
    sink.fail = false;
    ASSERT_TRUE(keyboard.apply(down));
    sink.fail = true;
    EXPECT_FALSE(keyboard.releaseAll());
    EXPECT_EQ(keyboard.pressedCount(), 1U);
    sink.fail = false;
    EXPECT_TRUE(keyboard.releaseAll());
}
TEST(Keyboard, PhysicalKeysAreNotReleased) {
    Sink sink;
    InjectedKeyboard keyboard(sink);
    ASSERT_TRUE(keyboard.apply(down));
    sink.physical = true;
    EXPECT_TRUE(keyboard.releaseAll());
    EXPECT_EQ(sink.events.size(), 1U);
}
TEST(Keyboard, LeftAndRightModifiersAreIndependent) {
    ModifierState state;
    state.update({{0, 0xA2, false}, true});
    state.update({{0, 0xA3, false}, true});
    state.update({{0, 0xA2, false}, false});
    EXPECT_TRUE(state.ctrl());
    state.update({{0, 0xA3, false}, false});
    EXPECT_FALSE(state.ctrl());
}
TEST(Queue, FullEmptyAndWraparound) {
    SpscQueue<int, 2> queue;
    int value{};
    EXPECT_FALSE(queue.pop(value));
    ASSERT_TRUE(queue.push(1));
    ASSERT_TRUE(queue.push(2));
    EXPECT_FALSE(queue.push(3));
    ASSERT_TRUE(queue.pop(value)); EXPECT_EQ(value, 1);
    ASSERT_TRUE(queue.push(3));
    ASSERT_TRUE(queue.pop(value)); EXPECT_EQ(value, 2);
    ASSERT_TRUE(queue.pop(value)); EXPECT_EQ(value, 3);
}
TEST(Queue, ConcurrentOrdering) {
    SpscQueue<int, 64> queue;
    std::jthread producer([&] { for (int i = 0; i < 100000; ++i) while (!queue.push(i)) std::this_thread::yield(); });
    for (int i = 0; i < 100000; ++i) {
        int value;
        while (!queue.pop(value)) std::this_thread::yield();
        ASSERT_EQ(value, i);
    }
}
TEST(Queue, MultipleProducersPreserveEachStreamAndCapacity) {
    struct Event { int producer{}, number{}; };
    MpscQueue<Event, 64> queue;
    for (int i = 0; i < 64; ++i) ASSERT_TRUE(queue.push({0, i}));
    EXPECT_FALSE(queue.push({0, 64}));
    Event event;
    for (int i = 0; i < 64; ++i) { ASSERT_TRUE(queue.pop(event)); EXPECT_EQ(event.number, i); }
    EXPECT_FALSE(queue.pop(event));
    auto produce = [&](int producer) { for (int i = 0; i < 100000; ++i) while (!queue.push({producer, i})) std::this_thread::yield(); };
    std::jthread keyboard(produce, 0), mouse(produce, 1);
    std::array<int, 2> next{};
    for (int i = 0; i < 200000; ++i) {
        while (!queue.pop(event)) std::this_thread::yield();
        EXPECT_EQ(event.number, next[static_cast<std::size_t>(event.producer)]++);
    }
    EXPECT_EQ(next[0], 100000); EXPECT_EQ(next[1], 100000);
}
