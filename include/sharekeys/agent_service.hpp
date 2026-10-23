#pragma once
#include "sharekeys/network.hpp"
#include "sharekeys/keyboard.hpp"
#include "sharekeys/mouse.hpp"
#include <filesystem>
#include <mutex>
#include <thread>

namespace sharekeys {
struct AgentStatus { bool connected{}; std::string message{"listening"}; };
// Sinks outlive the service. All injection and network callbacks run on its
// private io thread; destruction joins that thread before the sinks can die.
class AgentService {
public:
    AgentService(const std::filesystem::path& identity, std::string pin,
                 const std::string& address, std::uint16_t port,
                 KeyboardSink& keys, MouseSink& mouse,
                 std::function<std::uint8_t()> edge = {});
    ~AgentService();
    AgentService(const AgentService&) = delete;
    AgentService& operator=(const AgentService&) = delete;
    AgentStatus status() const;
private:
    void accept();
    void tick();
    bool release();
    void publish(bool connected, std::string message);
    asio::io_context io_;
    asio::ssl::context tls_{asio::ssl::context::tls};
    asio::ip::tcp::acceptor listener_{io_};
    asio::steady_timer timer_{io_};
    InjectedKeyboard keyboard_;
    InjectedMouse mouse_;
    std::string pin_;
    std::function<std::uint8_t()> edge_;
    std::shared_ptr<Peer> peer_;
    bool stopping_{};
    mutable std::mutex mutex_;
    AgentStatus status_;
    std::thread thread_;
};
}
