#pragma once
#include <QString>
#include <filesystem>
namespace sharekeys {
std::filesystem::path nativePath(const QString& value);
struct AgentSettings {
    QString hostPin, address;
    int port{24870};
    bool autoListen{};
};
AgentSettings loadAgentSettings(const QString& directory);
void saveAgentSettings(const QString& directory, const AgentSettings& settings);
bool loginStartupEnabled(const QString& name);
void setLoginStartup(const QString& name, bool enabled);
}
