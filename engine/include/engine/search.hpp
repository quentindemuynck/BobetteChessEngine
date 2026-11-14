#pragma once

#include <chess.hpp>

#include "engine/time_manager.hpp"

namespace engine {

/// Searches the given position and returns the best move found within the
/// given limits. Convert the result to UCI text with chess::uci::moveToUci().
///
/// Returns chess::Move::NO_MOVE if there are no legal moves (checkmate or
/// stalemate) — and, for now, always, since the search is not implemented.
///
/// TODO: implement the search (iterative deepening, alpha-beta, ...).
chess::Move findBestMove(const chess::Board& board, const TimeLimit& limit);

} // namespace engine
