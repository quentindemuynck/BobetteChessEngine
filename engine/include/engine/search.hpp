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
chess::Move findBestMove(const chess::Board& board, const TimeLimit& limit,
                          std::vector<MoveEval>* debugOut = nullptr);

/// Diagnostics threaded through recursive negamax/quiescence calls for one
/// root move.
struct SearchDiagnostics {
    bool timedOut = false;
};

/// Larger in magnitude than any real evaluate()/mate score, so it's safe to
/// use as the initial alpha/beta window.
inline constexpr int kInfinity = 1'000'000'000;

/// Score for delivering mate, before the per-ply distance adjustment
/// (`kMateScore - ply`) that makes a closer mate score higher than a farther
/// one — and, on the losing side, makes a farther-off loss preferred over an
/// immediate one. Comfortably below kInfinity so the two are never confused.
inline constexpr int kMateScore = 1'000'000;

/// Alpha-beta negamax: `alpha`/`beta` bound the range of scores still worth
/// exploring — a branch that proves it falls outside that range gets cut
/// off, since neither side would ever let the game reach it. Pass
/// `-kInfinity, kInfinity` for a fresh, unconstrained search. `ply` is the
/// distance from this search's own root (0 at the first call), used only to
/// make mate scores distance-sensitive.
///
/// If `pv` is non-null, it's filled with the best line found from this
/// position onward (this node's best move, then the reply's, and so on).
int negamax(chess::Board& board, int depth, int ply, int alpha, int beta,
    std::chrono::steady_clock::time_point start, std::chrono::milliseconds budget,
    SearchDiagnostics* diag = nullptr, std::vector<chess::Move>* pv = nullptr);

/// Extends a search past depth 0 along capture sequences (and, if in check,
/// any legal reply) until the position is "quiet" — no more profitable
/// captures — instead of evaluating mid-exchange. This is what `negamax`
/// calls once `depth` reaches 0, so a position right after a capture isn't
/// scored before seeing the recapture.
int quiescence(chess::Board& board, int ply, int alpha, int beta,
    std::chrono::steady_clock::time_point start, std::chrono::milliseconds budget,
    SearchDiagnostics* diag = nullptr);

} // namespace engine
