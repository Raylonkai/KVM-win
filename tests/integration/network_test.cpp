#include "sharekeys/network.hpp"
#include "sharekeys/security.hpp"
#include "sharekeys/host_engine.hpp"
#include "sharekeys/agent_service.hpp"
#include <gtest/gtest.h>
#include <filesystem>
#include <future>
using namespace sharekeys;
namespace {
struct Identities {
    std::filesystem::path directory = std::filesystem::temp_directory_path() / ("sharekeys-test-" + std::to_string(security::randomSession()));
    Identities() { security::createIdentity(directory / "host"); security::createIdentity(directory / "agent"); }
    ~Identities() { std::error_code error; std::filesystem::remove_all(directory, error); }
};
}
TEST(Security, IdentityIsStableAndCannotBeOverwritten) {
    Identities ids;
    const auto pin = security::fingerprint(ids.directory / "host/identity.crt");
    EXPECT_TRUE(security::validPin(pin));
    EXPECT_NE(pin, security::fingerprint(ids.directory / "agent/identity.crt"));
    EXPECT_THROW(security::createIdentity(ids.directory / "host"), std::runtime_error);
    EXPECT_EQ(pin, security::fingerprint(ids.directory / "host/identity.crt"));
}
TEST(AgentService, GuiOwnedServiceAuthenticatesInjectsAndReleasesOnStop) {
    Identities ids;
    struct Keys : KeyboardSink {
        std::atomic_uint downs{}, ups{};
        bool send(const KeyboardEvent& event) override { if (event.down) ++downs; else ++ups; return true; }
        bool physicallyHeld(KeyId) const override { return false; }
    } keys;
    struct Mouse : MouseSink {
        bool sendMouse(const MouseEvent&) override { return true; }
        bool physicalButton(std::uint8_t) const override { return false; }
    } mouse;
    asio::io_context reserveIo;
    asio::ip::tcp::acceptor reserve(reserveIo, {asio::ip::address_v4::loopback(), 0});
    const auto port = reserve.local_endpoint().port(); reserve.close();
    auto agent = std::make_unique<AgentService>(ids.directory / "agent", security::fingerprint(ids.directory / "host/identity.crt"), "127.0.0.1", port, keys, mouse);
    HostEngine host(ids.directory / "host");
    host.connect({"desktop", "127.0.0.1", security::fingerprint(ids.directory / "agent/identity.crt"), port});
    auto until = [](const auto& predicate) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (!predicate() && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds(2));
        return predicate();
    };
    ASSERT_TRUE(until([&] { const auto status = host.status(); return status.devices.size() == 1 && status.devices[0].online; }));
    host.controlDevice("desktop");
    ASSERT_TRUE(until([&] { return host.status().mode == ControlMode::Remote; }));
    host.captureKey({{29, 0xA2, false}, true, false, 1});
    ASSERT_TRUE(until([&] { return keys.downs.load() == 1; }));
    EXPECT_TRUE(agent->status().connected);
    agent.reset();
    EXPECT_EQ(keys.ups.load(), 1U);
    ASSERT_TRUE(until([&] { return host.status().mode == ControlMode::Local; }));
}
TEST(Tls, PairedPeersDeliverKeyboardAndDisconnect) {
    Identities ids;
    asio::io_context io;
    asio::ssl::context hostTls(asio::ssl::context::tls), agentTls(asio::ssl::context::tls);
    security::loadIdentity(hostTls, ids.directory / "host"); security::loadIdentity(agentTls, ids.directory / "agent");
    bool received = false, hostReady = false, agentReady = false;
    auto agent = std::make_shared<Peer>(io, agentTls, security::fingerprint(ids.directory / "host/identity.crt"),
        [&](const protocol::Frame& frame) { EXPECT_EQ(protocol::decodeKey(frame).key.scan, 30); received = true; io.stop(); },
        [&](bool ready, const std::string&) { agentReady = ready; });
    std::shared_ptr<Peer> host;
    host = std::make_shared<Peer>(io, hostTls, security::fingerprint(ids.directory / "agent/identity.crt"), [](const auto&) {},
        [&](bool ready, const std::string&) { hostReady = ready; if (ready) host->send(protocol::Type::KeyDown, protocol::encodeKey({{30, 65, false}, true})); });
    asio::ip::tcp::acceptor listener(io, {asio::ip::address_v4::loopback(), 0});
    listener.async_accept(agent->socket(), [&](const auto& error) { ASSERT_FALSE(error); agent->start(true); });
    host->socket().async_connect(listener.local_endpoint(), [&](const auto& error) { ASSERT_FALSE(error); host->start(false); });
    asio::steady_timer deadline(io, std::chrono::seconds(3));
    deadline.async_wait([&](const auto& error) { if (!error) io.stop(); });
    io.run();
    EXPECT_TRUE(hostReady); EXPECT_TRUE(agentReady); EXPECT_TRUE(received);
    host->close("test_shutdown"); agent->close("test_shutdown");
}
TEST(Tls, UnpairedCertificateNeverAuthorizesInput) {
    Identities ids;
    asio::io_context io;
    asio::ssl::context hostTls(asio::ssl::context::tls), agentTls(asio::ssl::context::tls);
    security::loadIdentity(hostTls, ids.directory / "host"); security::loadIdentity(agentTls, ids.directory / "agent");
    bool authorized = false, rejected = false;
    auto agent = std::make_shared<Peer>(io, agentTls, std::string(64, '0'), [&](const auto&) { FAIL(); },
        [&](bool ready, const auto&) { authorized |= ready; if (!ready) { rejected = true; io.stop(); } });
    auto host = std::make_shared<Peer>(io, hostTls, security::fingerprint(ids.directory / "agent/identity.crt"), [](const auto&) {}, [](bool, const auto&) {});
    asio::ip::tcp::acceptor listener(io, {asio::ip::address_v4::loopback(), 0});
    listener.async_accept(agent->socket(), [&](const auto& error) { ASSERT_FALSE(error); agent->start(true); });
    host->socket().async_connect(listener.local_endpoint(), [&](const auto& error) { ASSERT_FALSE(error); host->start(false); });
    asio::steady_timer deadline(io, std::chrono::seconds(3));
    deadline.async_wait([&](const auto& error) { if (!error) io.stop(); });
    io.run(); EXPECT_FALSE(authorized); EXPECT_TRUE(rejected);
    host->close("test_shutdown"); agent->close("test_shutdown");
}
namespace {
// Raw TLS client deliberately sends invalid application traffic after presenting
// the correctly paired certificate. This tests session authorization separately
// from certificate authentication.
std::string rawScenario(const std::function<void(asio::ssl::stream<asio::ip::tcp::socket>&, const protocol::Frame&)>& scenario) {
    Identities ids;
    asio::io_context serverIo, clientIo;
    asio::ssl::context serverTls(asio::ssl::context::tls), clientTls(asio::ssl::context::tls);
    security::loadIdentity(serverTls, ids.directory / "agent"); security::loadIdentity(clientTls, ids.directory / "host");
    std::promise<std::string> closed; auto result = closed.get_future();
    auto server = std::make_shared<Peer>(serverIo, serverTls, security::fingerprint(ids.directory / "host/identity.crt"), [](const auto&) {},
        [&](bool ready, const std::string& reason) { if (!ready) closed.set_value(reason); });
    asio::ip::tcp::acceptor listener(serverIo, {asio::ip::address_v4::loopback(), 0});
    listener.async_accept(server->socket(), [server](const auto& error) { if (!error) server->start(true); });
    std::jthread thread([&] { serverIo.run(); });
    // Stop/join on every path, including failed synchronous handshake.
    struct Stop { asio::io_context& io; std::jthread& thread; ~Stop() { io.stop(); thread.join(); } } stop{serverIo, thread};
    asio::ssl::stream<asio::ip::tcp::socket> client(clientIo, clientTls);
    client.next_layer().connect(listener.local_endpoint());
    client.handshake(asio::ssl::stream_base::client);
    std::array<std::uint8_t, 4> prefix{};
    asio::read(client, asio::buffer(prefix));
    std::vector<std::uint8_t> body(protocol::bodyLength(prefix));
    asio::read(client, asio::buffer(body));
    scenario(client, protocol::decode(body));
    if (result.wait_for(std::chrono::seconds(6)) != std::future_status::ready) return "test_deadline";
    return result.get();
}
void authenticate(asio::ssl::stream<asio::ip::tcp::socket>& client, const protocol::Frame& hello) {
    asio::write(client, asio::buffer(protocol::encode({protocol::Type::Authenticate, hello.session, 1, {}})));
    std::array<std::uint8_t, 4> prefix{}; asio::read(client, asio::buffer(prefix));
    std::vector<std::uint8_t> body(protocol::bodyLength(prefix)); asio::read(client, asio::buffer(body));
    EXPECT_EQ(protocol::decode(body).type, protocol::Type::Authenticate);
}
}
TEST(Tls, InputBeforeSessionAuthenticationIsRejected) {
    EXPECT_EQ(rawScenario([](auto& client, const auto& hello) {
        asio::write(client, asio::buffer(protocol::encode({protocol::Type::KeyDown, hello.session, 1, protocol::encodeKey({{30, 65, false}, true})})));
    }), "unauthorized_command");
}
TEST(Tls, OldSessionAndDuplicateSequenceAreRejected) {
    EXPECT_EQ(rawScenario([](auto& client, const auto& hello) {
        authenticate(client, hello);
        asio::write(client, asio::buffer(protocol::encode({protocol::Type::Heartbeat, hello.session ^ 0x8000000000000000ULL, 2, {}})));
    }), "stale_session");
    EXPECT_EQ(rawScenario([](auto& client, const auto& hello) {
        authenticate(client, hello);
        asio::write(client, asio::buffer(protocol::encode({protocol::Type::Heartbeat, hello.session, 1, {}})));
    }), "rate_or_sequence_violation");
}
TEST(Tls, IncompleteFrameTimesOutAndOversizePrefixIsRejected) {
    EXPECT_EQ(rawScenario([](auto& client, const auto& hello) {
        authenticate(client, hello);
        const std::array<std::uint8_t, 5> partial{0, 0, 0, 20, 0};
        asio::write(client, asio::buffer(partial));
    }), "heartbeat_timeout");
    EXPECT_EQ(rawScenario([](auto& client, const auto&) {
        const std::array<std::uint8_t, 4> prefix{255, 255, 255, 255};
        asio::write(client, asio::buffer(prefix));
    }), "invalid_length");
}
TEST(Tls, ExcessiveInputRateClosesTheSession) {
    EXPECT_EQ(rawScenario([](auto& client, const auto& hello) {
        authenticate(client, hello);
        std::vector<std::uint8_t> flood;
        for (std::uint64_t sequence = 2; sequence < 2100; ++sequence) {
            const auto bytes = protocol::encode({protocol::Type::Heartbeat, hello.session, sequence, {}});
            flood.insert(flood.end(), bytes.begin(), bytes.end());
        }
        asio::error_code error; asio::write(client, asio::buffer(flood), error);
    }), "rate_or_sequence_violation");
}
TEST(Tls, OutboundQueueOverflowFailsClosed) {
    Identities ids;
    asio::io_context io;
    asio::ssl::context hostTls(asio::ssl::context::tls), agentTls(asio::ssl::context::tls);
    security::loadIdentity(hostTls, ids.directory / "host"); security::loadIdentity(agentTls, ids.directory / "agent");
    auto agent = std::make_shared<Peer>(io, agentTls, security::fingerprint(ids.directory / "host/identity.crt"), [](const auto&) {}, [](bool, const auto&) {});
    std::string reason; bool rejected{}; std::shared_ptr<Peer> host;
    host = std::make_shared<Peer>(io, hostTls, security::fingerprint(ids.directory / "agent/identity.crt"), [](const auto&) {},
        [&](bool ready, const std::string& status) {
            if (ready) {
                for (unsigned i = 0; i < 1025; ++i) if (!host->send(protocol::Type::ReleaseAllKeys)) rejected = true;
            } else { reason = status; io.stop(); }
        });
    asio::ip::tcp::acceptor listener(io, {asio::ip::address_v4::loopback(), 0});
    listener.async_accept(agent->socket(), [&](const auto& error) { ASSERT_FALSE(error); agent->start(true); });
    host->socket().async_connect(listener.local_endpoint(), [&](const auto& error) { ASSERT_FALSE(error); host->start(false); });
    asio::steady_timer deadline(io, std::chrono::seconds(3)); deadline.async_wait([&](const auto&) { io.stop(); });
    io.run(); EXPECT_TRUE(rejected); EXPECT_EQ(reason, "outbound_queue_limit");
    host->close("test_shutdown"); agent->close("test_shutdown");
}
TEST(Tls, FiveAgentsReceiveMirrorEventsAndEmergencyCleanup) {
    Identities ids;
    asio::io_context io;
    asio::ssl::context tls(asio::ssl::context::tls); security::loadIdentity(tls, ids.directory / "agent");
    std::array<std::atomic_uint, 5> downs{}, ups{}, releases{}, buttonUps{};
    std::array<std::vector<protocol::Type>, 5> order;
    std::vector<std::shared_ptr<Peer>> agents;
    std::vector<std::unique_ptr<asio::ip::tcp::acceptor>> listeners;
    for (std::size_t i = 0; i < 5; ++i) {
        auto peer = std::make_shared<Peer>(io, tls, security::fingerprint(ids.directory / "host/identity.crt"), [&, i](const protocol::Frame& frame) {
            if (frame.type == protocol::Type::KeyDown || frame.type == protocol::Type::KeyUp || frame.type == protocol::Type::MouseButton) order[i].push_back(frame.type);
            if (frame.type == protocol::Type::KeyDown) ++downs[i];
            if (frame.type == protocol::Type::KeyUp) ++ups[i];
            if (frame.type == protocol::Type::MouseButton && frame.payload[1] == 0) ++buttonUps[i];
            if (frame.type == protocol::Type::ReleaseAllKeys) ++releases[i];
        }, [](bool, const auto&) {});
        auto listener = std::make_unique<asio::ip::tcp::acceptor>(io, asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), 0));
        listener->async_accept(peer->socket(), [peer](const auto& error) { if (!error) peer->start(true); });
        agents.push_back(peer); listeners.push_back(std::move(listener));
    }
    std::jthread thread([&] { io.run(); });
    struct Stop { asio::io_context& io; std::jthread& thread; ~Stop() { io.stop(); thread.join(); } } stop{io, thread};
    HostEngine host(ids.directory / "host");
    std::vector<DeviceId> targets;
    for (std::size_t i = 0; i < 5; ++i) {
        const auto id = std::to_string(i); targets.push_back(id);
        host.connect({id, "127.0.0.1", security::fingerprint(ids.directory / "agent/identity.crt"), listeners[i]->local_endpoint().port()});
    }
    auto until = [](const auto& predicate) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(4);
        while (!predicate() && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds(2));
        return predicate();
    };
    ASSERT_TRUE(until([&] { const auto state = host.status(); return state.devices.size() == 5 && std::all_of(state.devices.begin(), state.devices.end(), [](const auto& device) { return device.online; }); }));
    ASSERT_TRUE(until([&] { const auto state = host.status(); return std::all_of(state.devices.begin(), state.devices.end(), [](const auto& device) { return device.roundTripMs.has_value() && *device.roundTripMs >= 0; }); }));
    host.setTargets(targets); host.setMouseMirroring(true); host.setMode(ControlMode::Mirror);
    ASSERT_TRUE(until([&] { return host.status().mode == ControlMode::Mirror; }));
    EXPECT_FALSE(host.captureKey({{30, 65, false}, true, false, 1}));
    EXPECT_FALSE(host.captureKey({{30, 65, false}, false, false, 2}));
    ASSERT_TRUE(until([&] { return std::all_of(ups.begin(), ups.end(), [](const auto& count) { return count.load() == 1; }); }));
    host.captureKey({{29, 0xA2, false}, true, false, 3});
    host.captureMouse({MouseKind::Button, 0, 0, 0, true});
    host.captureKey({{29, 0xA2, false}, false, false, 4});
    host.captureMouse({MouseKind::Button, 0, 0, 0, false});
    ASSERT_TRUE(until([&] { return std::all_of(buttonUps.begin(), buttonUps.end(), [](const auto& count) { return count.load() == 1; }); }));
    const std::vector<protocol::Type> expected{protocol::Type::KeyDown, protocol::Type::KeyUp, protocol::Type::KeyDown, protocol::Type::MouseButton, protocol::Type::KeyUp, protocol::Type::MouseButton};
    for (const auto& observed : order) EXPECT_EQ(observed, expected);
    host.emergencyStop();
    ASSERT_TRUE(until([&] { return std::all_of(releases.begin(), releases.end(), [](const auto& count) { return count.load() > 0; }); }));
    EXPECT_EQ(host.status().mode, ControlMode::Local);
    for (const auto& count : downs) EXPECT_EQ(count.load(), 2U);
    // Restart one agent listener and exercise the real host reconnect timer.
    asio::post(io, [&] {
        agents[0]->close("restart_test");
        auto restarted = std::make_shared<Peer>(io, tls, security::fingerprint(ids.directory / "host/identity.crt"), [](const auto&) {}, [](bool, const auto&) {});
        agents[0] = restarted;
        listeners[0]->async_accept(restarted->socket(), [restarted](const auto& error) { if (!error) restarted->start(true); });
    });
    ASSERT_TRUE(until([&] { const auto state = host.status(); return state.devices[0].online && state.devices[0].retries > 0; }));
    EXPECT_EQ(host.status().mode, ControlMode::Local);
}
