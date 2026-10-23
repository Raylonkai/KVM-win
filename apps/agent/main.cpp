#include "sharekeys/network.hpp"
#include "sharekeys/security.hpp"
#include "sharekeys/win32_keyboard.hpp"
#include "sharekeys/win32_mouse.hpp"
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QFile>
#include <QSaveFile>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <iostream>

namespace {
std::atomic_bool stopping{};
BOOL WINAPI controlHandler(DWORD) { stopping.store(true); return TRUE; }
}
int main(int argc, char** argv) {
    using namespace sharekeys;
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("ShareKeysAgent");
    QCommandLineParser parser;
    parser.setApplicationDescription("ShareKeys interactive-session agent. Pair fingerprints before listening.");
    parser.addHelpOption();
    parser.addOptions({
        {{"i", "identity"}, "Identity/config directory", "directory", QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)},
        {"init", "Create an identity and print its fingerprint"},
        {"pair", "Authorize exactly this host certificate SHA-256 fingerprint", "fingerprint"},
        {"revoke", "Remove the authorized host (stop a running agent first)"},
        {"listen", "Bind this private LAN IPv4 address", "address"},
        {"port", "Listen port", "port", "24870"},
        {"self-test", "Explicitly inject F24 down/up on the current desktop"}});
    parser.process(app);
    try {
        const std::filesystem::path directory(parser.value("identity").toStdWString());
        if (parser.isSet("init")) {
            security::createIdentity(directory);
            std::cout << "Agent SHA-256: " << security::fingerprint(directory / "identity.crt") << '\n'; return 0;
        }
        const QString configPath = parser.value("identity") + "/authorized-host.json";
        if (parser.isSet("pair") || parser.isSet("revoke")) {
            const auto pin = parser.value("pair").toStdString();
            if (!parser.isSet("revoke") && !security::validPin(pin)) throw std::runtime_error("invalid host fingerprint");
            QSaveFile file(configPath);
            const auto data = QJsonDocument(QJsonObject{{"hostPin", parser.isSet("revoke") ? QString{} : QString::fromStdString(pin)}}).toJson();
            if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) throw std::runtime_error("cannot save authorization");
            std::cout << "Authorization saved. Restart agent to apply it.\n"; return 0;
        }
        KeyboardHook keyboardHook([](const auto&) { return false; });
        MouseHook mouseHook([](const auto&) { return false; });
        Win32KeyboardSink keySink(keyboardHook); InjectedKeyboard keyboard(keySink);
        Win32MouseSink mouseSink(mouseHook); InjectedMouse mouse(mouseSink);
        if (parser.isSet("self-test")) {
            if (!keyboard.apply({{0, VK_F24, false}, true, true, 1}) || !keyboard.releaseAll()) throw std::runtime_error("SendInput rejected F24");
            std::cout << "SendInput accepted F24 down/up; foreground application action is unverified.\n"; return 0;
        }
        QFile configFile(configPath);
        if (!configFile.open(QIODevice::ReadOnly)) throw std::runtime_error("pair a host first");
        const auto pin = QJsonDocument::fromJson(configFile.readAll()).object().value("hostPin").toString().toStdString();
        if (!security::validPin(pin)) throw std::runtime_error("no authorized host");
        asio::error_code error;
        const auto address = asio::ip::make_address(parser.value("listen").toStdString(), error);
        bool portOk{}; const auto port = parser.value("port").toUInt(&portOk);
        if (error || !address.is_v4() || !privateAddress(address) || !portOk || port == 0 || port > 65535) throw std::runtime_error("--listen requires an explicit private IPv4 address and valid port");
        asio::io_context io;
        asio::ssl::context tls(asio::ssl::context::tls); security::loadIdentity(tls, directory);
        asio::ip::tcp::acceptor listener(io, {address, static_cast<std::uint16_t>(port)});
        std::shared_ptr<Peer> peer;
        bool connected{};
        auto release = [&] { const bool keys = keyboard.releaseAll(); const bool buttons = mouse.releaseAll(); return keys && buttons; };
        std::function<void()> accept;
        accept = [&] {
            if (stopping.load()) return;
            peer = std::make_shared<Peer>(io, tls, pin, [&](const protocol::Frame& frame) {
                bool ok = true;
                switch (frame.type) {
                    case protocol::Type::KeyDown: case protocol::Type::KeyUp: ok = keyboard.apply(protocol::decodeKey(frame)); break;
                    case protocol::Type::MouseMove: case protocol::Type::MouseButton: case protocol::Type::MouseWheel: ok = mouse.apply(decodeMouse(frame)); break;
                    case protocol::Type::ReleaseAllKeys: case protocol::Type::MirrorStop: case protocol::Type::MirrorStart: ok = release(); break;
                    default: peer->close("unauthorized_message_type"); return;
                }
                if (!ok) { release(); peer->close("input_injection_failed"); }
            }, [&](bool ready, const std::string& reason) {
                connected = ready;
                std::cerr << "{\"event\":\"" << reason << "\"}\n";
                if (!ready) { release(); asio::post(io, accept); }
            });
            const auto pending = peer;
            listener.async_accept(pending->socket(), [&, pending](const auto& acceptError) {
                if (acceptError) { if (!stopping.load()) pending->close("accept_failed"); return; }
                asio::error_code endpointError;
                const auto endpoint = pending->socket().remote_endpoint(endpointError);
                if (endpointError || !privateAddress(endpoint.address())) { pending->close("non_lan_peer"); return; }
                if (!release()) { pending->close("key_cleanup_failed"); return; }
                pending->start(true);
            });
        };
        if (!SetConsoleCtrlHandler(controlHandler, TRUE)) throw std::runtime_error("cannot install shutdown handler");
        asio::steady_timer watchdog(io);
        std::function<void()> tick;
        tick = [&] {
            if (stopping.load()) {
                listener.close(); if (peer) peer->close("agent_shutdown"); release(); io.stop(); return;
            }
            if (!connected) release();
            else if (peer) peer->send(protocol::Type::DeviceStatus, {1, cursorEdge()});
            watchdog.expires_after(std::chrono::milliseconds(100));
            watchdog.async_wait([&](const auto& timerError) { if (!timerError) tick(); });
        };
        std::cout << "Agent SHA-256: " << security::fingerprint(directory / "identity.crt") << "\nListening for paired host on " << parser.value("listen").toStdString() << ':' << port << '\n';
        accept(); tick(); io.run(); release();
        SetConsoleCtrlHandler(controlHandler, FALSE);
        return 0;
    } catch (const std::exception& error) { std::cerr << "ShareKeys Agent: " << error.what() << '\n'; return 1; }
}
