#pragma once

#include <chess.hpp>

namespace engine {

/// Static evaluation of a position, in centipawns, from the point of view of
/// the side to move (positive = good for the side to move).
///
/// TODO: implement evaluation terms (material, piece-square tables, ...).
int evaluate(const chess::Board& board);

} // namespace engine
