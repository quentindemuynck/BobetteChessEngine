#include "engine/search.hpp"

#include "engine/evaluate.hpp"

namespace engine {

chess::Move findBestMove(const chess::Board& board, const TimeLimit& limit) {
    chess::Movelist moves;
    chess::movegen::legalmoves(moves, board);

    const auto start = std::chrono::steady_clock::now();
    const auto budget = allocateTime(limit, board.sideToMove());

    if (moves.empty()) {
        return chess::Move{chess::Move::NO_MOVE}; // checkmate or stalemate
    }

    // PLACEHOLDER so the bot can be tested end to end: plays the first legal move.
    // TODO: replace with the real search (iterative deepening, alpha-beta, ...),
    //       using evaluate() and respecting the time limit (see allocateTime).

    chess::Board workingBoard{ board };

    chess::Move bestMove = moves.front();
    int bestScore{ std::numeric_limits<int>::min() };

    for(chess::Move move :  moves)
    {
        int depth{ limit.depth.has_value() ? limit.depth.value() : 1 };
        workingBoard.makeMove(move);
        int moveScore = -negamax(workingBoard,  depth - 1, start, budget);
        workingBoard.unmakeMove(move);

        if (moveScore > bestScore)
        {
            bestMove = move;
            bestScore = moveScore;
        }
    }
    return bestMove;
}


int negamax(chess::Board& board, int depth,
    std::chrono::steady_clock::time_point start, std::chrono::milliseconds budget) {

    if (depth == 0 || std::chrono::steady_clock::now() - start >= budget) {
        return evaluate(board);  
    }

    chess::Movelist moves;
    chess::movegen::legalmoves(moves, board);

    if (moves.empty()) {
        // no legal moves here = checkmate or stalemate — evaluate() can't tell
        // the difference (it only counts material), so handle it explicitly
        return board.inCheck() ? -99999 : 0;
    }

    int best = std::numeric_limits<int>::min();
    for (chess::Move move : moves) {
        board.makeMove(move);
        int score = -negamax(board, depth - 1, start, budget);
        board.unmakeMove(move);
        best = std::max(best, score);
    }
    return best;
}

} // namespace engine
