#pragma once
#include "sharekeys/host_engine.hpp"
#include <QObject>

namespace sharekeys {
// Qt-thread adapter. The engine keeps ownership of all network/routing work.
// Sampling its bounded snapshot never waits for network activity.
class HostController final : public QObject {
    Q_OBJECT
public:
    explicit HostController(HostEngine& engine, QObject* parent = nullptr);
    HostStatus status() const { return engine_.status(); }
    void connectDevice(DeviceConfig device) { engine_.connect(std::move(device)); }
    void disconnect(const DeviceId& id, bool remove = false) { engine_.disconnect(id, remove); }
    void setMode(ControlMode mode) { engine_.setMode(mode); }
    void controlDevice(DeviceId id) { engine_.controlDevice(std::move(id)); }
    void setTargets(std::vector<DeviceId> ids) { engine_.setTargets(std::move(ids)); }
    void emergencyStop() { engine_.emergencyStop(); }
    void setMouseMirroring(bool enabled) { engine_.setMouseMirroring(enabled); }
    void configureLayout(std::map<DeviceId, GridPosition> positions, bool enabled) { engine_.configureLayout(std::move(positions), enabled); }
    void configureHotkeys(std::uint8_t mirror, std::uint8_t emergency, std::uint8_t next) { engine_.configureHotkeys(mirror, emergency, next); }
signals:
    void statusChanged(const sharekeys::HostStatus& status);
private:
    HostEngine& engine_;
};
}
