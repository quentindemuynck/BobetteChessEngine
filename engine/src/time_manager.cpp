#include "engine/time_manager.hpp"

#include <algorithm>

namespace engine {

std::chrono::milliseconds allocateTime(const TimeLimit& limit, chess::Color sideToMove) {
    using namespace std::chrono;

    // An explicit per-move time is authoritative.
    if (limit.moveTime) return *limit.moveTime;

    const auto& remaining = sideToMove == chess::Color::WHITE ? limit.whiteTime : limit.blackTime;
    const auto& increment = sideToMove == chess::Color::WHITE ? limit.whiteIncrement : limit.blackIncrement;

    // No clock info at all (only depth/nodes/infinite, or nothing): the clock
    // isn't what should stop this search, so don't cap it here.
    if (!remaining) return milliseconds::max();

    // Assume a long game remains when movesToGo isn't given (sudden death).
    constexpr int kAssumedMovesRemaining = 30;
    const int movesToGo = limit.movesToGo && *limit.movesToGo > 0 ? *limit.movesToGo : kAssumedMovesRemaining;

    milliseconds budget = *remaining / movesToGo;
    if (increment) budget += *increment;

    // Never plan to use the literal last moment on the clock.
    constexpr milliseconds kSafetyMargin{50};
    const milliseconds cap = *remaining > kSafetyMargin ? *remaining - kSafetyMargin : milliseconds{0};

    return std::clamp(budget, milliseconds{0}, cap);
}

} // namespace engine
