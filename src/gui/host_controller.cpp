#include "sharekeys/host_controller.hpp"
#include <QTimer>

namespace sharekeys {
HostController::HostController(HostEngine& engine, QObject* parent)
    : QObject(parent), engine_(engine) {
    auto* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this] { emit statusChanged(engine_.status()); });
    timer->start(100);
}
}
