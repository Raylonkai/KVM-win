#include "sharekeys/desktop_settings.hpp"
#include "sharekeys/network.hpp"
#include "sharekeys/security.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSettings>

namespace sharekeys {
DesktopPreferences loadDesktopPreferences(const QString& directory) {
    QFile file(directory + "/desktop.json");
    if (!file.exists()) return {};
    if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error("Cannot read desktop preferences.");
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    const auto json = document.object();
    if (error.error != QJsonParseError::NoError || !document.isObject()
        || !json.value("closeToTray").isBool() || !json.value("minimizeToTray").isBool())
        throw std::runtime_error("Invalid desktop preferences.");
    return {json.value("closeToTray").toBool(), json.value("minimizeToTray").toBool()};
}
void saveDesktopPreferences(const QString& directory, const DesktopPreferences& preferences) {
    if (!QDir().mkpath(directory)) throw std::runtime_error("Cannot create preferences directory.");
    const auto bytes = QJsonDocument(QJsonObject{{"closeToTray", preferences.closeToTray},
        {"minimizeToTray", preferences.minimizeToTray}}).toJson();
    QSaveFile file(directory + "/desktop.json");
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
        throw std::runtime_error("Cannot save desktop preferences.");
}
std::filesystem::path nativePath(const QString& value) {
#ifdef _WIN32
    return std::filesystem::path(value.toStdWString());
#else
    return std::filesystem::path(value.toStdString());
#endif
}
AgentSettings loadAgentSettings(const QString& directory) {
    QFile file(directory + "/authorized-host.json");
    if (!file.exists()) return {};
    if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error("Cannot read saved agent settings.");
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) throw std::runtime_error("Invalid agent settings file.");
    const auto json = document.object();
    AgentSettings settings{json.value("hostPin").toString(), json.value("address").toString(), json.value("port").toInt(24870), json.value("autoListen").toBool(false)};
    // Legacy CLI pairing files have only hostPin; they remain usable in the GUI.
    if ((!settings.hostPin.isEmpty() && !security::validPin(settings.hostPin.toStdString())) || settings.port < 1 || settings.port > 65535)
        throw std::runtime_error("Invalid saved pairing or port. Correct the settings file before listening.");
    return settings;
}
void saveAgentSettings(const QString& directory, const AgentSettings& settings) {
    if (settings.port < 1 || settings.port > 65535 || (!settings.hostPin.isEmpty() && !security::validPin(settings.hostPin.toStdString())))
        throw std::invalid_argument("Enter the verified 64-character host fingerprint and a valid port.");
    if (settings.autoListen) {
        asio::error_code error;
        const auto address = asio::ip::make_address(settings.address.toStdString(), error);
        if (settings.hostPin.isEmpty() || error || !address.is_v4() || !privateAddress(address)) throw std::invalid_argument("Choose a private IPv4 address and pair a host before enabling automatic listening.");
    }
    if (!QDir().mkpath(directory)) throw std::runtime_error("Cannot create settings directory.");
    const auto bytes = QJsonDocument(QJsonObject{{"hostPin", settings.hostPin}, {"address", settings.address}, {"port", settings.port}, {"autoListen", settings.autoListen}}).toJson();
    QSaveFile file(directory + "/authorized-host.json");
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) throw std::runtime_error("Cannot save agent settings.");
}
bool loginStartupEnabled(const QString& name) {
#ifdef _WIN32
    QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    return settings.contains(name);
#else
    (void)name; return false;
#endif
}
void setLoginStartup(const QString& name, bool enabled) {
#ifdef _WIN32
    QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    if (enabled) settings.setValue(name, '"' + QDir::toNativeSeparators(QCoreApplication::applicationFilePath()) + "\" --background");
    else settings.remove(name);
    settings.sync();
    if (settings.status() != QSettings::NoError) throw std::runtime_error("Could not update Windows login startup.");
#else
    (void)name; (void)enabled;
    throw std::runtime_error("Login startup is available in the Windows build.");
#endif
}
}
