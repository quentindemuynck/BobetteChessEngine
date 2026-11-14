#include "engine/time_manager.hpp"

namespace engine {

std::chrono::milliseconds allocateTime(const TimeLimit& /*limit*/, chess::Color /*sideToMove*/) {
    // TODO: compute a time budget from the limits.
    return std::chrono::milliseconds{0}; // placeholder
}

} // namespace engine
