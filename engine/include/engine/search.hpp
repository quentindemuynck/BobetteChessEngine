#pragma once

#include <chess.hpp>
#include <vector>

#include "engine/time_manager.hpp"

namespace engine {

/// Per-root-move diagnostics, for debugging move choice. Add fields here as
/// you need more visibility (e.g. nodes searched, depth actually reached).
struct MoveEval {
    chess::Move move;
    int score = 0;          // full negamax score for this move (possibly multi-ply)
    bool timedOut = false;  // true if any branch under this move hit the time budget before depth 0
    int materialScore = 0;  // evaluate()'s material term, one ply deep (just this move, no reply)
    int mobilityScore = 0;  // evaluate()'s mobility term, one ply deep
    std::vector<chess::Move> pv; // expected continuation after this move (their reply, your reply, ...)
};

/// Searches the given position and returns the best move found within the
/// given limits. Convert the result to UCI text with chess::uci::moveToUci().
///
/// Returns chess::Move::NO_MOVE if there are no legal moves (checkmate or
/// stalemate).
///
/// Iterative deepening: searches depth 1, then 2, then 3, ... re-trying every
/// root move at each depth, for as long as `limit`'s time budget allows.
/// `limit.depth`, if set, caps how deep this may go. A depth is only trusted
/// (its result kept) once every root move finished searching at it — a depth
/// abandoned partway through the move list is discarded, so a move is never
/// compared against siblings that got a deeper look than it did.
///
/// If `debugOut` is non-null, it's filled with the last fully-completed
/// depth's per-root-move results (score, whether it got cut short, etc.), in
/// search order.
///
/// TODO: alpha-beta pruning, quiescence search.
chess::Move findBestMove(const chess::Board& board, const TimeLimit& limit,
                          std::vector<MoveEval>* debugOut = nullptr);

/// Diagnostics threaded through recursive negamax calls for one root move.
struct SearchDiagnostics {
    bool timedOut = false;
};

/// If `pv` is non-null, it's filled with the best line found from this
/// position onward (this node's best move, then the reply's, and so on).
int negamax(chess::Board& board, int depth,
    std::chrono::steady_clock::time_point start, std::chrono::milliseconds budget,
    SearchDiagnostics* diag = nullptr, std::vector<chess::Move>* pv = nullptr);

} // namespace engine
