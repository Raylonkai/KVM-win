#include "sharekeys/host_engine.hpp"
#include "sharekeys/security.hpp"

namespace sharekeys {
HostEngine::HostEngine(const std::filesystem::path& identity, std::function<bool()> centerLocal)
    : router_([this](const auto& id, auto type, const auto& payload) {
        const auto it = slots_.find(id);
        return it != slots_.end() && it->second->peer && it->second->peer->send(type, payload);
    }), centerLocal_(std::move(centerLocal)) {
    security::loadIdentity(tls_, identity);
    pump();
    thread_ = std::thread([this] { io_.run(); });
}
HostEngine::~HostEngine() {
    exclusive_.store(false);
    asio::post(io_, [this] {
        router_.emergencyStop();
        pumpTimer_.cancel();
        for (auto& [id, slot] : slots_) {
            (void)id; slot->enabled = false; slot->timer.cancel();
            if (slot->peer) slot->peer->close("host_shutdown");
        }
        io_.stop();
    });
    if (thread_.joinable()) thread_.join();
}
void HostEngine::connect(DeviceConfig device) {
    asio::error_code error;
    const auto address = asio::ip::make_address(device.address, error);
    if (error || !privateAddress(address) || !security::validPin(device.pin) || device.id.empty() || device.port == 0) throw std::invalid_argument("device needs a name, private LAN IP, port and lowercase SHA-256 fingerprint");
    asio::post(io_, [this, device = std::move(device)] {
        auto it = slots_.find(device.id);
        if (it == slots_.end()) {
            if (slots_.size() >= max_devices) { publish("five_device_limit"); return; }
            it = slots_.emplace(device.id, std::make_unique<Slot>(io_)).first;
        }
        auto& slot = *it->second;
        ++slot.generation;
        if (slot.peer) slot.peer->close("reconfigured");
        router_.setOnline(device.id, false);
        slot.timer.cancel(); slot.view = {device, false, 0, "connecting", {}}; slot.enabled = true;
        slot.backoff.reset(); connectNow(device.id);
    });
}
void HostEngine::connectNow(const DeviceId& id) {
    auto& slot = *slots_.at(id);
    const auto generation = ++slot.generation;
    slot.view.status = "connecting";
    slot.peer = std::make_shared<Peer>(io_, tls_, slot.view.config.pin,
        [this, id](const protocol::Frame& frame) {
            if (frame.type != protocol::Type::DeviceStatus && frame.type != protocol::Type::Error) {
                slots_.at(id)->peer->close("unexpected_agent_command");
            } else if (frame.type == protocol::Type::Error) { emergencyStop(); publish("agent_input_error"); }
            else if (frame.payload.size() == 2 && frame.payload[0] == 1 && frame.payload[1] <= 4 && router_.mode() == ControlMode::Remote && router_.activeDevice() == id) {
                switchEdge(id, static_cast<Edge>(frame.payload[1]));
            }
        }, [this, id, generation](bool ready, const auto& reason) {
            // Defer state changes so a failed send cannot reenter router cleanup.
            asio::post(io_, [this, id, generation, ready, reason] { updateStatus(id, generation, ready, reason); });
        });
    const auto peer = slot.peer;
    slot.timer.expires_after(std::chrono::seconds(5));
    slot.timer.async_wait([peer](const auto& error) { if (!error) peer->close("connect_timeout"); });
    peer->socket().async_connect({asio::ip::make_address(slot.view.config.address), slot.view.config.port},
        [this, id, generation, peer](const auto& error) {
            const auto it = slots_.find(id);
            if (it == slots_.end() || it->second->generation != generation) return;
            it->second->timer.cancel();
            if (error) peer->close("connect_failed"); else peer->start(false);
        });
    publish();
}
void HostEngine::updateStatus(const DeviceId& id, std::uint64_t generation, bool online, std::string reason) {
    const auto it = slots_.find(id);
    if (it == slots_.end() || it->second->generation != generation) return;
    auto& slot = *it->second;
    slot.view.online = online; slot.view.status = reason;
    router_.setOnline(id, online);
    if (online) slot.backoff.reset();
    else if (slot.enabled) {
        ++slot.view.retries;
        slot.timer.expires_after(slot.backoff.next());
        slot.timer.async_wait([this, id, generation](const auto& error) {
            const auto found = slots_.find(id);
            if (!error && found != slots_.end() && found->second->enabled && found->second->generation == generation) connectNow(id);
        });
    }
    publish(std::move(reason));
}
void HostEngine::disconnect(const DeviceId& id, bool remove) {
    asio::post(io_, [this, id, remove] {
        const auto it = slots_.find(id); if (it == slots_.end()) return;
        auto& slot = *it->second;
        router_.setOnline(id, false);
        slot.enabled = false; ++slot.generation; slot.timer.cancel();
        if (slot.peer) slot.peer->close("user_disconnect");
        slot.view.online = false; slot.view.status = "disconnected";
        if (remove) slots_.erase(it);
        publish("disconnected");
    });
}
void HostEngine::setMode(ControlMode mode) {
    asio::post(io_, [this, mode] {
        if (mode == ControlMode::Remote) { enterRemote(router_.activeDevice()); return; }
        const bool ok = router_.setMode(mode); publish(ok ? "mode_changed" : "select_connected_target_first");
    });
}
void HostEngine::setActiveDevice(DeviceId id) {
    asio::post(io_, [this, id = std::move(id)] { router_.setActiveDevice(id); publish(); });
}
void HostEngine::controlDevice(DeviceId id) { asio::post(io_, [this, id = std::move(id)] { enterRemote(id); }); }
void HostEngine::setTargets(std::vector<DeviceId> ids) {
    asio::post(io_, [this, ids = std::move(ids)] { router_.setMirrorTargets(ids); publish(); });
}
void HostEngine::emergencyStop() { inputEpoch_.fetch_add(1); exclusive_.store(false); emergency_.store(true); }
void HostEngine::setMouseMirroring(bool enabled) {
    asio::post(io_, [this, enabled] { router_.setMouseMirroring(enabled); });
}
void HostEngine::configureLayout(std::map<DeviceId, GridPosition> positions, bool enabled) {
    Workspace candidate;
    if (!candidate.configure(positions)) throw std::invalid_argument("layout requires distinct grid cells; host occupies (0,0)");
    asio::post(io_, [this, candidate = std::move(candidate), enabled] { router_.emergencyStop(); workspace_ = candidate; edgesEnabled_ = enabled; publish(); });
}
void HostEngine::enterRemote(const DeviceId& id) {
    const auto target = id; // setActiveDevice may update the referenced string.
    if (!router_.setActiveDevice(target) || !router_.setMode(ControlMode::Remote)) { router_.emergencyStop(); publish("select_connected_target_first"); return; }
    if (centerLocal_ && !centerLocal_()) { router_.emergencyStop(); publish("pointer_center_failed"); return; }
    const auto it = slots_.find(target);
    if (it == slots_.end() || !it->second->peer || !it->second->peer->send(protocol::Type::MouseMove, encodeMouse({MouseKind::Move, 32768, 32768, 0, false, true}))) router_.emergencyStop();
    lastSwitch_ = std::chrono::steady_clock::now(); publish();
}
void HostEngine::switchEdge(const DeviceId& source, Edge edge) {
    if (!edgesEnabled_ || router_.mode() == ControlMode::Mirror || std::chrono::steady_clock::now() - lastSwitch_ < std::chrono::milliseconds(750)) return;
    const auto next = workspace_.neighbor(source, edge); if (!next) return;
    if (next->empty()) {
        router_.emergencyStop(); if (centerLocal_) centerLocal_(); lastSwitch_ = std::chrono::steady_clock::now(); publish("edge_local_control");
    } else {
        const auto it = slots_.find(*next); if (it != slots_.end() && it->second->view.online) enterRemote(*next);
    }
}
bool HostEngine::captureMouse(const MouseEvent& event) {
    const auto epoch = inputEpoch_.load();
    if (mouseEpoch_ != epoch) { mouseQuarantine_ = mouseHeld_; mouseEpoch_ = epoch; }
    bool suppress = exclusive_.load();
    if (event.kind == MouseKind::Button && event.button < mouseHeld_.size()) {
        if (event.down && mouseQuarantine_[event.button]) return mouseSuppressed_[event.button];
        if (!event.down) mouseQuarantine_[event.button] = false;
        if (event.down && !mouseHeld_[event.button]) mouseSuppressed_[event.button] = suppress;
        suppress = mouseSuppressed_[event.button]; mouseHeld_[event.button] = event.down;
        if (!event.down) mouseSuppressed_[event.button] = false;
    }
    if (!inputs_.push({epoch, true, {}, event})) emergencyStop();
    return suppress;
}
void HostEngine::configureHotkeys(std::uint8_t mirror, std::uint8_t emergency, std::uint8_t next) {
    if (mirror == 0 || emergency == 0 || next == 0 || mirror == emergency || mirror == next || emergency == next) throw std::invalid_argument("hotkeys must be distinct nonzero virtual keys");
    shortcuts_.store(mirror | (static_cast<std::uint32_t>(emergency) << 8) | (static_cast<std::uint32_t>(next) << 16));
}
bool HostEngine::captureKey(const KeyboardEvent& event) {
    const auto epoch = inputEpoch_.load();
    if (keyEpoch_ != epoch) { gate_.quarantineHeld(); keyEpoch_ = epoch; }
    const auto keys = shortcuts_.load(std::memory_order_relaxed);
    gate_.setKeys(static_cast<std::uint16_t>(keys & 255), static_cast<std::uint16_t>((keys >> 8) & 255), static_cast<std::uint16_t>((keys >> 16) & 255));
    auto result = gate_.process(event, exclusive_.load());
    if (result.captured) {
        if (result.captured->action == CaptureAction::Emergency) emergencyStop();
        else if (!inputs_.push({epoch, false, *result.captured, {}})) emergencyStop();
    }
    return result.suppress;
}
void HostEngine::nextDevice() {
    std::vector<DeviceId> online;
    for (const auto& [id, slot] : slots_) if (slot->view.online) online.push_back(id);
    if (online.empty()) return;
    const auto it = std::find(online.begin(), online.end(), router_.activeDevice());
    const auto next = it == online.end() || std::next(it) == online.end() ? online.front() : *std::next(it);
    enterRemote(next);
}
void HostEngine::pump() {
    CapturedInput input;
    if (emergency_.exchange(false)) {
        router_.emergencyStop();
        // Drain into Local state to preserve held-key quarantine without sending
        // stale input or queued mode-change commands to a remote.
        while (inputs_.pop(input)) {
            if (input.mouse) router_.processMouse(mouseType(input.pointer.kind), encodeMouse(input.pointer));
            else if (input.key.action == CaptureAction::Key) router_.processKeyboardEvent(input.key.event);
        }
        publish("emergency_local_control");
    }
    unsigned count{};
    while (++count <= 512 && inputs_.pop(input)) {
        // A producer may have reserved a queue slot just before an emergency and
        // published it later. Such input/commands must never reactivate routing.
        if (input.epoch != inputEpoch_.load()) continue;
        if (input.mouse) {
            const auto& mouse = input.pointer;
            if (router_.mode() == ControlMode::Local && mouse.kind == MouseKind::Move) {
                switchEdge("", static_cast<Edge>(mouse.edge));
                if (router_.mode() != ControlMode::Local) continue;
            }
            router_.processMouse(mouseType(mouse.kind), encodeMouse(mouse));
        } else {
            const auto& key = input.key;
            if (key.action == CaptureAction::Key) router_.processKeyboardEvent(key.event);
            else if (key.action == CaptureAction::ToggleMirror) router_.setMode(router_.mode() == ControlMode::Mirror ? ControlMode::Local : ControlMode::Mirror);
            else if (key.action == CaptureAction::NextDevice) nextDevice();
        }
    }
    publish();
    pumpTimer_.expires_after(std::chrono::milliseconds(1));
    pumpTimer_.async_wait([this](const auto& error) { if (!error) pump(); });
}
void HostEngine::publish(std::string event) {
    exclusive_.store(router_.mode() == ControlMode::Remote && !emergency_.load());
    std::lock_guard lock(statusMutex_);
    status_.mode = router_.mode(); status_.active = router_.activeDevice(); status_.devices.clear();
    for (const auto& [id, slot] : slots_) {
        (void)id; slot->view.roundTripMs = slot->peer && slot->view.online ? slot->peer->roundTripMs() : std::nullopt;
        status_.devices.push_back(slot->view);
    }
    if (!event.empty()) status_.lastEvent = std::move(event);
}
HostStatus HostEngine::status() const { std::lock_guard lock(statusMutex_); return status_; }
}
