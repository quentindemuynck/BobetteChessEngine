#pragma once

#include <optional>

#include <chess.hpp>

namespace engine {

/// Looks up `board`'s position in the hardcoded opening book (keyed by its
/// Zobrist hash, so transpositions are found too). Returns std::nullopt if
/// the position isn't in the book, in which case the caller should fall back
/// to the real search.
///
/// When a position has more than one book move, one is picked at random each
/// call, so the engine doesn't play the identical opening every game.
std::optional<chess::Move> lookupBookMove(const chess::Board& board);

} // namespace engine
