#pragma once

#include <array>
#include <chess.hpp>

namespace engine::piecevalues {

    inline constexpr std::array<int, 6> PieceValue = {
        100,  // PAWN
        320,  // KNIGHT
        330,  // BISHOP
        500,  // ROOK
        900,  // QUEEN
        0     // KING (never captured)
    };

}