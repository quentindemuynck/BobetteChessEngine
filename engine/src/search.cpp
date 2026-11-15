#include "engine/search.hpp"

#include "engine/evaluate.hpp"

namespace engine {

chess::Move findBestMove(const chess::Board& board, const TimeLimit& /*limit*/) {
    chess::Movelist moves;
    chess::movegen::legalmoves(moves, board);

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
        workingBoard.makeMove(move);
        int moveScore = -evaluate(workingBoard);
        workingBoard.unmakeMove(move);

        if (moveScore > bestScore)
        {
            bestMove = move;
            bestScore = moveScore;
        }
    }
    return bestMove;
}

} // namespace engine
