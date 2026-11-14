#pragma once
// Turns a predict_move request into the "resulting FEN" reply, using engine_core.
// No networking here, so it can be unit-tested directly.

#include <optional>
#include <string>
#include <string_view>

#include <chess.hpp>

#include "engine/time_manager.hpp"

namespace bot {

/// Maps a mode name (as advertised at registration) to search limits.
/// Modes are opaque to the backend; this is where we decide what they mean.
/// TODO: tune these once the real search exists.
engine::TimeLimit timeLimitForMode(std::string_view mode);

/// Loads a FEN, rejecting malformed positions (bad syntax, missing kings).
bool loadFen(chess::Board& board, std::string_view fen, std::string& error);

/// Applies exactly one legal move to a copy of `root` and returns the full
/// six-field FEN of the resulting position. std::nullopt if the move is illegal.
std::optional<std::string> resultingFen(const chess::Board& root, chess::Move move);

/// Full pipeline: FEN + mode -> engine search -> resulting FEN.
/// Returns std::nullopt and fills `error` if the FEN is invalid or there is no
/// legal move (the caller must then disconnect, not invent a reply).
std::optional<std::string> answerPrediction(std::string_view fen,
                                            std::string_view mode,
                                            std::string& error);

} // namespace bot
