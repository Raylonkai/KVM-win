#include "sharekeys/router.hpp"
#include "sharekeys/workspace.hpp"
#include <gtest/gtest.h>
using namespace sharekeys;
namespace {
using protocol::Type;
struct Delivery { DeviceId id; Type type; };
struct Routing : testing::Test {
    std::vector<Delivery> sent;
    InputRouter router{[&](const auto& id, auto type, const auto&) { sent.push_back({id, type}); return true; }};
    void SetUp() override { router.setOnline("a", true); router.setOnline("b", true); router.setMirrorTargets({"a", "b"}); sent.clear(); }
};
const KeyboardEvent key{{30, 65, false}, true, false, 1};
}
TEST_F(Routing, LocalRemoteAndMirrorSelection) {
    router.processKeyboardEvent(key); EXPECT_TRUE(sent.empty());
    router.processKeyboardEvent({key.key, false});
    ASSERT_TRUE(router.setActiveDevice("a"));
    ASSERT_TRUE(router.setMode(ControlMode::Remote));
    router.processKeyboardEvent(key); ASSERT_EQ(sent.size(), 1U); EXPECT_EQ(sent[0].id, "a");
    router.processKeyboardEvent({key.key, false});
    ASSERT_TRUE(router.setMode(ControlMode::Mirror)); sent.clear();
    router.processKeyboardEvent(key); EXPECT_EQ(sent.size(), 2U);
}
TEST_F(Routing, TransitionReleasesAndQuarantinesHeldKeys) {
    ASSERT_TRUE(router.setMode(ControlMode::Mirror));
    router.processKeyboardEvent(key); sent.clear();
    router.emergencyStop();
    ASSERT_EQ(sent.size(), 4U); EXPECT_EQ(sent[0].type, Type::ReleaseAllKeys);
    ASSERT_TRUE(router.setMode(ControlMode::Mirror)); sent.clear();
    router.processKeyboardEvent(key); EXPECT_TRUE(sent.empty());
    router.processKeyboardEvent({key.key, false}); EXPECT_TRUE(sent.empty());
    router.processKeyboardEvent(key); EXPECT_EQ(sent.size(), 2U);
}
TEST_F(Routing, DisconnectAndReconnectRequireDeliberateActivation) {
    ASSERT_TRUE(router.setMode(ControlMode::Mirror));
    router.setOnline("a", false); EXPECT_EQ(router.mode(), ControlMode::Local);
    router.setOnline("a", true); EXPECT_EQ(router.mode(), ControlMode::Local);
    EXPECT_FALSE(router.setActiveDevice("missing"));
}
TEST_F(Routing, FiveDeviceLimitAndGroups) {
    EXPECT_TRUE(router.setOnline("c", true)); EXPECT_TRUE(router.setOnline("d", true)); EXPECT_TRUE(router.setOnline("e", true));
    EXPECT_FALSE(router.setOnline("f", true));
    DeviceGroups groups;
    EXPECT_TRUE(groups.set("desk", {"a", "b", "a"})); EXPECT_EQ(groups.get("desk").size(), 2U);
    EXPECT_FALSE(groups.set("six", {"a", "b", "c", "d", "e", "f"}));
    EXPECT_TRUE(groups.get("missing").empty());
}
TEST_F(Routing, MouseMirroringIsIndependentAndDisabledByDefault) {
    ASSERT_TRUE(router.setMode(ControlMode::Mirror)); sent.clear();
    router.processMouse(Type::MouseMove, std::vector<std::uint8_t>(9)); EXPECT_TRUE(sent.empty());
    router.setMouseMirroring(true); sent.clear();
    router.processMouse(Type::MouseMove, std::vector<std::uint8_t>(9)); EXPECT_EQ(sent.size(), 2U);
}
TEST(Workspace, AdjacentMachinesAndReturnToHost) {
    Workspace workspace;
    ASSERT_TRUE(workspace.configure({{"a", {1, 0}}, {"b", {2, 0}}, {"c", {1, -1}}}));
    EXPECT_EQ(workspace.neighbor("", Edge::Right), "a");
    EXPECT_EQ(workspace.neighbor("a", Edge::Left), "");
    EXPECT_EQ(workspace.neighbor("a", Edge::Right), "b");
    EXPECT_EQ(workspace.neighbor("a", Edge::Top), "c");
    EXPECT_FALSE(workspace.neighbor("", Edge::Left));
    EXPECT_FALSE(workspace.configure({{"a", {0, 0}}}));
    EXPECT_FALSE(workspace.configure({{"a", {1, 0}}, {"b", {1, 0}}}));
}
TEST_F(Routing, MouseButtonsHeldAcrossTransitionsAreQuarantined) {
    router.processMouse(Type::MouseButton, {0, 1});
    ASSERT_TRUE(router.setActiveDevice("a")); ASSERT_TRUE(router.setMode(ControlMode::Remote)); sent.clear();
    router.processMouse(Type::MouseButton, {0, 1}); router.processMouse(Type::MouseButton, {0, 0}); EXPECT_TRUE(sent.empty());
    router.processMouse(Type::MouseButton, {0, 1}); EXPECT_EQ(sent.size(), 1U);
}
TEST(Shortcuts, EmergencyIsConsumedBeforeForwardingAndKeyupsStayBalanced) {
    ShortcutGate gate;
    gate.process({{0, 0xA2, false}, true}, false);
    gate.process({{0, 0xA4, false}, true}, false);
    auto result = gate.process({{0, 0x1B, false}, true}, true);
    ASSERT_TRUE(result.captured); EXPECT_EQ(result.captured->action, CaptureAction::Emergency); EXPECT_TRUE(result.suppress);
    EXPECT_FALSE(gate.process({{0, 0x1B, false}, false}, false).captured);
    EXPECT_FALSE(gate.process({{0, 0xA2, false}, false}, true).suppress);
    EXPECT_TRUE(gate.process({{0, 65, false}, true}, true).suppress);
    EXPECT_TRUE(gate.process({{0, 65, false}, false}, false).suppress);
}
TEST(Shortcuts, EmergencyQuarantinesRepeatsUntilPhysicalRelease) {
    ShortcutGate gate;
    ASSERT_TRUE(gate.process({{30, 65, false}, true}, true).captured);
    gate.quarantineHeld();
    const auto repeat = gate.process({{30, 65, false}, true}, false);
    EXPECT_FALSE(repeat.captured); EXPECT_TRUE(repeat.suppress);
    EXPECT_TRUE(gate.process({{30, 65, false}, false}, false).suppress);
    const auto fresh = gate.process({{30, 65, false}, true}, false);
    EXPECT_TRUE(fresh.captured); EXPECT_FALSE(fresh.suppress);
}
