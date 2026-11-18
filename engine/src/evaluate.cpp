#include "engine/evaluate.hpp"
#include "scoresystems/piecevalues.hpp"
#include "scoresystems/pst.hpp"

#include <algorithm>

namespace engine {

    namespace {
        // Squares strictly between a and b, `step` apart (8 for same file, 1
        // for same rank). Used to check nothing blocks the line between two
        // rooks before calling them "connected".
        chess::Bitboard between(int a, int b, int step) {
            chess::Bitboard mask;
            for (int sq = std::min(a, b) + step; sq < std::max(a, b); sq += step) {
                mask |= chess::Bitboard::fromSquare(sq);
            }
            return mask;
        }
    } // namespace

    int evaluate(const chess::Board& board, EvalBreakdown* breakdown)
    {
        chess::Color engineSide = board.sideToMove();

        int material = 0;
        int positional = 0;
        int bonus = 0;
        chess::Board workingBoard{ board };

        // Game phase: 1.0 with (close to) the full set of minors/majors still
        // on the board, fading toward 0.0 as material comes off. Standard
        // phase weights; pawns and kings don't count. Used to taper the
        // king's PST between "hide on the back rank" and "get active and
        // centralize" as the game empties out.
        int phaseMaterial = 0;
        for (chess::Color c : {chess::Color::WHITE, chess::Color::BLACK}) {
            phaseMaterial += board.pieces(chess::PieceType::KNIGHT, c).count() * 1;
            phaseMaterial += board.pieces(chess::PieceType::BISHOP, c).count() * 1;
            phaseMaterial += board.pieces(chess::PieceType::ROOK, c).count() * 2;
            phaseMaterial += board.pieces(chess::PieceType::QUEEN, c).count() * 4;
        }
        constexpr int kMaxPhaseMaterial = 24; // 2 * (2*1 + 2*1 + 2*2 + 1*4)
        const float phase = std::min(1.0f, static_cast<float>(phaseMaterial) / kMaxPhaseMaterial);

        // Non-king material per side (WHITE, BLACK) — feeds the mop-up
        // mating heuristic below, which needs to know who's overwhelmingly
        // ahead and who has almost nothing left to defend with.
        int sideMaterial[2] = { 0, 0 };

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

                sideMaterial[static_cast<int>(color)] +=
                    engine::piecevalues::PieceValue[type] * bb.count();

                while (bb) {
                    int sq = bb.pop();
                    int idx = color == chess::Color::WHITE ? sq : sq ^ 56;
                    material += sign * engine::piecevalues::PieceValue[type];

                    if (pt == 5) {
                        // King: taper between the middlegame table (hide on
                        // the back rank/corners) and the endgame one
                        // (centralize), instead of a single flat lookup.
                        float mg = static_cast<float>(engine::pst::KingTable[idx]);
                        float eg = static_cast<float>(engine::pst::EndgameKingTable[idx]);
                        positional += sign * static_cast<int>(mg * phase + eg * (1.0f - phase));
                    } else {
                        positional += sign * (*table)[idx];
                    }
                }
            }

            // Everything below depends only on `color`, not on `pt` — it ran
            // once per piece type (6x too much) when it lived inside the loop
            // above. Keep new per-color bonus terms down here, not up there.

            // preserve castling rights
            if (workingBoard.castlingRights().has(color) || hasCastled(workingBoard, color))
            {
                bonus += sign * bonuspoints::CASTLING_RIGHTS;
            }

            chess::Bitboard pawns = board.pieces(chess::PieceType::PAWN, color);

            for (int f = 0; f < 8; ++f) {
                chess::Bitboard fileMask(static_cast<chess::File::underlying>(f)); // all 8 squares of that file
                int pawnsOnFile = (pawns & fileMask).count();
                if (pawnsOnFile == 0) continue;

                if (pawnsOnFile > 1) {
                    bonus += sign * bonuspoints::DOUBLE_PAWNS * (pawnsOnFile - 1);
                }

                chess::Bitboard neighborMask; // empty by default
                if (f > 0) neighborMask |= chess::Bitboard(static_cast<chess::File::underlying>(f - 1));
                if (f < 7) neighborMask |= chess::Bitboard(static_cast<chess::File::underlying>(f + 1));

                bool isolated = (pawns & neighborMask).empty(); // no friendly pawns on either side

                if (isolated) {
                    bonus += sign * bonuspoints::ISOLATED_PAWN * pawnsOnFile;
                }
            }

