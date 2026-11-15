#include "engine/search.hpp"

#include "engine/evaluate.hpp"

namespace engine {

chess::Move findBestMove(const chess::Board& board, const TimeLimit& limit,
                          std::vector<MoveEval>* debugOut) {
    chess::Movelist moves;
    chess::movegen::legalmoves(moves, board);

    const auto start = std::chrono::steady_clock::now();
    const auto budget = allocateTime(limit, board.sideToMove());

    if (moves.empty()) {
        return chess::Move{chess::Move::NO_MOVE}; // checkmate or stalemate
    }

    // TODO: alpha-beta pruning, quiescence search.

    chess::Board workingBoard{ board };

    // Fallback if even depth 1 can't finish: play the first legal move.
    chess::Move bestMove = moves.front();

    // limit.depth, if given, is a ceiling on how deep iterative deepening may
    // go — not a depth to jump straight to (that starved later root moves of
    // the shared time budget whenever an earlier move's search ran long).
    const int maxDepth = limit.depth.has_value() ? limit.depth.value() : std::numeric_limits<int>::max();

    for (int depth = 1; depth <= maxDepth; ++depth) {
        chess::Move bestMoveThisDepth = moves.front();
        int bestScoreThisDepth{ std::numeric_limits<int>::min() };
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
            int moveScore = -negamax(workingBoard, depth - 1, start, budget, &diag, debugOut ? &pv : nullptr);

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


int negamax(chess::Board& board, int depth,
    std::chrono::steady_clock::time_point start, std::chrono::milliseconds budget,
    SearchDiagnostics* diag, std::vector<chess::Move>* pv) {

    if (depth == 0) {
        return evaluate(board);
    }
    if (std::chrono::steady_clock::now() - start >= budget) {
        if (diag) diag->timedOut = true;
        return evaluate(board);
    }

    chess::Movelist moves;
    chess::movegen::legalmoves(moves, board);

    if (moves.empty()) {
        // no legal moves here = checkmate or stalemate — evaluate() can't tell
        // the difference (it only counts material), so handle it explicitly
        return board.inCheck() ? -99999999 : 0;
    }

    int best = std::numeric_limits<int>::min();
    for (chess::Move move : moves) {
        board.makeMove(move);
        std::vector<chess::Move> childPv;
        int score = -negamax(board, depth - 1, start, budget, diag, pv ? &childPv : nullptr);
        board.unmakeMove(move);

        if (score > best) {
            best = score;
            if (pv) {
                pv->clear();
                pv->push_back(move);
                pv->insert(pv->end(), childPv.begin(), childPv.end());
            }
        }
    }
    return best;
}

} // namespace engine
