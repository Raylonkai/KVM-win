#include "sharekeys/workspace.hpp"
#include <set>
namespace sharekeys {
bool Workspace::configure(std::map<DeviceId, GridPosition> positions) {
    if (positions.size() > max_devices || positions.contains("")) return false;
    positions.emplace("", GridPosition{});
    std::set<GridPosition> occupied;
    for (const auto& [id, position] : positions) {
        (void)id;
        if (position.x < -5 || position.x > 5 || position.y < -5 || position.y > 5 || !occupied.insert(position).second) return false;
    }
    positions_ = std::move(positions); return true;
}
std::optional<DeviceId> Workspace::neighbor(const DeviceId& source, Edge edge) const {
    const auto found = positions_.find(source); if (found == positions_.end()) return {};
    auto position = found->second;
    switch (edge) {
        case Edge::Left: --position.x; break;
        case Edge::Right: ++position.x; break;
        case Edge::Top: --position.y; break;
        case Edge::Bottom: ++position.y; break;
        default: return {};
    }
    for (const auto& [id, candidate] : positions_) if (candidate == position) return id;
    return {};
}
}
