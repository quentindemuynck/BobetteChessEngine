#pragma once

#include <chrono>
#include <cstdint>
#include <optional>

#include <chess.hpp>

namespace engine {

/// Limits for a single search, filled in from a UCI "go" command or a
/// websocket request. Every field is optional; an empty TimeLimit means
/// "use a sensible default".
struct TimeLimit {
    /// Fixed time for this move ("go movetime").
    std::optional<std::chrono::milliseconds> moveTime;

    /// Remaining clock time and increment per side ("go wtime/btime/winc/binc").
    std::optional<std::chrono::milliseconds> whiteTime;
    std::optional<std::chrono::milliseconds> blackTime;
    std::optional<std::chrono::milliseconds> whiteIncrement;
    std::optional<std::chrono::milliseconds> blackIncrement;

    /// Moves until the next time control ("go movestogo").
    std::optional<int> movesToGo;

    /// Maximum search depth in plies ("go depth").
    std::optional<int> depth;

    /// Maximum number of nodes to search ("go nodes").
    std::optional<std::uint64_t> nodes;

    /// Search until told to stop ("go infinite").
    bool infinite = false;
};

/// Decides how much wall-clock time the search may spend on the current move,
/// given the limits and the side to move.
///
/// TODO: implement time allocation (e.g. remaining / movesToGo + increment share,
///       with a safety margin).
std::chrono::milliseconds allocateTime(const TimeLimit& limit, chess::Color sideToMove);

} // namespace engine