            chess::Bitboard rooks = board.pieces(chess::PieceType::ROOK, color);

            for (int f = 0; f < 8; ++f) {
                chess::Bitboard fileMask(static_cast<chess::File::underlying>(f)); // all 8 squares of that file
                chess::Bitboard onFile = rooks & fileMask;
                int rooksOnFile = onFile.count();

                if (rooksOnFile > 1) {
                    chess::Bitboard gap = between(onFile.lsb(), onFile.msb(), 8);
                    if ((board.occ() & gap).empty()) { // nothing (friend or foe) blocking the file between them
                        bonus += sign * bonuspoints::CONNECTED_ROOKS * (rooksOnFile - 1);
                    }
                }
            }

            for (int l = 0; l < 8; ++l) {
                chess::Bitboard rankMask(static_cast<chess::Rank::underlying>(l)); // all 8 squares of that rank
                chess::Bitboard onRank = rooks & rankMask;
                int rooksOnRank = onRank.count();

                if (rooksOnRank > 1) {
                    chess::Bitboard gap = between(onRank.lsb(), onRank.msb(), 1);
                    if ((board.occ() & gap).empty()) { // nothing blocking the rank between them
                        bonus += sign * bonuspoints::CONNECTED_ROOKS * (rooksOnRank - 1);
                    }
                }
            }
        }

        // Basic mating technique ("mop-up"): once one side has almost
        // nothing left to defend with and the other has enough material to
        // actually force mate, reward pushing the weak king toward the
        // edge/corner and bringing the strong side's own king closer to
        // help — the two ideas behind every basic king+queen/rook-vs-king
        // mate. Gated tightly so it never kicks in during normal,
        // roughly-even play.
        {
            const chess::Color weakSide =
                sideMaterial[0] < sideMaterial[1] ? chess::Color::WHITE : chess::Color::BLACK;
            const chess::Color strongSide = ~weakSide;
            const int advantage = sideMaterial[static_cast<int>(strongSide)] -
                                   sideMaterial[static_cast<int>(weakSide)];

            if (sideMaterial[static_cast<int>(weakSide)] <= piecevalues::PieceValue[static_cast<int>(chess::PieceType::BISHOP)] &&
                advantage >= piecevalues::PieceValue[static_cast<int>(chess::PieceType::ROOK)]) {

                const int sign = (strongSide == engineSide) ? 1 : -1;

                const chess::Square strongKing = board.kingSq(strongSide);
                const chess::Square weakKing = board.kingSq(weakSide);

                const int wf = weakKing.index() % 8, wr = weakKing.index() / 8;
                const int sf = strongKing.index() % 8, sr = strongKing.index() / 8;

                // 0 at the center, growing toward the rim/corners — the
                // "drive the king to the edge" half of the technique.
                const int centerDistance = std::max(std::abs(wf - 3), std::abs(wf - 4)) +
                                            std::max(std::abs(wr - 3), std::abs(wr - 4));

                // Chebyshev distance between the two kings — the strong
                // side's king has to come help, it can't mate alone.
                const int kingDistance = std::max(std::abs(sf - wf), std::abs(sr - wr));

                bonus += sign * (centerDistance * 10 - kingDistance * 4);
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

        return material + mobility  + positional + bonus;
    }

    bool hasCastled(const chess::Board& board, chess::Color color) {
        chess::Square kingSq = board.kingSq(color);
        chess::Square kingside = color == chess::Color::WHITE ? chess::Square("g1") : chess::Square("g8");
        chess::Square queenside = color == chess::Color::WHITE ? chess::Square("c1") : chess::Square("c8");
        return kingSq == kingside || kingSq == queenside;
    }

} // namespace engine
