#include "sharekeys/network.hpp"
#include "sharekeys/security.hpp"

namespace sharekeys {
using protocol::Type;
Peer::Peer(asio::io_context& io, asio::ssl::context& tls, std::string pin, Message message, Status status)
    : stream_(io, tls), timer_(io), pin_(std::move(pin)), message_(std::move(message)), status_(std::move(status)) {
    if (!security::validPin(pin_)) throw std::invalid_argument("pair a certificate fingerprint first");
    stream_.set_verify_mode(asio::ssl::verify_peer | asio::ssl::verify_fail_if_no_peer_cert);
    stream_.set_verify_callback([this](bool, asio::ssl::verify_context& context) {
        auto* store = context.native_handle();
        // Explicit leaf pin is the trust anchor; do not accept arbitrary CA peers.
        try { return X509_STORE_CTX_get_error_depth(store) == 0 && security::verifyPin(X509_STORE_CTX_get_current_cert(store), pin_); }
        catch (...) { return false; }
    });
}
void Peer::start(bool server) {
    server_ = server;
    started_ = lastRead_ = rateWindow_ = std::chrono::steady_clock::now();
    asio::error_code ec;
    socket().set_option(asio::ip::tcp::no_delay(true), ec);
    if (ec) { close("socket_option"); return; }
    tick();
    const auto self = shared_from_this();
    stream_.async_handshake(server ? asio::ssl::stream_base::server : asio::ssl::stream_base::client, [self](const asio::error_code& error) {
        if (self->closed_) return;
        if (error) { self->close("tls_authentication_failed"); return; }
        if (self->server_) { self->session_ = security::randomSession(); self->enqueue(Type::Hello); }
        self->readPrefix();
    });
}
bool Peer::enqueue(Type type, std::vector<std::uint8_t> payload) {
    if (closed_) return false;
    if (outbound_.size() >= 1024 || sequence_ == UINT64_MAX) { close("outbound_queue_limit"); return false; }
    auto bytes = std::make_shared<std::vector<std::uint8_t>>(protocol::encode({type, session_, ++sequence_, std::move(payload)}));
    const bool idle = outbound_.empty(); outbound_.push_back(std::move(bytes));
    if (idle) write();
    return true;
}
bool Peer::send(Type type, std::vector<std::uint8_t> payload) {
    return ready_ && enqueue(type, std::move(payload));
}
void Peer::write() {
    const auto self = shared_from_this(); const auto bytes = outbound_.front();
    asio::async_write(stream_, asio::buffer(*bytes), [self, bytes](const asio::error_code& error, std::size_t) {
        if (self->closed_) return;
        if (error) { self->close("write_failed"); return; }
        self->outbound_.pop_front();
        if (!self->outbound_.empty()) self->write();
    });
}
void Peer::readPrefix() {
    const auto self = shared_from_this();
    asio::async_read(stream_, asio::buffer(prefix_), [self](const asio::error_code& error, std::size_t) {
        if (self->closed_) return;
        if (error) { self->close("read_failed"); return; }
        try { self->body_.resize(protocol::bodyLength(self->prefix_)); }
        catch (...) { self->close("invalid_length"); return; }
        asio::async_read(self->stream_, asio::buffer(self->body_), [self](const asio::error_code& bodyError, std::size_t) {
            if (self->closed_) return;
            if (bodyError) { self->close("truncated_message"); return; }
            try { self->receive(protocol::decode(self->body_)); }
            catch (...) { self->close("invalid_message"); return; }
            if (!self->closed_) self->readPrefix();
        });
    });
}
void Peer::receive(protocol::Frame frame) {
    const auto now = std::chrono::steady_clock::now();
    if (now - rateWindow_ >= std::chrono::seconds(1)) { rateWindow_ = now; rate_ = 0; }
    if (++rate_ > 2000 || !incoming_.accept(frame.sequence)) { close("rate_or_sequence_violation"); return; }
    if (!server_ && session_ == 0 && frame.type == Type::Hello) {
        session_ = frame.session; enqueue(Type::Authenticate); lastRead_ = now; return;
    }
    if (frame.session != session_ || session_ == 0) { close("stale_session"); return; }
    if (!ready_) {
        if (frame.type != Type::Authenticate) { close("unauthorized_command"); return; }
        if (server_) enqueue(Type::Authenticate);
        ready_ = true; lastRead_ = now; status_(true, "authenticated"); return;
    }
    lastRead_ = now;
    if (frame.type == Type::Heartbeat) {
        if (frame.payload.size() == 9) {
            if (frame.payload[0] == 0) { frame.payload[0] = 1; enqueue(Type::Heartbeat, std::move(frame.payload)); }
            else {
                std::uint64_t nonce{};
                for (std::size_t i = 1; i < 9; ++i) nonce = (nonce << 8) | frame.payload[i];
                if (nonce == pingNonce_ && pingNonce_ != 0) roundTripMs_ = std::chrono::duration<double, std::milli>(now - pingSent_).count();
            }
        }
        return;
    }
    if (frame.type == Type::Disconnect) { close("peer_disconnect"); return; }
    if (frame.type == Type::Hello || frame.type == Type::Authenticate || frame.type == Type::PairRequest || frame.type == Type::PairApproved) { close("unexpected_handshake"); return; }
    message_(frame);
}
void Peer::tick() {
    timer_.expires_after(std::chrono::seconds(1));
    const auto self = shared_from_this();
    timer_.async_wait([self](const asio::error_code& error) {
        if (error || self->closed_) return;
        const auto now = std::chrono::steady_clock::now();
        if ((!self->ready_ && now - self->started_ >= std::chrono::seconds(5)) || (self->ready_ && now - self->lastRead_ >= std::chrono::seconds(3))) { self->close("heartbeat_timeout"); return; }
        if (self->ready_) {
            self->pingSent_ = now; ++self->pingNonce_;
            std::vector<std::uint8_t> ping{0};
            for (unsigned i = 8; i > 0; --i) ping.push_back(static_cast<std::uint8_t>(self->pingNonce_ >> ((i - 1) * 8)));
            self->send(Type::Heartbeat, std::move(ping));
        }
        if (!self->closed_) self->tick();
    });
}
void Peer::close(const std::string& reason) {
    if (closed_) return;
    closed_ = true; ready_ = false;
    asio::error_code ignored;
    timer_.cancel(); socket().cancel(ignored); socket().shutdown(asio::ip::tcp::socket::shutdown_both, ignored); socket().close(ignored);
    outbound_.clear();
    status_(false, reason);
}
bool privateAddress(const asio::ip::address& address) {
    if (address.is_loopback()) return true;
    if (!address.is_v4()) return false; // IPv4 LAN transport in v1.
    const auto bytes = address.to_v4().to_bytes();
    return bytes[0] == 10 || (bytes[0] == 172 && bytes[1] >= 16 && bytes[1] <= 31) || (bytes[0] == 192 && bytes[1] == 168) || (bytes[0] == 169 && bytes[1] == 254);
}
}
