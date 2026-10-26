#include "sharekeys/host_window.hpp"
#include "sharekeys/security.hpp"
#include "sharekeys/desktop_application.hpp"
#ifdef _WIN32
#include "sharekeys/win32_keyboard.hpp"
#include "sharekeys/win32_mouse.hpp"
#endif
#include <QApplication>
#include <QCommandLineParser>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTimer>
#include <QLockFile>
#include <iostream>

int main(int argc, char** argv) {
    QApplication app(argc, argv); QApplication::setApplicationName("ShareKeysHost");
    sharekeys::initializeDesktopApplication();
    QCommandLineParser parser; parser.addHelpOption();
    parser.addOption({"identity", "Identity and configuration directory", "directory", QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)});
    parser.addOption({"smoke-test", "Open the GUI for 250 ms without input hooks"});
    parser.addOption({"background", "Open in the notification area"}); parser.process(app);
    try {
        const auto directory = parser.value("identity");
#ifdef _WIN32
        const std::filesystem::path path(directory.toStdWString());
#else
        const std::filesystem::path path(directory.toStdString());
#endif
        std::filesystem::create_directories(path);
        QLockFile lock(directory + "/host.lock"); lock.setStaleLockTime(0);
        if (!lock.tryLock()) throw std::runtime_error("ShareKeys Host is already running. Open it from the notification area.");
        if (!std::filesystem::exists(path / "identity.crt")) sharekeys::security::createIdentity(path);
#ifdef _WIN32
        sharekeys::HostEngine engine(path, sharekeys::centerPointer);
#else
        sharekeys::HostEngine engine(path);
#endif
#ifdef _WIN32
        const bool inputAvailable = !parser.isSet("smoke-test");
#else
        const bool inputAvailable = false;
#endif
        sharekeys::HostWindow window(engine, directory, QString::fromStdString(sharekeys::security::fingerprint(path / "identity.crt")), parser.isSet("smoke-test"), inputAvailable);
#ifdef _WIN32
        std::unique_ptr<sharekeys::KeyboardHook> keyboard;
        std::unique_ptr<sharekeys::MouseHook> mouse;
        if (!parser.isSet("smoke-test")) {
            keyboard = std::make_unique<sharekeys::KeyboardHook>([&](const auto& event) { return engine.captureKey(event); });
            mouse = std::make_unique<sharekeys::MouseHook>([&](const auto& event) { return engine.captureMouse(event); });
        }
#endif
        QObject::connect(&app, &QGuiApplication::commitDataRequest, &app, [&engine](QSessionManager&) { engine.emergencyStop(); QApplication::quit(); }, Qt::DirectConnection);
        if (!parser.isSet("background") || !window.hasTray()) window.show();
        if (parser.isSet("smoke-test")) QTimer::singleShot(250, &app, &QApplication::quit);
        return app.exec();
    } catch (const std::exception& error) {
        std::cerr << "ShareKeys Host: " << error.what() << '\n';
        if (!parser.isSet("smoke-test")) QMessageBox::critical(nullptr, "ShareKeys", error.what());
        return 1;
    }
}
