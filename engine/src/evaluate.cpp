#include "engine/evaluate.hpp"
#include "scoresystems/piecevalues.hpp"
#include "scoresystems/pst.hpp"

namespace engine {

    int evaluate(const chess::Board& board, EvalBreakdown* breakdown)
    {
        chess::Color engineSide = board.sideToMove();

        int material = 0;
        int positional = 0;
        chess::Board workingBoard{ board };

        for (chess::Color color : {chess::Color::WHITE, chess::Color::BLACK}) {
            int sign = (color == engineSide) ? 1 : -1;

            for (int pt = 0; pt < 6; ++pt) {
                chess::PieceType type = static_cast<chess::PieceType::underlying>(pt);
                chess::Bitboard bb = board.pieces(type, color);

                const std::array<int, 64>* table = nullptr;
                switch (pt) {
                case 0: table = &engine::pst::PawnTable;   break;
                case 1: table = &engine::pst::KnightTable; break;
                case 2: table = &engine::pst::BishopTable; break;
                case 3: table = &engine::pst::RookTable;   break;
                case 4: table = &engine::pst::QueenTable;  break;
                case 5: table = &engine::pst::KingTable;   break;
                }

                material += sign * engine::piecevalues::PieceValue[type] * bb.count();
                while (bb) {
                    int sq = bb.pop();
                    material += sign * engine::piecevalues::PieceValue[type];
                    positional += sign * (*table)[color == chess::Color::WHITE ? sq : sq ^ 56];
                }
            }
        }

        int mobility{ 0 };

        //chess::Movelist myMoves;
        //chess::movegen::legalmoves(myMoves, workingBoard);
        //
        //workingBoard.makeNullMove();
        //
        //chess::Movelist theirMoves;
        //chess::movegen::legalmoves(theirMoves, workingBoard);
        //workingBoard.unmakeNullMove();
        //
        //mobility = int(float(static_cast<int>(myMoves.size()) - static_cast<int>(theirMoves.size())) * 0.5f );

        if (breakdown) {
            breakdown->material = material;
            breakdown->mobility = mobility;
        }

        return material + mobility  + positional;
    }

} // namespace engine
