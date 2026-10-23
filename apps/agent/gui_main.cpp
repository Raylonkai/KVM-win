#include "sharekeys/agent_window.hpp"
#include "sharekeys/security.hpp"
#ifdef _WIN32
#include "sharekeys/win32_keyboard.hpp"
#include "sharekeys/win32_mouse.hpp"
#endif
#include <QApplication>
#include <QCommandLineParser>
#include <QLockFile>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTimer>
#include <iostream>

int main(int argc, char** argv) {
    QApplication app(argc, argv); QApplication::setApplicationName("ShareKeysAgent");
    QCommandLineParser parser; parser.addHelpOption();
    parser.addOption({"identity", "Identity and configuration directory", "directory", QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)});
    parser.addOption({"background", "Open in the notification area after setup"});
    parser.addOption({"smoke-test", "Preview setup without input hooks or listening"}); parser.process(app);
    try {
        const auto directory = parser.value("identity");
        const auto path = sharekeys::nativePath(directory);
        std::filesystem::create_directories(path);
        QLockFile lock(directory + "/agent.lock"); lock.setStaleLockTime(0);
        if (!lock.tryLock()) throw std::runtime_error("ShareKeys Agent is already running. Open it from the notification area.");
        if (!std::filesystem::exists(path / "identity.crt")) sharekeys::security::createIdentity(path);
        sharekeys::AgentWindow::Start start;
#ifdef _WIN32
        std::unique_ptr<sharekeys::KeyboardHook> keyboardHook;
        std::unique_ptr<sharekeys::MouseHook> mouseHook;
        std::unique_ptr<sharekeys::Win32KeyboardSink> keySink;
        std::unique_ptr<sharekeys::Win32MouseSink> mouseSink;
        if (!parser.isSet("smoke-test")) {
            keyboardHook = std::make_unique<sharekeys::KeyboardHook>([](const auto&) { return false; });
            mouseHook = std::make_unique<sharekeys::MouseHook>([](const auto&) { return false; });
            keySink = std::make_unique<sharekeys::Win32KeyboardSink>(*keyboardHook);
            mouseSink = std::make_unique<sharekeys::Win32MouseSink>(*mouseHook);
            start = [&](const sharekeys::AgentSettings& settings) {
                return std::make_unique<sharekeys::AgentService>(path, settings.hostPin.toStdString(), settings.address.toStdString(), static_cast<std::uint16_t>(settings.port), *keySink, *mouseSink, sharekeys::cursorEdge);
            };
        }
#endif
        sharekeys::AgentWindow window(directory, std::move(start), parser.isSet("smoke-test"));
        QObject::connect(&app, &QGuiApplication::commitDataRequest, &app, [&window](QSessionManager&) { window.close(); QApplication::quit(); }, Qt::DirectConnection);
        if (!parser.isSet("background") || !window.running() || !window.hasTray()) window.show();
        if (parser.isSet("smoke-test")) QTimer::singleShot(250, &app, &QApplication::quit);
        return app.exec();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        if (!parser.isSet("smoke-test")) QMessageBox::critical(nullptr, "ShareKeys Agent", error.what());
        return 1;
    }
}
