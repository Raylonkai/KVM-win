#include "sharekeys/agent_service.hpp"
#include "sharekeys/security.hpp"

namespace sharekeys {
AgentService::AgentService(const std::filesystem::path& identity, std::string pin,
    const std::string& address, std::uint16_t port, KeyboardSink& keys, MouseSink& mouse,
    std::function<std::uint8_t()> edge)
    : keyboard_(keys), mouse_(mouse), pin_(std::move(pin)), edge_(std::move(edge)) {
    asio::error_code error;
    const auto bind = asio::ip::make_address(address, error);
    if (error || !bind.is_v4() || !privateAddress(bind) || port == 0 || !security::validPin(pin_))
        throw std::invalid_argument("Choose a private IPv4 address, a valid port and the verified host fingerprint.");
    security::loadIdentity(tls_, identity);
    listener_.open(asio::ip::tcp::v4());
    listener_.bind({bind, port});
    listener_.listen(1);
    accept(); tick();
    thread_ = std::thread([this] {
        try { io_.run(); }
        catch (const std::exception& exception) { release(); publish(false, exception.what()); }
    });
}
AgentService::~AgentService() {
    // run() may have exited on an exception, so never wait on a posted promise.
    asio::post(io_, [this] {
        stopping_ = true;
        asio::error_code ignored; listener_.close(ignored); timer_.cancel();
        if (peer_) peer_->close("agent_stopped");
        release(); io_.stop();
    });
    if (thread_.joinable()) thread_.join();
    release();
}
bool AgentService::release() {
    const bool keys = keyboard_.releaseAll(), buttons = mouse_.releaseAll();
    return keys && buttons;
}
void AgentService::publish(bool connected, std::string message) {
    std::lock_guard lock(mutex_); status_ = {connected, std::move(message)};
}
AgentStatus AgentService::status() const { std::lock_guard lock(mutex_); return status_; }
void AgentService::accept() {
    if (stopping_) return;
    peer_ = std::make_shared<Peer>(io_, tls_, pin_, [this](const protocol::Frame& frame) {
        bool ok{};
        switch (frame.type) {
            case protocol::Type::KeyDown: case protocol::Type::KeyUp: ok = keyboard_.apply(protocol::decodeKey(frame)); break;
            case protocol::Type::MouseMove: case protocol::Type::MouseButton: case protocol::Type::MouseWheel: ok = mouse_.apply(decodeMouse(frame)); break;
            case protocol::Type::ReleaseAllKeys: case protocol::Type::MirrorStart: case protocol::Type::MirrorStop: ok = release(); break;
            default: peer_->close("unauthorized_message_type"); return;
        }
        if (!ok) { release(); peer_->close("input_injection_failed"); }
    }, [this](bool ready, const std::string& reason) {
        publish(ready, reason);
        if (!ready) { release(); if (!stopping_) asio::post(io_, [this] { accept(); }); }
    });
    const auto pending = peer_;
    listener_.async_accept(pending->socket(), [this, pending](const auto& error) {
        if (stopping_) return;
        if (error) { pending->close("accept_failed"); return; }
        asio::error_code endpointError;
        const auto endpoint = pending->socket().remote_endpoint(endpointError);
        if (endpointError || !privateAddress(endpoint.address())) { pending->close("non_lan_peer"); return; }
        if (!release()) { pending->close("key_cleanup_failed"); return; }
        pending->start(true);
    });
}
void AgentService::tick() {
    timer_.expires_after(std::chrono::milliseconds(100));
    timer_.async_wait([this](const auto& error) {
        if (error || stopping_) return;
        if (peer_ && peer_->ready()) {
            if (edge_) peer_->send(protocol::Type::DeviceStatus, {1, edge_()});
        } else if (!release()) publish(false, "key_cleanup_failed");
        tick();
    });
}
}
