#pragma once
#include "sharekeys/protocol.hpp"
#include <asio.hpp>
#include <asio/ssl.hpp>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <algorithm>

namespace sharekeys {
// All methods/callbacks are confined to one io_context thread.
class Peer : public std::enable_shared_from_this<Peer> {
public:
    using Message = std::function<void(const protocol::Frame&)>;
    using Status = std::function<void(bool, const std::string&)>;
    Peer(asio::io_context& io, asio::ssl::context& tls, std::string pin, Message message, Status status);
    asio::ip::tcp::socket& socket() { return stream_.next_layer(); }
    void start(bool server);
    bool send(protocol::Type type, std::vector<std::uint8_t> payload = {});
    void close(const std::string& reason);
    bool ready() const { return ready_; }
    std::optional<double> roundTripMs() const { return roundTripMs_; }
private:
    void readPrefix();
    void receive(protocol::Frame frame);
    void write();
    bool enqueue(protocol::Type type, std::vector<std::uint8_t> payload = {});
    void tick();
    asio::ssl::stream<asio::ip::tcp::socket> stream_;
    asio::steady_timer timer_;
    std::string pin_;
    Message message_;
    Status status_;
    std::array<std::uint8_t, 4> prefix_{};
    std::vector<std::uint8_t> body_;
    std::deque<std::shared_ptr<std::vector<std::uint8_t>>> outbound_;
    protocol::Sequence incoming_;
    std::uint64_t session_{}, sequence_{};
    bool server_{}, ready_{}, closed_{};
    std::chrono::steady_clock::time_point lastRead_{}, started_{}, rateWindow_{};
    unsigned rate_{};
    std::uint64_t pingNonce_{};
    std::chrono::steady_clock::time_point pingSent_{};
    std::optional<double> roundTripMs_;
};
bool privateAddress(const asio::ip::address& address);
class Backoff {
public:
    std::chrono::milliseconds next() { const auto result = delay_; delay_ = std::min(delay_ * 2, std::chrono::milliseconds(30000)); return result; }
    void reset() { delay_ = std::chrono::milliseconds(250); }
private:
    std::chrono::milliseconds delay_{250};
};
}
