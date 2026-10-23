#pragma once
#include "sharekeys/types.hpp"
#include <map>
#include <optional>

namespace sharekeys {
enum class Edge : std::uint8_t { None, Left, Right, Top, Bottom };
struct GridPosition { int x{}, y{}; auto operator<=>(const GridPosition&) const = default; };
class Workspace {
public:
    // Empty device ID represents the host at (0,0). One desktop per grid cell.
    bool configure(std::map<DeviceId, GridPosition> positions);
    std::optional<DeviceId> neighbor(const DeviceId& source, Edge edge) const;
private:
    std::map<DeviceId, GridPosition> positions_{{"", {0, 0}}};
};
}
