#pragma once

#include <chess.hpp>
#include "scoresystems/bonuspoints.hpp"

namespace engine {

/// evaluate()'s terms, broken out for debugging. Both are already signed from
/// the side-to-move's point of view, same as evaluate()'s return value, and
/// evaluate() == breakdown.material + breakdown.mobility.
struct EvalBreakdown {
    int material = 0;
    int mobility = 0;
};

/// Static evaluation of a position, in centipawns, from the point of view of
/// the side to move (positive = good for the side to move).
///
/// If `breakdown` is non-null, it's filled with this call's terms.
///
/// TODO: implement more evaluation terms (piece-square tables, ...).
int evaluate(const chess::Board& board, EvalBreakdown* breakdown = nullptr);
bool hasCastled(const chess::Board& board, chess::Color color);

} // namespace engine
