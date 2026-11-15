#include "engine/evaluate.hpp"
#include "scoresystems/piecevalues.hpp"

namespace engine {

    int evaluate(const chess::Board& board)
    {
        chess::Color engineSide = board.sideToMove();

        int score = 0;

        for (chess::Color color : {chess::Color::WHITE, chess::Color::BLACK}) {
            int sign = (color == engineSide) ? 1 : -1;

            for (int pt = 0; pt < 6; ++pt) {
                chess::PieceType type = static_cast<chess::PieceType::underlying>(pt);
                chess::Bitboard bb = board.pieces(type, color);

                score += sign * engine::piecevalues::PieceValue[type] * bb.count();
            }
        }

        return score;
    }

} // namespace engine
