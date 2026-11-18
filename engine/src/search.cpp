#include "engine/search.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

#include "engine/evaluate.hpp"
#include "engine/openingbook.hpp"
#include "scoresystems/piecevalues.hpp"

namespace engine {

namespace {

// MVV-LVA (most valuable victim, least valuable attacker): captures sort
// before quiet moves, and among captures, taking a big piece with a small
// one sorts before taking a big piece with another big one. This is what
// lets alpha-beta actually cut off branches early instead of only pruning
// whatever the move generator happened to list last. `preferred`, if given
// (the transposition table's remembered best move for this position), is
// sorted ahead of everything else, since it already proved itself last time.
int moveOrderScore(const chess::Board& board, chess::Move move) {
    if (!board.isCapture(move)) return 0;
    int victim = piecevalues::PieceValue[board.getCapturing<chess::PieceType>(move)];
    int attacker = piecevalues::PieceValue[board.at<chess::PieceType>(move.from())];
    return victim * 10 - attacker;
}

void orderMoves(const chess::Board& board, chess::Movelist& moves,
                 chess::Move preferred = chess::Move::NO_MOVE) {
    std::sort(moves.begin(), moves.end(), [&board, preferred](chess::Move a, chess::Move b) {
        if (a == preferred) return true;
        if (b == preferred) return false;
        return moveOrderScore(board, a) > moveOrderScore(board, b);
    });
}

// --- Transposition table ---------------------------------------------------
// Caches, per position (by Zobrist hash), the last depth it was searched to,
// the score found, whether that score is exact or just a bound (alpha-beta
// cutoffs only ever prove a bound, not the exact value), and the move that
// was best. Reused both to skip re-searching a position outright and to try
// its remembered best move first elsewhere, which is what makes alpha-beta
// cutoffs actually fire early. Single global table: safe only because this
// engine searches on one worker thread at a time (see BotClient::workerLoop).

enum class Bound : std::uint8_t { EXACT, LOWER, UPPER };

struct TTEntry {
    std::uint64_t hash = 0;
    int depth = -1;
    int score = 0;
    Bound bound = Bound::EXACT;
    chess::Move bestMove = chess::Move::NO_MOVE;
};

constexpr std::size_t kTTSize = 1 << 20; // ~1M entries, a few tens of MB
std::vector<TTEntry> g_tt(kTTSize);

// Mate scores (kMateScore - ply) are specific to how deep *this* node is in
// *this* search, so a cached one can't be reused as-is from a different ply
// without adjusting it — easy to get subtly wrong. Simplest safe choice:
// never use a cached mate score for a cutoff, only ever for move ordering.
bool isMateScore(int score) {
    return score > kMateScore - 10000 || score < -(kMateScore - 10000);
}

} // namespace

chess::Move findBestMove(const chess::Board& board, const TimeLimit& limit,
                          std::vector<MoveEval>* debugOut) {
    if (auto bookMove = lookupBookMove(board)) {
        return *bookMove; // known opening position: skip the search entirely
    }

    chess::Movelist moves;
    chess::movegen::legalmoves(moves, board);
    orderMoves(board, moves);

    const auto start = std::chrono::steady_clock::now();
    const auto budget = allocateTime(limit, board.sideToMove());

    if (moves.empty()) {
        return chess::Move{chess::Move::NO_MOVE}; // checkmate or stalemate
    }

    chess::Board workingBoard{ board };

    // Fallback if even depth 1 can't finish: play the first legal move.
    chess::Move bestMove = moves.front();

    // limit.depth, if given, is a ceiling on how deep iterative deepening may
    // go — not a depth to jump straight to (that starved later root moves of
    // the shared time budget whenever an earlier move's search ran long).
    const int maxDepth = limit.depth.has_value() ? limit.depth.value() : std::numeric_limits<int>::max();

    for (int depth = 1; depth <= maxDepth; ++depth) {
        chess::Move bestMoveThisDepth = moves.front();
        int bestScoreThisDepth{ -kInfinity };
        std::vector<MoveEval> evalsThisDepth;
        bool completed = true;

        for (chess::Move move : moves)
        {
            if (std::chrono::steady_clock::now() - start >= budget) {
                completed = false;
                break; // no time left to even start this move at this depth
            }

            workingBoard.makeMove(move);
            SearchDiagnostics diag;
            std::vector<chess::Move> pv;
            // Root moves always get a full, unconstrained window: every one of
            // them has to be scored exactly, since bestMoveThisDepth is chosen
            // by comparing these scores directly against each other.
            int moveScore = -negamax(workingBoard, depth - 1, 1, -kInfinity, kInfinity, start, budget,
                                      &diag, debugOut ? &pv : nullptr);

            if (debugOut) {
                // One-ply breakdown of this move alone (no reply considered), for
                // comparing material vs mobility directly — separate from moveScore,
                // which is the full (possibly multi-ply) negamax result above.
                EvalBreakdown breakdown;
                evaluate(workingBoard, &breakdown);
                evalsThisDepth.push_back({ move, moveScore, diag.timedOut, -breakdown.material,
                                           -breakdown.mobility, std::move(pv) });
            }

            workingBoard.unmakeMove(move);

            if (diag.timedOut) {
                // This move's own search got cut short, so its score isn't
                // comparable to the other moves' full-depth scores — the whole
                // depth is unfair, not just this move.
                completed = false;
            }

            if (moveScore > bestScoreThisDepth)
            {
                bestMoveThisDepth = move;
                bestScoreThisDepth = moveScore;
            }
        }

        if (!completed) break; // discard this partial depth, keep the last full one

        bestMove = bestMoveThisDepth;
        if (debugOut) *debugOut = std::move(evalsThisDepth);

        if (std::chrono::steady_clock::now() - start >= budget) break; // no time for another depth
    }

    return bestMove;
}


int negamax(chess::Board& board, int depth, int ply, int alpha, int beta,
    std::chrono::steady_clock::time_point start, std::chrono::milliseconds budget,
    SearchDiagnostics* diag, std::vector<chess::Move>* pv) {

    if (depth == 0) {
        return quiescence(board, ply, alpha, beta, start, budget, diag);
    }
    if (std::chrono::steady_clock::now() - start >= budget) {
        if (diag) diag->timedOut = true;
        return evaluate(board);
    }

    const int originalAlpha = alpha;
    const std::uint64_t hash = board.hash();
    TTEntry& entry = g_tt[hash % kTTSize];
    chess::Move ttMove = chess::Move::NO_MOVE;

    if (entry.hash == hash) {
        ttMove = entry.bestMove; // try it first below, regardless of depth/bound
        if (entry.depth >= depth && !isMateScore(entry.score)) {
            if (entry.bound == Bound::EXACT) return entry.score;
            if (entry.bound == Bound::LOWER) alpha = std::max(alpha, entry.score);
            else if (entry.bound == Bound::UPPER) beta = std::min(beta, entry.score);
            if (alpha >= beta) return entry.score;
        }
    }

    chess::Movelist moves;
    chess::movegen::legalmoves(moves, board);

    if (moves.empty()) {
        // no legal moves here = checkmate or stalemate — evaluate() can't tell
        // the difference (it only counts material), so handle it explicitly.
        // Distance-adjusted: a mate found closer to the root (smaller ply)
        // scores higher for the side delivering it, so the search prefers
        // the fastest mate, and the side getting mated prefers delaying it.
        return board.inCheck() ? -(kMateScore - ply) : 0;
    }

    orderMoves(board, moves, ttMove);

    chess::Move bestMoveHere = moves.front();
    int best = -kInfinity;
    for (chess::Move move : moves) {
        board.makeMove(move);
        std::vector<chess::Move> childPv;
        int score = -negamax(board, depth - 1, ply + 1, -beta, -alpha, start, budget, diag,
                              pv ? &childPv : nullptr);
        board.unmakeMove(move);

        if (score > best) {
            best = score;
            bestMoveHere = move;
            if (pv) {
                pv->clear();
                pv->push_back(move);
                pv->insert(pv->end(), childPv.begin(), childPv.end());
            }
        }

        alpha = std::max(alpha, score);
        if (alpha >= beta) {
            // Beta cutoff: whoever moved us into this position already has a
            // better alternative elsewhere, so they'd never let the game
            // reach this branch — no point scoring the remaining siblings.
            break;
        }
    }

    entry.hash = hash;
    entry.depth = depth;
    entry.score = best;
    entry.bound = best <= originalAlpha ? Bound::UPPER : (best >= beta ? Bound::LOWER : Bound::EXACT);
    entry.bestMove = bestMoveHere;

    return best;
}

int quiescence(chess::Board& board, int ply, int alpha, int beta,
    std::chrono::steady_clock::time_point start, std::chrono::milliseconds budget,
    SearchDiagnostics* diag) {

    if (std::chrono::steady_clock::now() - start >= budget) {
        if (diag) diag->timedOut = true;
        return evaluate(board);
    }

    const bool inCheck = board.inCheck();

    // Not in check: "doing nothing further" (stand pat) is itself a legal
    // option worth comparing against — this is what stops the search from
    // chasing a capture that doesn't actually improve the position.
    if (!inCheck) {
        int standPat = evaluate(board);
        if (standPat >= beta) return standPat;
        alpha = std::max(alpha, standPat);
    }

    chess::Movelist moves;
    if (inCheck) {
        // In check, "quiet" isn't meaningful — every legal reply matters,
        // since there may be no way out at all (checkmate).
        chess::movegen::legalmoves(moves, board);
    } else {
        chess::movegen::legalmoves<chess::movegen::MoveGenType::CAPTURE>(moves, board);
    }

    if (moves.empty()) {
        // No legal replies while in check = checkmate; otherwise, simply no
        // more captures worth considering — alpha already holds the best of
        // the stand-pat score and whatever captures were tried before this.
        return inCheck ? -(kMateScore - ply) : alpha;
    }

    orderMoves(board, moves);

    for (chess::Move move : moves) {
        board.makeMove(move);
        int score = -quiescence(board, ply + 1, -beta, -alpha, start, budget, diag);
        board.unmakeMove(move);

        if (score > alpha) {
            alpha = score;
            if (alpha >= beta) break;
        }
    }

    return alpha;
}

} // namespace engine
