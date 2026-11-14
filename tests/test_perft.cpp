#include <doctest/doctest.h>

#include <chess.hpp>

TEST_CASE("placeholder: start position loads") {
    const chess::Board board(chess::constants::STARTPOS);
    CHECK(board.getFen() == chess::constants::STARTPOS);
}

// TODO: perft verification.
//   Count leaf nodes of the legal move tree to a fixed depth and compare with
//   known values, e.g. start position: d1 = 20, d2 = 400, d3 = 8'902, d4 = 197'281,
//   plus "Kiwipete" and the other standard perft positions
//   (https://www.chessprogramming.org/Perft_Results).
