#pragma once

#include <array>

namespace engine::pst
{
    // All tables are written rank 1 (White's back rank) first, rank 8 last —
    // matching this engine's square indexing (a1=0 ... h8=63) directly, so
    // White looks these up as-is and Black uses index ^ 56 (vertical mirror).

    constexpr std::array<int, 64> PawnTable = {
        // rank 1 (never occupied by a pawn, value irrelevant)
         0,  0,  0,  0,  0,  0,  0,  0,
        // rank 2 (start square: small bonus for center, penalty for d/e staying put)
         5, 10, 10,-20,-20, 10, 10,  5,
        // rank 3
         5, -5,-10,  0,  0,-10, -5,  5,
        // rank 4 (center push)
         0,  0,  0, 20, 20,  0,  0,  0,
        // rank 5
         5,  5, 10, 25, 25, 10,  5,  5,
        // rank 6
        10, 10, 20, 30, 30, 20, 10, 10,
        // rank 7 (about to promote)
        50, 50, 50, 50, 50, 50, 50, 50,
        // rank 8 (never occupied by a pawn, value irrelevant)
         0,  0,  0,  0,  0,  0,  0,  0,
    };

    constexpr std::array<int, 64> KnightTable = {
        // rank 1
        -50, -40, -30, -30, -30, -30, -40, -50,
        // rank 2
        -40, -20,   0,   0,   0,   0, -20, -40,
        // rank 3
        -30,   0,  10,  15,  15,  10,   0, -30,
        // rank 4 (center: best squares for a knight)
        -30,   5,  15,  20,  20,  15,   5, -30,
        // rank 5
        -30,   0,  15,  20,  20,  15,   0, -30,
        // rank 6
        -30,   5,  10,  15,  15,  10,   5, -30,
        // rank 7
        -40, -20,   0,   5,   5,   0, -20, -40,
        // rank 8 (rim is dim)
        -50, -40, -30, -30, -30, -30, -40, -50,
    };

    constexpr std::array<int, 64> BishopTable = {
        // rank 1
        -20, -10, -10, -10, -10, -10, -10, -20,
        // rank 2
        -10,   5,   0,   0,   0,   0,   5, -10,
        // rank 3
        -10,  10,  10,  10,  10,  10,  10, -10,
        // rank 4
        -10,   0,  10,  10,  10,  10,   0, -10,
        // rank 5
        -10,   5,   5,  10,  10,   5,   5, -10,
        // rank 6
        -10,   0,   5,  10,  10,   5,   0, -10,
        // rank 7
        -10,   0,   0,   0,   0,   0,   0, -10,
        // rank 8
        -20, -10, -10, -10, -10, -10, -10, -20,
    };

    constexpr std::array<int, 64> RookTable = {
        // rank 1 (small bonus toward the center files, for castled rooks)
          0,   0,   0,   5,   5,   0,   0,   0,
        // rank 2
         -5,   0,   0,   0,   0,   0,   0,  -5,
        // rank 3
         -5,   0,   0,   0,   0,   0,   0,  -5,
        // rank 4
         -5,   0,   0,   0,   0,   0,   0,  -5,
        // rank 5
         -5,   0,   0,   0,   0,   0,   0,  -5,
        // rank 6
         -5,   0,   0,   0,   0,   0,   0,  -5,
        // rank 7 ("rooks on the 7th")
          5,  10,  10,  10,  10,  10,  10,   5,
        // rank 8
          0,   0,   0,   0,   0,   0,   0,   0,
    };

    // Deliberately much flatter than the other tables — a mild nudge, not a
    // reason on its own to bring the queen out before the minor pieces.
    constexpr std::array<int, 64> QueenTable = {
        // rank 1 (tiny penalty right on d1/e1 to not be the first move played)
        -20, -10, -10,  -5,  -5, -10, -10, -20,
        // rank 2
        -10,   0,   5,   0,   0,   5,   0, -10,
        // rank 3
        -10,   5,   5,   5,   5,   5,   5, -10,
        // rank 4
         -5,   0,   5,   5,   5,   5,   0,  -5,
        // rank 5
         -5,   0,   5,   5,   5,   5,   0,  -5,
        // rank 6
        -10,   5,   5,   5,   5,   5,   5, -10,
        // rank 7
        -10,   0,   5,   0,   0,   5,   0, -10,
        // rank 8
        -20, -10, -10,  -5,  -5, -10, -10, -20,
    };

    // Middlegame shape: stay on the back rank near the corners (castled),
    // stay far away from the center. This is what should stop the engine
    // from wandering its king out for no reason.
    constexpr std::array<int, 64> KingTable = {
        // rank 1 (g1/c1-ish castled corners rewarded)
         20,  30,  10,   0,   0,  10,  30,  20,
        // rank 2
         20,  20,   0,   0,   0,   0,  20,  20,
        // rank 3
        -10, -20, -20, -20, -20, -20, -20, -10,
        // rank 4
        -20, -30, -30, -40, -40, -30, -30, -20,
        // rank 5
        -30, -40, -40, -50, -50, -40, -40, -30,
        // rank 6
        -30, -40, -40, -50, -50, -40, -40, -30,
        // rank 7
        -30, -40, -40, -50, -50, -40, -40, -30,
        // rank 8 (worst: dead center of the board)
        -30, -40, -40, -50, -50, -40, -40, -30,
    };

    // Endgame shape: the opposite instinct from KingTable. With few pieces
    // left there's usually no attack to hide from, and the king becomes one
    // of the strongest pieces on the board — it needs to be centralized to
    // help push passed pawns and to take part in mating the enemy king.
    constexpr std::array<int, 64> EndgameKingTable = {
        // rank 1 (corners are now the worst squares, not the best)
        -50, -30, -30, -30, -30, -30, -30, -50,
        // rank 2
        -30, -30,   0,   0,   0,   0, -30, -30,
        // rank 3
        -30, -10,  20,  30,  30,  20, -10, -30,
        // rank 4 (center: best squares for an endgame king)
        -30, -10,  30,  40,  40,  30, -10, -30,
        // rank 5
        -30, -10,  30,  40,  40,  30, -10, -30,
        // rank 6
        -30, -10,  20,  30,  30,  20, -10, -30,
        // rank 7
        -30, -20, -10,   0,   0, -10, -20, -30,
        // rank 8
        -50, -40, -30, -20, -20, -30, -40, -50,
    };
}
