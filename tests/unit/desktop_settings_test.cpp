#include "sharekeys/desktop_settings.hpp"
#include <QTemporaryDir>
#include <QFile>
#include <gtest/gtest.h>
using namespace sharekeys;
TEST(DesktopSettings, PairingAndAutomaticListeningSurviveRestart) {
    QTemporaryDir directory; ASSERT_TRUE(directory.isValid());
    const AgentSettings original{QString(64, 'a'), "192.168.1.25", 24871, true};
    saveAgentSettings(directory.path(), original);
    const auto saved = loadAgentSettings(directory.path());
    EXPECT_EQ(saved.hostPin, original.hostPin); EXPECT_EQ(saved.address, original.address);
    EXPECT_EQ(saved.port, original.port); EXPECT_TRUE(saved.autoListen);
    auto revoked = saved; revoked.hostPin.clear(); revoked.autoListen = false;
    saveAgentSettings(directory.path(), revoked);
    EXPECT_TRUE(loadAgentSettings(directory.path()).hostPin.isEmpty());
    EXPECT_FALSE(loadAgentSettings(directory.path()).autoListen);
}
TEST(DesktopSettings, InvalidPairingCannotReplaceWorkingSettings) {
    QTemporaryDir directory;
    const AgentSettings original{QString(64, 'a'), "192.168.1.25", 24870, true};
    saveAgentSettings(directory.path(), original);
    auto invalid = original; invalid.hostPin = "unverified";
    EXPECT_THROW(saveAgentSettings(directory.path(), invalid), std::invalid_argument);
    invalid = original; invalid.address = "0.0.0.0";
    EXPECT_THROW(saveAgentSettings(directory.path(), invalid), std::invalid_argument);
    invalid.address = "8.8.8.8";
    EXPECT_THROW(saveAgentSettings(directory.path(), invalid), std::invalid_argument);
    EXPECT_EQ(loadAgentSettings(directory.path()).hostPin, original.hostPin);
    EXPECT_EQ(loadAgentSettings(directory.path()).address, original.address);
}
TEST(DesktopSettings, LegacyPairingLoadsWithoutAutomaticListening) {
    QTemporaryDir directory;
    QFile file(directory.path() + "/authorized-host.json"); ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write("{\"hostPin\":\"" + QByteArray(64, 'b') + "\"}"); file.close();
    const auto saved = loadAgentSettings(directory.path());
    EXPECT_EQ(saved.hostPin, QString(64, 'b')); EXPECT_FALSE(saved.autoListen); EXPECT_EQ(saved.port, 24870);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Truncate)); file.write("broken"); file.close();
    EXPECT_THROW(loadAgentSettings(directory.path()), std::runtime_error);
}
TEST(DesktopSettings, TrayBehaviorRequiresExplicitPreferenceAndSurvivesRestart) {
    QTemporaryDir directory; ASSERT_TRUE(directory.isValid());
    const auto defaults = loadDesktopPreferences(directory.path());
    EXPECT_FALSE(defaults.closeToTray); EXPECT_FALSE(defaults.minimizeToTray);
    saveDesktopPreferences(directory.path(), {true, true});
    EXPECT_TRUE(loadDesktopPreferences(directory.path()).closeToTray);
    EXPECT_TRUE(loadDesktopPreferences(directory.path()).minimizeToTray);
    saveDesktopPreferences(directory.path(), {});
    EXPECT_FALSE(loadDesktopPreferences(directory.path()).closeToTray);
}
