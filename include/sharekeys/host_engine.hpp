#pragma once
#include "sharekeys/network.hpp"
#include "sharekeys/queue.hpp"
#include "sharekeys/router.hpp"
#include "sharekeys/mouse.hpp"
#include "sharekeys/workspace.hpp"
#include <filesystem>
#include <mutex>
#include <thread>

namespace sharekeys {
struct DeviceConfig { DeviceId id; std::string address, pin; std::uint16_t port{24870}; };
struct DeviceStatus { DeviceConfig config; bool online{}; unsigned retries{}; std::string status{"disconnected"}; std::optional<double> roundTripMs; };
struct HostStatus { ControlMode mode{}; std::string active; std::vector<DeviceStatus> devices; std::string lastEvent; bool healthy{true}; };
class HostEngine {
public:
    explicit HostEngine(const std::filesystem::path& identity, std::function<bool()> centerLocal = {});
    ~HostEngine();
    HostEngine(const HostEngine&) = delete;
    HostEngine& operator=(const HostEngine&) = delete;
    void connect(DeviceConfig device);
    void disconnect(const DeviceId& id, bool remove = false);
    void setMode(ControlMode mode);
    void setActiveDevice(DeviceId id);
    void controlDevice(DeviceId id);
    void setTargets(std::vector<DeviceId> ids);
    void emergencyStop();
    bool captureKey(const KeyboardEvent& event); // One hook producer only.
    bool captureMouse(const MouseEvent& event); // A separate single producer.
    void setMouseMirroring(bool enabled);
    void configureLayout(std::map<DeviceId, GridPosition> positions, bool enabled);
    void configureHotkeys(std::uint8_t mirror, std::uint8_t emergency, std::uint8_t next);
    HostStatus status() const;
private:
    struct Slot {
        explicit Slot(asio::io_context& io) : timer(io) {}
        DeviceStatus view;
        std::shared_ptr<Peer> peer;
        asio::steady_timer timer;
        Backoff backoff;
        std::uint64_t generation{};
        bool enabled{};
    };
    void connectNow(const DeviceId& id);
    void updateStatus(const DeviceId& id, std::uint64_t generation, bool online, std::string reason);
    void pump();
    void publish(std::string event = {});
    void nextDevice();
    void switchEdge(const DeviceId& source, Edge edge);
    void enterRemote(const DeviceId& id);
    void fail() noexcept;
    asio::io_context io_;
    asio::ssl::context tls_{asio::ssl::context::tls};
    asio::steady_timer pumpTimer_{io_};
    std::map<DeviceId, std::unique_ptr<Slot>> slots_;
    InputRouter router_;
    Workspace workspace_;
    bool edgesEnabled_{};
    std::function<bool()> centerLocal_;
    std::chrono::steady_clock::time_point lastSwitch_{};
    struct CapturedInput { std::uint64_t epoch{}; bool mouse{}; CapturedKey key{}; MouseEvent pointer{}; };
    MpscQueue<CapturedInput, 2048> inputs_;
    std::array<bool, 5> mouseHeld_{}, mouseSuppressed_{}, mouseQuarantine_{};
    ShortcutGate gate_;
    std::atomic<std::uint64_t> inputEpoch_{1};
    std::uint64_t keyEpoch_{1}, mouseEpoch_{1}; // Each owned by its respective hook.
    std::atomic_bool emergency_{}, exclusive_{};
    std::atomic<std::uint32_t> shortcuts_{0x00091B4D};
    mutable std::mutex statusMutex_;
    HostStatus status_;
    std::thread thread_;
};
}
