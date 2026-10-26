#include "sharekeys/host_window.hpp"
#include "sharekeys/agent_window.hpp"
#include "sharekeys/desktop_application.hpp"
#include "sharekeys/security.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTableWidget>
#include <QThread>
#include <gtest/gtest.h>

using namespace sharekeys;
namespace {
struct Keys final : KeyboardSink {
    std::atomic_uint downs{}, ups{};
    bool send(const KeyboardEvent& event) override { if (event.down) ++downs; else ++ups; return true; }
    bool physicallyHeld(KeyId) const override { return false; }
};
struct Mouse final : MouseSink {
    bool sendMouse(const MouseEvent&) override { return true; }
    bool physicalButton(std::uint8_t) const override { return false; }
};
bool until(const std::function<bool()>& predicate) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(4);
    while (!predicate() && std::chrono::steady_clock::now() < deadline) {
        QApplication::processEvents(); QThread::msleep(2);
    }
    return predicate();
}
std::uint16_t freePort() {
    asio::io_context io;
    asio::ip::tcp::acceptor reserve(io, {asio::ip::address_v4::loopback(), 0});
    return reserve.local_endpoint().port();
}
class Desktop : public testing::Test {
protected:
    QTemporaryDir directory;
    Keys keys;
    Mouse mouse;
    std::filesystem::path hostPath, agentPath;
    std::string hostPin, agentPin;
    std::uint16_t port = freePort();
    void SetUp() override {
        ASSERT_TRUE(directory.isValid());
        hostPath = nativePath(directory.path()) / "host";
        agentPath = nativePath(directory.path()) / "agent";
        security::createIdentity(hostPath); security::createIdentity(agentPath);
        hostPin = security::fingerprint(hostPath / "identity.crt");
        agentPin = security::fingerprint(agentPath / "identity.crt");
        QFile config(directory.path() + "/host/host.json"); ASSERT_TRUE(config.open(QIODevice::WriteOnly));
        const QJsonObject device{{"name", "desk"}, {"address", "127.0.0.1"}, {"pin", QString::fromStdString(agentPin)}, {"port", port}, {"mirror", true}};
        config.write(QJsonDocument(QJsonObject{{"version", 1}, {"devices", QJsonArray{device}}, {"autoConnect", false}}).toJson());
    }
};
TEST_F(Desktop, MirrorButtonEmergencyAndCloseReleaseRemoteModifier) {
    AgentService agent(agentPath, hostPin, "127.0.0.1", port, keys, mouse);
    HostEngine host(hostPath);
    HostWindow window(host, directory.path() + "/host", QString::fromStdString(hostPin), true, true);
    window.show();
    auto* mirror = window.findChild<QPushButton*>("mirrorMode");
    auto* stop = window.findChild<QPushButton*>("emergencyStop");
    auto* closeToTray = window.findChild<QCheckBox*>("closeToTray");
    ASSERT_NE(mirror, nullptr); ASSERT_NE(stop, nullptr); ASSERT_NE(closeToTray, nullptr);
    EXPECT_FALSE(closeToTray->isChecked()); EXPECT_FALSE(mirror->isEnabled());
    host.connect({"desk", "127.0.0.1", agentPin, port});
    ASSERT_TRUE(until([&] { return mirror->isEnabled(); }));
    auto* devices = window.findChild<QTableWidget*>("devices"); ASSERT_NE(devices, nullptr);
    devices->selectRow(0);
    auto* remote = window.findChild<QPushButton*>("remoteMode"); ASSERT_NE(remote, nullptr);
    ASSERT_TRUE(until([&] { return remote->isEnabled(); }));
    remote->click();
    ASSERT_TRUE(until([&] { return host.status().mode == ControlMode::Remote && host.status().active == "desk"; }));
    mirror->click();
    ASSERT_TRUE(until([&] { return host.status().mode == ControlMode::Mirror; }));
    mirror->click();
    ASSERT_TRUE(until([&] { return host.status().mode == ControlMode::Local; }));
    mirror->click();
    ASSERT_TRUE(until([&] { return host.status().mode == ControlMode::Mirror; }));
    ASSERT_TRUE(until([&] { return mirror->text().startsWith("Disable"); }));
    if (qEnvironmentVariableIsSet("SHAREKEYS_TEST_SCREENSHOTS")) {
        EXPECT_TRUE(window.grab().save(qEnvironmentVariable("SHAREKEYS_TEST_SCREENSHOTS") + "/host.png"));
    }
    EXPECT_FALSE(host.captureKey({{29, 0xA2, false}, true})); // Local execution retained.
    ASSERT_TRUE(until([&] { return keys.downs == 1; }));
    stop->click();
    ASSERT_TRUE(until([&] { return keys.ups == 1 && host.status().mode == ControlMode::Local; }));
    host.captureKey({{29, 0xA2, false}, false});
    mirror->click();
    ASSERT_TRUE(until([&] { return host.status().mode == ControlMode::Mirror; }));
    host.captureKey({{29, 0xA2, false}, true});
    ASSERT_TRUE(until([&] { return keys.downs == 2; }));
    EXPECT_TRUE(window.close());
    ASSERT_TRUE(until([&] { return keys.ups == 2 && host.status().mode == ControlMode::Local; }));
}
TEST_F(Desktop, AgentWindowStartsRealServiceAndReleasesOnClose) {
    const auto agentDirectory = directory.path() + "/agent";
    saveAgentSettings(agentDirectory, {QString::fromStdString(hostPin), "127.0.0.1", port, false});
    AgentWindow window(agentDirectory, [&](const AgentSettings& settings) {
        return std::make_unique<AgentService>(agentPath, settings.hostPin.toStdString(), settings.address.toStdString(), static_cast<std::uint16_t>(settings.port), keys, mouse);
    }, true);
    window.show();
    auto* start = window.findChild<QPushButton*>("startListening"); ASSERT_NE(start, nullptr);
    start->click(); ASSERT_TRUE(window.running());
    QApplication::processEvents();
    if (qEnvironmentVariableIsSet("SHAREKEYS_TEST_SCREENSHOTS")) {
        EXPECT_TRUE(window.grab().save(qEnvironmentVariable("SHAREKEYS_TEST_SCREENSHOTS") + "/agent.png"));
    }
    HostEngine host(hostPath); host.connect({"desk", "127.0.0.1", agentPin, port});
    ASSERT_TRUE(until([&] { const auto s = host.status(); return s.devices.size() == 1 && s.devices[0].online; }));
    host.setTargets({"desk"}); host.setMode(ControlMode::Mirror);
    ASSERT_TRUE(until([&] { return host.status().mode == ControlMode::Mirror; }));
    host.captureKey({{29, 0xA2, false}, true});
    ASSERT_TRUE(until([&] { return keys.downs == 1; }));
    EXPECT_TRUE(window.close()); EXPECT_FALSE(window.running()); EXPECT_EQ(keys.ups, 1U);
    ASSERT_TRUE(until([&] { return host.status().mode == ControlMode::Local; }));
}
TEST_F(Desktop, UnavailableAgentKeepsGuiResponsiveAndLocal) {
    HostEngine host(hostPath);
    HostWindow window(host, directory.path() + "/host", QString::fromStdString(hostPin), true, true);
    window.show(); host.connect({"desk", "127.0.0.1", agentPin, port});
    ASSERT_TRUE(until([&] { const auto s = host.status(); return !s.devices.empty() && s.devices[0].retries > 0; }));
    EXPECT_EQ(host.status().mode, ControlMode::Local);
    EXPECT_FALSE(window.findChild<QPushButton*>("mirrorMode")->isEnabled());
    EXPECT_TRUE(window.close());
}
}
int main(int argc, char** argv) {
    QApplication app(argc, argv); app.setQuitOnLastWindowClosed(false);
    initializeDesktopApplication();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
